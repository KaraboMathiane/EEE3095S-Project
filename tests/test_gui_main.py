"""Headless GUI behaviour tests: no COM port, real threads or Tk window needed."""

import queue
from types import SimpleNamespace
from unittest.mock import MagicMock, Mock, call

import pytest

import main


@pytest.fixture
def app():
    # Bypass widget construction, keeping real application methods and queue.
    instance = main.App.__new__(main.App)
    instance.root = Mock()
    instance.client = Mock()
    instance.events = queue.Queue()
    instance.connected = instance.playing = instance.busy = instance.closing = False
    instance.board = "---------"
    instance.ai_mode = False
    instance.status = Mock()
    instance.port = MagicMock()
    instance.port.get.return_value = "COM5"
    instance.cells = [Mock() for _ in range(9)]
    for name in ("connect_button", "refresh_button", "new_button", "ai_button",
                 "resign_button", "disconnect_button", "log"):
        setattr(instance, name, Mock())
    return instance


def drain(app):
    events = []
    while not app.events.empty():
        events.append(app.events.get_nowait())
    return events


def test_window_initialisation_and_cell_callbacks(monkeypatch):
    # Build with fake widgets: each cell must capture its own index, not index 8.
    for name in ("Frame", "Label", "Combobox", "Button"):
        monkeypatch.setattr(main.ttk, name, MagicMock())
    cells = MagicMock()
    monkeypatch.setattr(main.tk, "Button", cells)
    monkeypatch.setattr(main.tk, "Text", MagicMock())
    monkeypatch.setattr(main.tk, "StringVar", MagicMock())
    monkeypatch.setattr(main.list_ports, "comports", lambda: [])
    root = Mock()

    instance = main.App(root)
    assert not any((instance.connected, instance.playing, instance.busy, instance.closing))
    assert instance.board == "---------" and len(instance.cells) == 9
    root.protocol.assert_called_once_with("WM_DELETE_WINDOW", instance.quit)
    root.after.assert_called_once_with(50, instance.poll)

    instance.send = Mock()
    for construction in cells.call_args_list:
        construction.kwargs["command"]()
    assert instance.send.call_args_list == [call(f"MOVE:{i}") for i in range(9)]


def test_send_forwards_command_to_scheduler(app):
    # Button callbacks share the same scheduling path.
    app.start = Mock()
    app.send("RESIGN")
    app.start.assert_called_once_with("RESIGN")


@pytest.mark.parametrize("command,reply,board", [
    ("CONNECT", "OK", None),
    ("NEW", "OK:BOARD:---------", "---------"),
    ("NEW:AI", "OK:BOARD:---------", "---------"),
    ("MOVE:4", "OK:BOARD:----X----", "----X----"),
    ("RESIGN", "OK:GAMEOVER", None),
    ("DISCONNECT", "OK", None),
    ("MOVE:4", "NACK:OCCUPIED", None),
])
def test_exchange_accepts_expected_replies(app, command, reply, board):
    # Accepted exchanges publish data, not direct widget updates.
    app.client.open.return_value = reply
    app.client.request.return_value = reply
    app.exchange(command, "COM5")
    assert drain(app) == [
        ("log", f"> {command}\n< {reply}"),
        ("reply", (command, reply, board)),
    ]
    app.status.set.assert_not_called()
    if command == "CONNECT":
        app.client.open.assert_called_once_with("COM5")
        app.client.request.assert_not_called()
    else:
        app.client.request.assert_called_once_with(command)
    assert app.client.close.call_count == int(command == "DISCONNECT")


@pytest.mark.parametrize("result,board", [
    ("WIN:X", "XXXOO----"), ("WIN:O", "XX-OOOX--"), ("DRAW", "XOXXOOOXX"),
])
def test_terminal_move_fetches_final_board(app, result, board):
    # Terminal tokens lack cells: STATE must supply the final display.
    app.client.request.side_effect = [result, "BOARD:" + board]
    app.exchange("MOVE:2", None)
    assert app.client.request.call_args_list == [call("MOVE:2"), call("STATE")]
    assert drain(app)[-1] == ("reply", ("MOVE:2", result, board))


@pytest.mark.parametrize("command,reply", [
    ("NEW", "OK"), ("NEW", "BOARD:---------"),
    ("MOVE:4", "OK"), ("RESIGN", "WIN:X"),
    ("DISCONNECT", "BOARD:---------"), ("NEW", "OK:BOARD:bad"),
])
def test_unexpected_replies_close_connection(app, command, reply):
    # Well-framed but command-inappropriate or corrupt replies cannot update state.
    app.client.request.return_value = reply
    app.exchange(command, None)
    events = drain(app)
    assert events[-1][0] == "error"
    assert not any(kind == "reply" for kind, _ in events)
    app.client.close.assert_called_once_with()


@pytest.mark.parametrize("state_reply", ["OK", "NACK:NOGAME", "OK:BOARD:XXXOO----", "BOARD:bad"])
def test_terminal_state_failure_does_not_guess_board(app, state_reply):
    # A failed final-state fetch is an error, never an optimistic board update.
    app.client.request.side_effect = ["WIN:X", state_reply]
    app.exchange("MOVE:2", None)
    assert drain(app)[-1][0] == "error"
    app.client.close.assert_called_once_with()


@pytest.mark.parametrize("command", ["CONNECT", "MOVE:4"])
def test_exchange_reports_io_failure(app, command):
    # Exceptions are delivered to the UI queue and the serial handle is released.
    app.client.open.side_effect = OSError("unplugged")
    app.client.request.side_effect = OSError("unplugged")
    app.exchange(command, "COM5")
    assert drain(app) == [("error", "unplugged")]
    app.client.close.assert_called_once_with()


@pytest.mark.parametrize("board,turn", [("---------", "X"), ("----X----", "O"), ("O---X----", "X")])
def test_confirmed_board_updates_turn_hint(app, board, turn):
    # The displayed turn is derived from confirmed cells, not the clicked index.
    app.apply_reply("NEW", "OK:BOARD:" + board, board)
    assert app.board == board and app.playing
    app.status.set.assert_called_with(f"{turn} to move.")


@pytest.mark.parametrize("code", list(main.ERRORS) + ["FUTURE_ERROR"])
def test_rejection_preserves_board_and_shows_reason(app, code):
    # Rejected moves cannot change cells; state errors also disable further play.
    app.board = "----X----"
    app.playing = True
    app.apply_reply("MOVE:4", "NACK:" + code, None)
    assert app.board == "----X----"
    assert app.playing == (code not in ("NOGAME", "ILLEGAL"))
    app.status.set.assert_called_with(
        main.ERRORS.get(code, "Server rejected the action: " + code)
    )


@pytest.mark.parametrize("reply,message", [
    ("WIN:X", "X wins!"), ("WIN:O", "O wins!"),
    ("DRAW", "Draw!"), ("OK:GAMEOVER", "Game ended by resignation."),
])
def test_game_end_disables_moves(app, reply, message):
    # Win, draw and resignation keep their outcome visible and stop move input.
    app.playing = True
    app.apply_reply("MOVE:2", reply, None)
    assert not app.playing
    assert app.status.set.call_args.args[0].startswith(message)


def test_ai_mode_starts_and_switches_back(app):
    # Only a successful AI new-game reply enables the AI status display.
    app.apply_reply("NEW:AI", "NACK:NOGAME", None)
    assert not app.ai_mode
    app.apply_reply("NEW:AI", "OK:BOARD:---------", "---------")
    assert app.ai_mode and app.playing
    app.status.set.assert_called_with("Your turn (X). STM32 plays O.")
    app.apply_reply("MOVE:0", "OK:BOARD:X---O----", "X---O----")
    assert app.board == "X---O----"
    app.apply_reply("NEW", "OK:BOARD:---------", "---------")
    assert not app.ai_mode


def test_connection_requires_new_game_and_disconnect_clears_flags(app):
    # Connecting does not silently resume an unknown game.
    app.apply_reply("CONNECT", "OK", None)
    assert app.connected and not app.playing
    app.apply_reply("DISCONNECT", "OK", None)
    assert not app.connected and not app.playing


@pytest.mark.parametrize("connected,playing,busy", [
    (False, False, False), (False, False, True), (True, False, False),
    (True, True, False), (True, True, True),
])
def test_control_states_and_confirmed_cell_rendering(app, connected, playing, busy):
    # Busy/disconnected/finished games block moves; occupied cells remain testable.
    app.connected, app.playing, app.busy = connected, playing, busy
    app.board = "XO-------"
    app.controls()
    available = connected and not busy
    app.new_button.configure.assert_called_with(state="normal" if available else "disabled")
    app.disconnect_button.configure.assert_called_with(state="normal" if available else "disabled")
    app.resign_button.configure.assert_called_with(state="normal" if available and playing else "disabled")
    app.connect_button.configure.assert_called_with(state="normal" if not connected and not busy else "disabled")
    for cell, text in zip(app.cells, ["X", "O"] + [""] * 7):
        cell.configure.assert_called_with(text=text, state="normal" if available and playing else "disabled")


def test_start_schedules_worker_without_changing_board(app, monkeypatch):
    # Clicking sends work to a thread; no speculative mark appears while waiting.
    thread = Mock()
    monkeypatch.setattr(main.threading, "Thread", thread)
    app.start("MOVE:4")
    assert app.busy and app.board == "---------"
    thread.assert_called_once_with(target=app.exchange, args=("MOVE:4", None), daemon=True)
    thread.return_value.start.assert_called_once_with()
    app.start("MOVE:0")
    assert thread.call_count == 1  # No overlapping requests.


@pytest.mark.parametrize("name", ["", "   ", " COM5 "])
def test_connect_validates_port_selection(app, name):
    # Empty selection stays local; valid input is trimmed before opening.
    app.port.get.return_value = name
    app.start = Mock()
    app.connect()
    if name.strip():
        app.start.assert_called_once_with("CONNECT", "COM5")
    else:
        app.start.assert_not_called()


@pytest.mark.parametrize("current,names", [("", []), ("", ["COM5", "COM6"]), ("COM9", ["COM5"])])
def test_refresh_preserves_manual_port(app, monkeypatch, current, names):
    # Port discovery selects a default only when the user has not entered one.
    app.port.get.return_value = current
    monkeypatch.setattr(main.list_ports, "comports", lambda: [SimpleNamespace(device=n) for n in names])
    app.refresh()
    app.port.__setitem__.assert_called_once_with("values", names)
    if not current and names:
        app.port.set.assert_called_once_with(names[0])
    else:
        app.port.set.assert_not_called()


def test_poll_log_does_not_complete_pending_request(app):
    # Diagnostic traffic must not re-enable controls before a reply/error arrives.
    app.busy = True
    app.log_line = Mock()
    app.events.put(("log", "> NEW"))
    app.poll()
    assert app.busy
    app.log_line.assert_called_once_with("> NEW")
    app.root.after.assert_called_once_with(50, app.poll)


def test_poll_applies_reply_and_recovers_from_error(app):
    # Queue results reach real UI state handling, and errors disable the session.
    app.busy = True
    app.events.put(("reply", ("CONNECT", "OK", None)))
    app.poll()
    assert app.connected and not app.busy
    app.log_line = Mock()
    app.events.put(("error", "timeout"))
    app.poll()
    assert not app.connected and not app.playing and not app.busy
    app.status.set.assert_called_with("Connection closed: timeout")


@pytest.mark.parametrize("connected,busy", [(True, True), (True, False), (False, False)])
def test_shutdown_waits_then_disconnects_or_destroys(app, connected, busy):
    # Closing never interrupts pending serial I/O; an idle connection is ended first.
    app.connected, app.busy = connected, busy
    app.send = Mock()
    app.quit()
    assert app.closing
    app.poll()
    if busy:
        app.send.assert_not_called()
        app.root.destroy.assert_not_called()
    elif connected:
        app.send.assert_called_once_with("DISCONNECT")
        app.root.destroy.assert_not_called()
    else:
        app.root.destroy.assert_called_once_with()
        app.root.after.assert_not_called()


@pytest.mark.parametrize("line_count", [10, 201])
def test_log_is_read_only_and_trims_old_output(app, line_count):
    # Logging restores read-only mode and bounds retained traffic.
    app.log.index.return_value = f"{line_count}.0"
    app.log_line("< OK")
    assert app.log.configure.call_args_list == [call(state="normal"), call(state="disabled")]
    app.log.insert.assert_called_once_with("end", "< OK\n")
    app.log.see.assert_called_once_with("end")
    assert app.log.delete.call_count == int(line_count > 200)
