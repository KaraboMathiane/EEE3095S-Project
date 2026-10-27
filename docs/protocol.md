# Serial Game Server Protocol

The STM32 owns the board, turn, move validation and result. The PC submits
commands and displays confirmed replies. Both players share one PC client in
two-player mode; in AI mode the human is X and the STM32 is O.

## Connection and framing

| Setting | Value |
|---|---|
| Peripheral | USART1: PA9 TX, PA10 RX, alternate function 1 |
| Serial format | 115200 baud, 8 data bits, no parity, 1 stop bit |
| Flow control | None |
| MCU clock | Reset-default 8 MHz HSI |
| Encoding | Printable ASCII, case-sensitive |
| Terminator | LF (`\n`); incoming CRLF also accepted |
| Maximum command | 32 bytes, excluding the terminator |
| Incomplete-frame timeout | 1000 serviced milliseconds without a consumed byte |

Use a 3.3 V logic-level UART connection, crossed TX/RX and shared GND.
The course board uses separate programming/power and UART USB connections.

Send one command and wait for its complete reply before sending the next.
Replies end in LF. Empty lines, unknown commands, invalid characters and
oversized frames receive `NACK:BAD`. CR is permitted only immediately before
LF. Oversized/invalid frames are discarded through LF; an incomplete frame
is cleared after the timeout. Rejected requests leave game state unchanged.

USART1 interrupts capture bytes and error markers into a 128-entry ring
(127 usable entries). Main performs framing and command handling. Receive
errors or detected queue overflow mark the affected frame invalid. Buffering
allows LCD/button work while bytes arrive; unlimited sustained traffic is
not supported. No checksum or CRC is implemented.

## Board encoding

Nine characters represent cells in row-major order:

```text
0 | 1 | 2
3 | 4 | 5
6 | 7 | 8
```

`X` and `O` are occupied cells; `-` is empty. X starts.
For example, `X-O--O---` places X at 0 and O at 2 and 5.
Both `OK:BOARD:` and `BOARD:` prefixes are followed by exactly nine cells.

## Commands and replies

| Command | Requirement | Successful reply / effect |
|---|---|---|
| `CONNECT` | Any state | `OK`; establishes an idle session if disconnected |
| `NEW` | Connected | `OK:BOARD:---------`; starts/restarts two-player mode |
| `NEW:AI` | Connected | `OK:BOARD:---------`; starts/restarts AI mode |
| `MOVE:<index>` | Running game | `OK:BOARD:<cells>`, or `WIN:X`, `WIN:O`, `DRAW` |
| `STATE` | Connected with a started game | `BOARD:<cells>`; also works after game over |
| `RESIGN` | Running game | `OK:GAMEOVER`; ends the game, preserving cells |
| `DISCONNECT` | Any state | `OK`; clears session, game and AI mode |

CONNECT is idempotent during an established session: it does not reset an
existing game. NEW and NEW:AI can restart from any connected state, including
mid-game. DISCONNECT is also accepted when already disconnected.

`<index>` contains one or more decimal digits. Leading zeros are accepted;
signs, spaces and empty/non-decimal indices are malformed. Values 0--8
identify cells. Commands do not carry a player identity.

## Errors and validation order

| Reply | Meaning |
|---|---|
| `NACK:BAD` | Invalid frame, unknown command or malformed index |
| `NACK:RANGE` | Numeric index outside 0--8 |
| `NACK:NOTURN` | MOVE during O's turn in a playing AI game |
| `NACK:NOGAME` | Required session or started game does not exist |
| `NACK:ILLEGAL` | MOVE or RESIGN after game over |
| `NACK:OCCUPIED` | Target cell already contains a mark |

After framing, MOVE checks decimal syntax, range, then the AI turn guard.
The game engine then checks session/game existence, playing status and cell
occupancy. The first failed check determines the reply. For example,
`MOVE:9` returns `NACK:RANGE` even without a game; a valid `MOVE:0` without
a game returns `NACK:NOGAME`.

A rejection changes neither board, turn nor result, and triggers no AI move.

## Game modes and terminal results

In two-player mode each accepted move uses the server's current mark, then
switches turns unless terminal. The client cannot choose a mark or prove
which person submitted the request.

In AI mode one MOVE submits X's action. If the game continues, the STM32
chooses and validates O's action before replying. A non-terminal board reply
therefore includes both marks and returns control to X. No second MOVE is
needed for O. The AI uses minimax with alpha-beta pruning, preferring earlier
wins and later losses; equal scores favour centre, corners, then edges.
Search uses state copies and no heap allocation.

A terminal MOVE returns only `WIN:X`, `WIN:O` or `DRAW`. The client must send
STATE to obtain the final board. Further moves return `NACK:ILLEGAL`;
NEW/NEW:AI starts another game. RESIGN ends the game without a winner token.

Lifecycle states are represented by game flags/result:

- **IDLE:** boot/disconnected, or connected before the first game.
- **IN_PROGRESS:** a started game with result PLAYING.
- **GAME_OVER:** win, draw or resignation; STATE remains available.

The separate connection flag determines whether NEW/NEW:AI is allowed.

## Example: AI game and rejected move

Each line below is terminated by LF on the wire.

```text
PC -> STM32: CONNECT
STM32 -> PC: OK
PC -> STM32: NEW:AI
STM32 -> PC: OK:BOARD:---------
PC -> STM32: MOVE:0
STM32 -> PC: OK:BOARD:X---O----
PC -> STM32: MOVE:4
STM32 -> PC: NACK:OCCUPIED
PC -> STM32: STATE
STM32 -> PC: BOARD:X---O----
PC -> STM32: DISCONNECT
STM32 -> PC: OK
```

## PC client behaviour

The GUI serialises exchanges in a worker thread and processes results through
a queue on the Tk thread. It checks ASCII framing, board length/symbols and
command-specific replies before updating cells. Serial reads/writes use
two-second timeouts; reply reads are capped at 65 bytes. A timeout or invalid
reply closes the connection to avoid misidentifying late replies. Reconnect
and start a new game to recover.

STATE returns cells only, not mode, turn or terminal metadata. The GUI does
not resume an unknown game after connecting: the user starts NEW or NEW:AI.

## Local LCD, LEDs and buttons

These features send no unsolicited serial messages and never change game state.

| Hardware | Behaviour |
|---|---|
| LCD board view | Nine cells separated by `/`, AI/2P mode, turn or result |
| LCD summary view | Mode, occupied-cell count and turn, or final result/count |
| PB0--PB7 | Active-high cumulative move bar; saturates at eight for eight/nine marks |
| PB10/PB11 | Red/green turn indicators; both toggle every 250 serviced ms after game over |
| SW0 / PA0 | Active-low pull-up input; toggles all game LEDs, not the LCD |
| SW1 / PA1 | Active-low pull-up input; toggles LCD board/summary view |

The progress bar clears on NEW/NEW:AI or DISCONNECT and retains its value at
game over. Rejections do not advance it; AI exchanges normally add two marks.
The existing status-LED driver uses active-low output logic; confirm actual
colour/polarity on the fitted board. AI O-turn indication may not be visible
because search is synchronous.

PA0 uses three matching 10 ms samples. PA1 uses 30 consecutive serviced
millisecond samples for both press and release. Holds do not repeat toggles.
The view defaults to board after reset and persists across new games.
Before a game exists, both views show connection/waiting information.

LCD wiring: RS=PC14, E=PC15, D4=PB8, D5=PB9, D6=PA12, D7=PA15;
backlight control=PC13 and R/W is grounded. LCD writes occur only when the
formatted screen changes. The polling-based servicing means prolonged work
can delay local blink/debounce timing.

## Differences from the assignment reference

- Empty cells use `-` rather than `.`.
- Malformed input uses `NACK:BAD` rather than `NACK:BADCMD`.
- `NACK:ILLEGAL` is the documented post-game rejection.
- `NEW:AI` adds the optional MCU opponent; NEW retains two-player mode.
- `NACK:NOTURN` protects AI-mode turn ownership; two-player mode shares one client.

Terminal result tokens and the follow-up STATE request retain the reference
protocol's separation of result and board. The optional checksum is omitted.
