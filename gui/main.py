"""Thin Tic-Tac-Toe client. Run with: python gui/main.py.

    cell callback -> send -> start -> exchange (worker thread)
    -> events queue -> poll (Tk thread) -> apply_reply -> controls

Only the STM32 decides whether a move is legal or a game has ended.
The GUI retains confirmed board data and local UI/session flags.
"""

import queue
import threading
import tkinter as tk
from tkinter import ttk

from serial.tools import list_ports

from client import ProtocolError, SerialClient, board_from_reply

# ── Colour palette ─────────────────────────────────────────────────────────
BG        = "#0a0f2c"   # dark navy background
BG_PANEL  = "#0d1540"   # slightly lighter panel surface
NEON_BLUE = "#00cfff"   # neon cyan-blue for X marks and accents
NEON_RED  = "#ff2d55"   # neon red for O marks and warnings
WHITE     = "#ffffff"
FG_DIM    = "#7a9abf"   # muted text for labels and log

def _apply_theme(root: tk.Tk) -> None:
    """Configure ttk styles and root background for the dark/neon theme."""
    root.configure(bg=BG)

    style = ttk.Style(root)
    # Use a base theme that exposes enough options; 'clam' works on all platforms.
    style.theme_use("clam")

    style.configure(".",
        background=BG, foreground=WHITE,
        fieldbackground=BG_PANEL, bordercolor=NEON_BLUE,
        troughcolor=BG_PANEL, selectbackground=NEON_BLUE,
        selectforeground=BG, font=("Segoe UI", 10))

    style.configure("TFrame",  background=BG)
    style.configure("TLabel",  background=BG, foreground=WHITE)

    style.configure("TButton",
        background=BG_PANEL, foreground=NEON_BLUE,
        bordercolor=NEON_BLUE, focuscolor=NEON_BLUE,
        padding=(8, 4))
    style.map("TButton",
        background=[("active", NEON_BLUE), ("disabled", BG_PANEL)],
        foreground=[("active", BG),        ("disabled", FG_DIM)])

    style.configure("TCombobox",
        fieldbackground=BG_PANEL, background=BG_PANEL,
        foreground=WHITE, arrowcolor=NEON_BLUE,
        bordercolor=NEON_BLUE, selectbackground=NEON_BLUE)
    style.map("TCombobox",
        fieldbackground=[("readonly", BG_PANEL)],
        foreground=[("disabled", FG_DIM)])


# Translate protocol tokens into messages. BAD/BADCMD support both the current
# firmware spelling and the assignment reference spelling.
ERRORS = {
    "OCCUPIED": "That cell is occupied. Choose another cell.",
    "RANGE": "Cell index is outside the board.",
    "NOTURN": "The server rejected the move: not your turn.",
    "ILLEGAL": "The action is not allowed in the current game state.",
    "NOGAME": "No game is available. Select New game.",
    "BAD": "The server rejected a malformed command.",
    "BADCMD": "The server rejected a malformed command.",
}


class App:
    """Build the window and coordinate serial requests with Tk's event loop."""

    def __init__(self, root):
        # Application state
        self.root = root
        self.client = SerialClient()

        # A thread-safe mailbox: the worker posts data; poll() updates widgets.
        self.events = queue.Queue()

        # connected: handshake succeeded; playing: moves currently enabled;
        # busy: one exchange is pending; closing: window close was requested.
        self.connected = self.playing = self.busy = self.closing = False

        # Initial blank placeholder. Subsequent boards come from server replies.
        self.board = "---------"
        self.ai_mode = False

        # Window and heading
        root.title("STM32 · Tic-Tac-Toe")
        root.minsize(440, 540)
        root.protocol("WM_DELETE_WINDOW", self.quit)
        _apply_theme(root)

        # pack() arranges the main sections vertically. grid() is used inside
        # a separate frame for the nine board cells; each parent uses one layout.
        frame = ttk.Frame(root, padding=20)
        frame.pack(fill="both", expand=True)

        ttk.Label(
            frame,
            text="STM32 Tic-Tac-Toe",
            font=("Segoe UI", 20, "bold"),
            foreground=NEON_BLUE,
        ).pack(anchor="w")

        ttk.Label(
            frame,
            text="Two players · one board · STM32 referee",
            foreground=FG_DIM,
        ).pack(anchor="w", pady=(0, 16))

        # Serial connection controls
        connection = ttk.Frame(frame)
        connection.pack(fill="x")

        self.port = ttk.Combobox(connection, width=15)
        self.port.pack(side="left", fill="x", expand=True)

        self.refresh_button = ttk.Button(connection, text="Refresh", command=self.refresh)
        self.refresh_button.pack(side="left", padx=6)

        self.connect_button = ttk.Button(connection, text="Connect", command=self.connect)
        self.connect_button.pack(side="left")

        self.status = tk.StringVar(value="Select or type a serial port, then connect.")
        ttk.Label(frame, textvariable=self.status, wraplength=390).pack(fill="x", pady=16)

        # Game board
        grid = ttk.Frame(frame)
        grid.pack(fill="both", expand=True)
        self.cells = []

        for i in range(9):
            # n=i captures this iteration's index. Without it, all callbacks
            # would use the loop's final value. No mark is placed on click.
            button = tk.Button(
                grid,
                text="",
                font=("Segoe UI", 26, "bold"),
                width=3,
                relief="flat",
                bg=BG_PANEL, fg=WHITE,
                activebackground=NEON_BLUE, activeforeground=BG,
                disabledforeground=FG_DIM,
                highlightbackground=NEON_BLUE, highlightthickness=1,
                command=lambda n=i: self.send(f"MOVE:{n}"),
            )
            button.grid(row=i // 3, column=i % 3, sticky="nsew", padx=2, pady=2)
            self.cells.append(button)

        for i in range(3):
            # Equal weights let all rows/columns grow with the window.
            grid.rowconfigure(i, weight=1)
            grid.columnconfigure(i, weight=1)

        # Game actions
        actions = ttk.Frame(frame)
        actions.pack(fill="x", pady=14)
        self.ai_button = ttk.Button(
            frame, text="Play against STM32 (you are X)",
            command=lambda: self.send("NEW:AI"),
        )
        self.ai_button.pack(fill="x", pady=(0, 8))

        self.new_button = ttk.Button(
            actions, text="New game", command=lambda: self.send("NEW")
        )
        self.new_button.pack(side="left")

        self.resign_button = ttk.Button(
            actions, text="Resign", command=lambda: self.send("RESIGN")
        )
        self.resign_button.pack(side="left", padx=8)

        self.disconnect_button = ttk.Button(
            actions, text="Disconnect", command=lambda: self.send("DISCONNECT")
        )
        self.disconnect_button.pack(side="right")

        # Traffic log and initial display
        self.log = tk.Text(
            frame, height=5, state="disabled",
            font=("Consolas", 9),
            bg=BG_PANEL, fg=FG_DIM,
            insertbackground=NEON_BLUE,
            relief="flat", borderwidth=0,
        )
        self.log.pack(fill="x")

        self.refresh()
        self.controls()
        # after schedules a callback; it does not pause the UI thread.
        root.after(50, self.poll)

    # Connection and display helpers

    def refresh(self):
        """Discover serial ports, retaining a manually entered port if present."""
        names = [p.device for p in list_ports.comports()]
        self.port["values"] = names

        if not self.port.get() and names:
            self.port.set(names[0])

    def controls(self):
        """Render confirmed cells and enable controls according to local flags.

        Occupied cells stay clickable during play so the server can reject
        them and the GUI can demonstrate its specific error response.
        """
        available = self.connected and not self.busy

        self.port.configure(state="disabled" if self.connected or self.busy else "normal")
        self.connect_button.configure(
            state="normal" if not self.connected and not self.busy else "disabled"
        )
        self.refresh_button.configure(
            state="disabled" if self.busy or self.connected else "normal"
        )

        self.new_button.configure(state="normal" if available else "disabled")
        self.ai_button.configure(state="normal" if available else "disabled")
        self.disconnect_button.configure(state="normal" if available else "disabled")
        self.resign_button.configure(state="normal" if available and self.playing else "disabled")

        for i, button in enumerate(self.cells):
            mark = self.board[i]
            colour = NEON_BLUE if mark == "X" else NEON_RED if mark == "O" else WHITE
            button.configure(
                text="" if mark == "-" else mark,
                fg=colour,
                state="normal" if available and self.playing else "disabled",
            )

    def log_line(self, text):
        """Append diagnostic traffic to the read-only log on the Tk thread."""
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")

        # Tk text indices use 'line.column'; trim old output as it grows.
        if int(self.log.index("end-1c").split(".")[0]) > 200:
            self.log.delete("1.0", "2.0")

        self.log.see("end")
        self.log.configure(state="disabled")

    def connect(self):
        """Read the selected port and schedule the connection handshake."""
        name = self.port.get().strip()

        if not name:
            self.status.set("Select or type a serial port first.")
            return
        self.start("CONNECT", name)

    # Sending commands in the background

    def send(self, command):
        """Entry point for game/disconnect button callbacks."""
        self.start(command)

    def start(self, command, name=None):
        """Allow one outstanding request and launch it without blocking Tk."""
        if self.busy:
            return

        self.busy = True
        self.status.set("Waiting for STM32…")
        self.controls()

        # The worker is a daemon so it cannot keep Python alive after UI exit.
        threading.Thread(
            target=self.exchange,
            args=(command, name),
            daemon=True,
        ).start()

    def exchange(self, command, name):
        """Perform I/O, validate the reply, and publish events for poll().

        Event payloads are ('log', text), ('error', explanation), or
        ('reply', (original_command, response, optional_board)).
        """
        # This worker exclusively owns the serial port. Tk widgets stay on the UI thread.
        try:
            reply = (
                self.client.open(name)
                if command == "CONNECT"
                else self.client.request(command)
            )
            self.events.put(("log", f"> {command}\n< {reply}"))

            board = board_from_reply(reply)
            terminal = reply in ("WIN:X", "WIN:O", "DRAW")

            # A well-framed reply can still be wrong for a command: for example,
            # NEW must return a board, whereas RESIGN must return OK:GAMEOVER.
            # NACKs are valid protocol replies describing a rejected action.
            valid = (
                reply.startswith("NACK:")
                or (command in ("CONNECT", "DISCONNECT") and reply == "OK")
                or (
                    command in ("NEW", "NEW:AI")
                    and reply.startswith("OK:BOARD:")
                    and board is not None
                )
                or (
                    command.startswith("MOVE:")
                    and (
                        terminal
                        or (reply.startswith("OK:BOARD:") and board is not None)
                    )
                )
                or (command == "RESIGN" and reply == "OK:GAMEOVER")
            )

            if not valid:
                raise ProtocolError("Unexpected response: " + reply)

            if terminal:
                # Terminal replies omit the board. Fetch confirmed final cells
                # rather than guessing where the last mark should appear.
                state = self.client.request("STATE")
                self.events.put(("log", f"> STATE\n< {state}"))
                board = board_from_reply(state)
                if not state.startswith("BOARD:") or board is None:
                    raise ProtocolError("Could not retrieve the final board.")

            if command == "DISCONNECT":
                self.client.close()

            self.events.put(("reply", (command, reply, board)))

        except Exception as exc:
            # This worker boundary reports failures to the UI and drops the
            # connection so uncertain/late data cannot affect another request.
            self.client.close()
            self.events.put(("error", str(exc)))

    # Receiving results on the Tk thread

    def poll(self):
        """Drain queued worker events on the Tk thread, then schedule again."""
        try:
            while True:
                kind, data = self.events.get_nowait()

                if kind == "log":
                    self.log_line(data)
                    continue

                # Log events do not complete an exchange; reply/error events do.
                self.busy = False
                if kind == "error":
                    self.connected = self.playing = False
                    self.status.set("Connection closed: " + data)
                    self.log_line("! " + data)
                else:
                    self.apply_reply(*data)

                self.controls()

        except queue.Empty:
            # get_nowait() lets Tk resume processing mouse/keyboard events.
            pass

        # Wait for pending I/O before disconnecting; never close the serial
        # handle from the UI while the worker is using it.
        if self.closing and not self.busy:
            if self.connected:
                self.send("DISCONNECT")
            else:
                self.root.destroy()
                return

        self.root.after(50, self.poll)

    def apply_reply(self, command, reply, board):
        """Apply a validated exchange result to display/session state.

        A rejection without a board leaves the displayed cells unchanged.
        These flags control the interface; they do not enforce game rules.
        """
        if board is not None:
            self.board = board

        if command in ("NEW", "NEW:AI") and reply.startswith("OK:BOARD:"):
            self.ai_mode = command == "NEW:AI"

        if command == "DISCONNECT":
            self.connected = self.playing = False

        if reply.startswith("NACK:"):
            code = reply[5:]
            if code in ("NOGAME", "ILLEGAL"):
                self.playing = False
            self.status.set(ERRORS.get(code, "Server rejected the action: " + code))

        elif command == "CONNECT":
            self.connected = True
            self.playing = False
            self.status.set("Connected. Select New game to start.")

        elif command == "DISCONNECT":
            self.connected = self.playing = False
            self.status.set("Disconnected.")

        elif reply in ("WIN:X", "WIN:O", "DRAW", "OK:GAMEOVER"):
            self.playing = False
            self.status.set(
                {
                    "WIN:X": "X wins!",
                    "WIN:O": "O wins!",
                    "DRAW": "Draw!",
                    "OK:GAMEOVER": "Game ended by resignation.",
                }[reply]
                + " Select New game to play again."
            )

        else:
            self.playing = True
            # Display hint derived from confirmed state; legality remains on the server.
            turn = "X" if self.board.count("X") == self.board.count("O") else "O"
            self.status.set(f"{turn} to move.")
            if self.ai_mode:
                self.status.set("Your turn (X). STM32 plays O.")

    # Window shutdown

    def quit(self):
        """Request orderly shutdown; poll() handles pending I/O/disconnection."""
        self.closing = True
        self.status.set("Closing connection…")


if __name__ == "__main__":
    # mainloop dispatches callbacks and repaints until the window is destroyed.
    root = tk.Tk()
    App(root)
    root.mainloop()
