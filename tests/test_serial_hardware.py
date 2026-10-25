"""Opt-in tests of the flashed STM32 through its physical serial interface.

Close the GUI/terminal first, then run:
    python -m pytest tests/test_serial_hardware.py --serial-port COM5 -v

These tests replace the current game. They do not flash firmware or check LEDs.
Raw pyserial is used independently of the GUI transport to test invalid frames.
"""

import time

import pytest
import serial


class BoardLink:
    """Send raw bytes and assert bounded, complete ASCII replies."""

    def __init__(self, port):
        self.port = port

    def write(self, data):
        assert self.port.write(data) == len(data), "Incomplete serial write"
        self.port.flush()

    def read(self):
        raw = self.port.read_until(b"\n", size=65)
        assert raw.endswith(b"\n"), f"Missing/incomplete STM32 response: {raw!r}"
        return raw[:-1].removesuffix(b"\r").decode("ascii")

    def expect(self, command, response):
        self.write(command.encode("ascii") + b"\n")
        actual = self.read()
        assert actual == response, f"{command!r}: expected {response!r}, got {actual!r}"


@pytest.fixture(scope="module")
def uart(request):
    # Without explicit opt-in, never open a port or disturb a running game.
    name = request.config.getoption("--serial-port")
    if not name:
        pytest.skip("Physical STM32 test: supply --serial-port COM5")

    try:
        port = serial.Serial(name, baudrate=115200, timeout=3, write_timeout=2)
    except serial.SerialException as exc:
        pytest.fail(f"Cannot open {name}; close GUI/terminal and check the cable: {exc}")

    try:
        # Allow adapters that reset the target on open to settle. Discard only
        # before initial synchronisation, never between tested command/reply pairs.
        time.sleep(1.2)
        port.reset_input_buffer()
        link = BoardLink(port)
        # A newline terminates any stale partial input; its reply is consumed.
        link.write(b"\n")
        assert link.read() == "NACK:BAD"
        yield link
    finally:
        # Always release COM5, even if an assertion fails.
        port.close()


@pytest.fixture
def board(uart):
    # Isolate cases by discarding the previous session, not by trusting boot state.
    uart.expect("DISCONNECT", "OK")
    uart.expect("CONNECT", "OK")
    uart.expect("NEW", "OK:BOARD:---------")
    yield uart
    # Normal and failed tests both attempt to leave the board disconnected.
    uart.expect("DISCONNECT", "OK")


def test_connect_new_and_state(board):
    # The fixture checks handshake/new-game replies; STATE confirms the empty board.
    board.expect("STATE", "BOARD:---------")


def test_legal_moves_and_occupied_rejection(board):
    # Rejected moves preserve both cells and turn: the next accepted mark is O.
    board.expect("MOVE:4", "OK:BOARD:----X----")
    board.expect("MOVE:4", "NACK:OCCUPIED")
    board.expect("STATE", "BOARD:----X----")
    board.expect("MOVE:0", "OK:BOARD:O---X----")


@pytest.mark.parametrize("command,reply", [
    ("MOVE:9", "NACK:RANGE"),
    ("MOVE:999999999999999999", "NACK:RANGE"),
    ("MOVE:-1", "NACK:BAD"), ("MOVE:", "NACK:BAD"),
    ("MOVE:1x", "NACK:BAD"), ("UNKNOWN", "NACK:BAD"),
])
def test_invalid_command_preserves_board_and_turn(board, command, reply):
    # Error classification must be specific and must not consume X's turn.
    board.expect(command, reply)
    board.expect("STATE", "BOARD:---------")
    board.expect("MOVE:4", "OK:BOARD:----X----")


@pytest.mark.parametrize("moves,result", [
    ([0, 3, 1, 4, 2], "WIN:X"),
    ([3, 0, 4, 1, 5], "WIN:X"),
    ([6, 0, 7, 1, 8], "WIN:X"),
    ([0, 1, 3, 2, 6], "WIN:X"),
    ([1, 0, 4, 2, 7], "WIN:X"),
    ([2, 0, 5, 1, 8], "WIN:X"),
    ([0, 1, 4, 2, 8], "WIN:X"),
    ([2, 0, 4, 1, 6], "WIN:X"),
    ([0, 3, 1, 4, 8, 5], "WIN:O"),
    ([0, 1, 2, 4, 3, 5, 7, 6, 8], "DRAW"),
], ids=["x-top", "x-middle", "x-bottom", "x-left", "x-centre", "x-right",
        "x-diagonal", "x-antidiagonal", "o-win", "draw"])
def test_terminal_sequences_and_restart(board, moves, result):
    # Exercise every X win line, an O win and a draw on actual firmware.
    cells = list("---------")
    for turn, index in enumerate(moves):
        cells[index] = "X" if turn % 2 == 0 else "O"
        expected = result if turn == len(moves) - 1 else "OK:BOARD:" + "".join(cells)
        board.expect(f"MOVE:{index}", expected)

    # Terminal boards remain readable and cannot be changed by further moves.
    final = "BOARD:" + "".join(cells)
    board.expect("STATE", final)
    board.expect("MOVE:0", "NACK:ILLEGAL")
    board.expect("STATE", final)
    board.expect("NEW", "OK:BOARD:---------")
    board.expect("MOVE:4", "OK:BOARD:----X----")


def test_resignation_and_disconnection(board):
    # Resignation preserves cells; disconnect removes access to the old game.
    board.expect("MOVE:4", "OK:BOARD:----X----")
    board.expect("RESIGN", "OK:GAMEOVER")
    board.expect("MOVE:0", "NACK:ILLEGAL")
    board.expect("STATE", "BOARD:----X----")
    board.expect("DISCONNECT", "OK")
    for command in ("NEW", "STATE", "MOVE:0", "RESIGN"):
        board.expect(command, "NACK:NOGAME")
    board.expect("CONNECT", "OK")
    board.expect("STATE", "NACK:NOGAME")
    board.expect("NEW", "OK:BOARD:---------")


@pytest.mark.parametrize("frame", [
    b"x" * 33 + b"\n", b"CONNECT\x00junk\n", b"MOVE:\xff\n", b"MO\rVE:4\n",
], ids=["oversized", "embedded-nul", "non-ascii", "embedded-cr"])
def test_garbage_frame_recovers_at_next_command(board, frame):
    # Malformed frames are rejected once; the following valid command still works.
    board.write(frame)
    assert board.read() == "NACK:BAD"
    board.expect("STATE", "BOARD:---------")
    board.expect("MOVE:4", "OK:BOARD:----X----")


def test_crlf_and_split_frame(board):
    # CRLF is accepted and a short inter-byte pause must not terminate a frame.
    board.write(b"STA")
    time.sleep(0.05)
    board.write(b"TE\r\n")
    assert board.read() == "BOARD:---------"


def test_partial_frame_times_out_and_recovers(board):
    # No newline is sent: firmware must time out, NACK, and clear partial bytes.
    board.write(b"MOVE:")
    started = time.monotonic()
    assert board.read() == "NACK:BAD"
    elapsed = time.monotonic() - started
    assert 0.7 <= elapsed < 2.5, f"Unexpected inter-byte timeout: {elapsed:.3f}s"
    board.expect("STATE", "BOARD:---------")
    board.expect("MOVE:4", "OK:BOARD:----X----")
