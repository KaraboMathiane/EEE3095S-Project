"""Client boundary tests; requires gui/requirements.txt as well as pytest."""

import importlib.util
import pathlib

import pytest

pytest.importorskip("serial")
path = pathlib.Path(__file__).resolve().parents[1] / "gui" / "client.py"
spec = importlib.util.spec_from_file_location("gui_client", path)
client = importlib.util.module_from_spec(spec)
spec.loader.exec_module(client)


@pytest.mark.parametrize("reply", ["BOARD:---------", "OK:BOARD:X---O----"])
def test_valid_board(reply):
    assert client.board_from_reply(reply) == reply.split(":")[-1]


@pytest.mark.parametrize("reply", ["BOARD:--------", "BOARD:.........", "BOARD:---------junk"])
def test_invalid_board(reply):
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
    transport = client.SerialClient()
    transport.port = Port(b"OK\r\n")
    assert transport.request("CONNECT") == "OK"
    assert transport.port.sent == b"CONNECT\n"


@pytest.mark.parametrize("response", [b"", b"OK", b"\xff\n", b"OK\x00\n"])
def test_rejects_incomplete_or_corrupt_reply(response):
    transport = client.SerialClient()
    transport.port = Port(response)
    with pytest.raises(client.ProtocolError):
        transport.request("CONNECT")
