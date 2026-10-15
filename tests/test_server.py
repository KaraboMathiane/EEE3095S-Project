import os
import pathlib
import subprocess

import pytest


ROOT = pathlib.Path(__file__).resolve().parents[1]


@pytest.fixture(scope="session")
def server_binary(tmp_path_factory):
    build_dir = tmp_path_factory.mktemp("server-build")
    executable = build_dir / "host_server"
    compiler = os.environ.get("CC", "gcc")
    command = [
        compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        "-I",
        str(ROOT / "Game_Server" / "Inc"),
        str(ROOT / "Game_Server" / "Src" / "game.c"),
        str(ROOT / "Game_Server" / "Src" / "server_protocol.c"),
        str(ROOT / "tests" / "host_server.c"),
        "-o",
        str(executable),
    ]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    assert result.returncode == 0, result.stderr
    return executable


def run_server(server_binary, commands):
    result = subprocess.run(
        [str(server_binary)],
        input="\n".join(commands) + "\n",
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    return result.stdout.splitlines()


def test_connect_new_and_state(server_binary):
    assert run_server(server_binary, ["CONNECT", "NEW", "STATE"]) == [
        "OK",
        "OK:BOARD:---------",
        "BOARD:---------",
    ]


@pytest.mark.parametrize(
    "moves",
    [
        ["MOVE:0", "MOVE:3", "MOVE:1", "MOVE:4", "MOVE:2"],
        ["MOVE:3", "MOVE:0", "MOVE:4", "MOVE:1", "MOVE:5"],
        ["MOVE:6", "MOVE:0", "MOVE:7", "MOVE:1", "MOVE:8"],
        ["MOVE:0", "MOVE:1", "MOVE:3", "MOVE:2", "MOVE:6"],
        ["MOVE:1", "MOVE:0", "MOVE:4", "MOVE:2", "MOVE:7"],
        ["MOVE:2", "MOVE:0", "MOVE:5", "MOVE:1", "MOVE:8"],
        ["MOVE:0", "MOVE:1", "MOVE:4", "MOVE:2", "MOVE:8"],
        ["MOVE:2", "MOVE:0", "MOVE:4", "MOVE:1", "MOVE:6"],
    ],
)
def test_detects_each_x_winning_line(server_binary, moves):
    responses = run_server(server_binary, ["CONNECT", "NEW", *moves])
    assert responses[0:2] == ["OK", "OK:BOARD:---------"]
    assert responses[-1] == "WIN:X"


def test_detects_o_win(server_binary):
    assert run_server(
        server_binary,
        [
            "CONNECT",
            "NEW",
            "MOVE:0",
            "MOVE:3",
            "MOVE:1",
            "MOVE:4",
            "MOVE:8",
            "MOVE:5",
        ],
    ) == [
        "OK",
        "OK:BOARD:---------",
        "OK:BOARD:X--------",
        "OK:BOARD:X--O-----",
        "OK:BOARD:XX-O-----",
        "OK:BOARD:XX-OO----",
        "OK:BOARD:XX-OO---X",
        "WIN:O",
    ]


def test_detects_draw(server_binary):
    assert run_server(
        server_binary,
        [
            "CONNECT",
            "NEW",
            "MOVE:0",
            "MOVE:1",
            "MOVE:2",
            "MOVE:4",
            "MOVE:3",
            "MOVE:5",
            "MOVE:7",
            "MOVE:6",
            "MOVE:8",
        ],
    )[-1] == "DRAW"


def test_rejects_invalid_moves_without_changing_board(server_binary):
    assert run_server(
        server_binary,
        [
            "CONNECT",
            "NEW",
            "MOVE:0",
            "MOVE:0",
            "MOVE:9",
            "MOVE:-1",
            "MOVE:",
            "MOVE:1x",
            "UNKNOWN",
            "STATE",
        ],
    ) == [
        "OK",
        "OK:BOARD:---------",
        "OK:BOARD:X--------",
        "NACK:OCCUPIED",
        "NACK:RANGE",
        "NACK:BAD",
        "NACK:BAD",
        "NACK:BAD",
        "NACK:BAD",
        "BOARD:X--------",
    ]


def test_rejects_commands_without_a_game_and_after_disconnect(server_binary):
    assert run_server(
        server_binary,
        ["NEW", "CONNECT", "STATE", "MOVE:0", "NEW", "DISCONNECT", "STATE"],
    ) == [
        "NACK:NOGAME",
        "OK",
        "NACK:NOGAME",
        "NACK:NOGAME",
        "OK:BOARD:---------",
        "OK",
        "NACK:NOGAME",
    ]


def test_resign_and_moves_after_game_over(server_binary):
    assert run_server(
        server_binary,
        ["CONNECT", "NEW", "MOVE:4", "RESIGN", "MOVE:0", "STATE"],
    ) == [
        "OK",
        "OK:BOARD:---------",
        "OK:BOARD:----X----",
        "OK:GAMEOVER",
        "NACK:ILLEGAL",
        "BOARD:----X----",
    ]


def test_rejects_overlong_frames_and_large_indices(server_binary):
    assert run_server(
        server_binary,
        ["CONNECT", "NEW", "MOVE:999999999999999999", "x" * 33, "STATE"],
    ) == [
        "OK",
        "OK:BOARD:---------",
        "NACK:RANGE",
        "NACK:BAD",
        "BOARD:---------",
    ]


def test_rejects_non_ascii_command_bytes(server_binary):
    assert run_server(
        server_binary,
        ["CONNECT\x00junk", "CONNECT", "NEW", "STATE"],
    ) == [
        "NACK:BAD",
        "OK",
        "OK:BOARD:---------",
        "BOARD:---------",
    ]
