"""Serial transport and board decoding, independent of the GUI widgets.

One request writes an ASCII command plus LF, then waits for one reply line.
These calls block while waiting, so main.py runs them in a worker thread.
The STM32 implements the rules; this module only checks message formatting.
"""

import serial


class ProtocolError(Exception):
    """A reply is missing, malformed, or unexpected for the current exchange."""

    pass


def board_from_reply(reply):
    """Return nine cells from a board reply, or None for other reply types.

    NEW/MOVE use OK:BOARD:, while STATE uses BOARD:. A board-shaped reply
    with invalid cells raises an error instead of changing the display.
    Indices 0..8 run left to right, top to bottom; '-' means empty.
    """
    for prefix in ("OK:BOARD:", "BOARD:"):
        if reply.startswith(prefix):
            board = reply[len(prefix):]
            if len(board) == 9 and all(cell in "-XO" for cell in board):
                return board

            raise ProtocolError("Invalid board received from server.")

    return None


class SerialClient:
    """Own one pyserial connection. The caller must serialise all requests."""

    def __init__(self):
        self.port = None

    def open(self, name):
        """Open a port such as COM3 and perform the CONNECT/OK handshake.

        Matching baud rate and 8N1 settings are required on the STM32.
        The GUI's exchange() closes the port if opening/handshaking fails.
        """
        self.port = serial.Serial(
            name,
            baudrate=115200,
            timeout=2,
            write_timeout=2,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
        )

        # Discard stale received bytes before starting the new handshake.
        self.port.reset_input_buffer()
        reply = self.request("CONNECT")

        if reply != "OK":
            raise ProtocolError("Connection rejected: " + reply)

        return reply

    def request(self, command):
        """Send one command and return a printable reply without its terminator.

        Read/write timeouts are two seconds. The read is capped at 65 bytes
        so garbage cannot create an unbounded reply. Command-specific reply
        validation happens in App.exchange(), rather than in this transport.
        """
        if self.port is None:
            raise ProtocolError("No serial connection.")

        # pyserial exchanges bytes; the application works with Python strings.
        self.port.write((command + "\n").encode("ascii"))
        raw = self.port.read_until(b"\n", size=65)

        if not raw.endswith(b"\n"):
            # Continuing could mistake a late reply for the next command's reply.
            raise ProtocolError("Incomplete reply or timeout. Reconnect before continuing.")

        try:
            # Accept both LF and CRLF, but preserve other bytes for validation.
            reply = raw[:-1].removesuffix(b"\r").decode("ascii")
        except UnicodeDecodeError as exc:
            raise ProtocolError("Non-ASCII reply received.") from exc

        if not reply or any(ord(c) < 32 or ord(c) > 126 for c in reply):
            raise ProtocolError("Invalid reply received.")

        return reply

    def close(self):
        """Release the OS serial-port handle; safe when already disconnected."""
        if self.port is not None:
            self.port.close()
            self.port = None
