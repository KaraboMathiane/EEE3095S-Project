"""Client boundary tests; requires gui/requirements.txt as well as pytest."""

from unittest.mock import Mock, call

import pytest

import serial
import client


@pytest.mark.parametrize("reply", ["BOARD:---------", "OK:BOARD:X---O----"])
def test_valid_board(reply):
    # Both board prefixes decode to exactly the server's cells.
    assert client.board_from_reply(reply) == reply.split(":")[-1]


@pytest.mark.parametrize("reply", [
    "BOARD:--------", "BOARD:.........", "BOARD:---------junk",
    "BOARD:", "OK:BOARD:xxxxxxxxx", "BOARD:--------\n",
])
def test_invalid_board(reply):
    # Bad lengths and unsupported symbols must never become a displayed board.
    with pytest.raises(client.ProtocolError):
        client.board_from_reply(reply)


class Port:
    def __init__(self, response):
        self.response = response
        self.sent = None

    def write(self, data):
        self.sent = data

    def read_until(self, terminator, size):
        assert terminator == b"\n" and size == 65
        return self.response


def test_request_framing():
    # Commands gain one LF; CRLF replies are returned without either terminator.
    transport = client.SerialClient()
    transport.port = Port(b"OK\r\n")
    assert transport.request("CONNECT") == "OK"
    assert transport.port.sent == b"CONNECT\n"


@pytest.mark.parametrize("response", [
    b"", b"OK", b"\xff\n", b"OK\x00\n", b"\n", b"\r\n",
    b"OK\t\n", b"OK\x7f\n", b"O\rK\n", b"x" * 65,
])
def test_rejects_incomplete_or_corrupt_reply(response):
    # Timeouts, oversized unterminated frames and nonprintable data are errors.
    transport = client.SerialClient()
    transport.port = Port(response)
    with pytest.raises(client.ProtocolError):
        transport.request("CONNECT")


@pytest.mark.parametrize("reply", ["OK", "WIN:X", "DRAW", "NACK:OCCUPIED", ""])
def test_non_board_replies_have_no_board(reply):
    # Result/error tokens are not board updates.
    assert client.board_from_reply(reply) is None


@pytest.mark.parametrize("reply", [b"OK\n", b"NACK:OCCUPIED\n", b"BOARD:X---O----\n"])
def test_request_accepts_printable_lf_replies(reply):
    # Transport preserves response content without interpreting game rules.
    transport = client.SerialClient()
    transport.port = Port(reply)
    assert transport.request("STATE") == reply[:-1].decode("ascii")


def test_request_requires_an_open_port():
    # A disconnected client fails explicitly rather than accessing a missing handle.
    with pytest.raises(client.ProtocolError, match="No serial connection"):
        client.SerialClient().request("NEW")


def test_open_configures_port_and_handshakes(monkeypatch):
    # Correct electrical framing, bounded waits and buffer clearing precede CONNECT.
    port = Mock()
    port.read_until.return_value = b"OK\n"
    constructor = Mock(return_value=port)
    monkeypatch.setattr(client.serial, "Serial", constructor)
    transport = client.SerialClient()

    assert transport.open("COM5") == "OK"
    constructor.assert_called_once_with(
        "COM5", baudrate=115200, timeout=2, write_timeout=2,
        bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
    )
    assert port.mock_calls == [
        call.reset_input_buffer(), call.write(b"CONNECT\n"),
        call.read_until(b"\n", size=65),
    ]


def test_open_rejects_failed_handshake(monkeypatch):
    # Opening an OS port alone does not establish an application session.
    port = Mock()
    port.read_until.return_value = b"NACK:BAD\n"
    monkeypatch.setattr(client.serial, "Serial", Mock(return_value=port))
    with pytest.raises(client.ProtocolError, match="Connection rejected"):
        client.SerialClient().open("COM5")


@pytest.mark.parametrize("operation", ["write", "read_until"])
def test_serial_failures_reach_caller(operation):
    # The UI worker must be able to detect unplugged cables and write failures.
    transport = client.SerialClient()
    transport.port = Mock()
    getattr(transport.port, operation).side_effect = serial.SerialException("unplugged")
    with pytest.raises(serial.SerialException, match="unplugged"):
        transport.request("STATE")


def test_close_releases_handle_only_once():
    # Repeated cleanup is harmless and cannot reuse a closed handle.
    transport = client.SerialClient()
    port = Mock()
    transport.port = port
    transport.close()
    transport.close()
    port.close.assert_called_once_with()
    assert transport.port is None
