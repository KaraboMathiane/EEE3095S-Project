# Serial Game Server Protocol

## 1. Purpose and authority

This document defines the line-oriented serial contract between the PC GUI
and the STM32 game server. The STM32 is the sole authority for the board,
whose turn it is, move legality, and the game result. The GUI sends user
input and renders state reported by the server; it must not optimistically
change the authoritative board.

The game is two-player, turn-based tic-tac-toe. `X` starts, followed by `O`.

## 2. Serial transport

| Setting | Value |
| --- | --- |
| Baud rate | 115200 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |
| Encoding | ASCII |
| Message terminator | LF (`\n`); the receiver also accepts CRLF |

These settings are the Phase 1 interface baseline. Firmware UART configuration
and the GUI serial-port settings must match. Messages are single-line records;
the server emits LF line endings.

## 3. Board and cell numbering

The board is serialized in row-major order, from the top-left cell to the
bottom-right cell:

```text
1 | 2 | 3
--+---+--
4 | 5 | 6
--+---+--
7 | 8 | 9
```

The state string contains exactly nine characters. `X` and `O` indicate
occupied cells; `-` indicates an empty cell. For example, `X-O--O---`
represents X in cells 1 and 3, and O in cells 2 and 6.

## 4. Message grammar

Each record is one ASCII line. Fields are comma-separated; commas and newlines
are not permitted inside fields. Tokens shown below are case-sensitive.

### GUI to STM32

| Record | Meaning |
| --- | --- |
| `HELLO` | Start or re-synchronize the GUI session. Does not reset the game. |
| `MOVE,<cell>` | Request a move in cell `1` through `9`. |
| `RESET` | Start a new game with an empty board and X to move. |

Examples:

```text
HELLO
MOVE,5
RESET
```

### STM32 to GUI

| Record | Meaning |
| --- | --- |
| `READY,1` | Server is ready; `1` is the protocol version. |
| `STATE,<board>,<turn>,<result>` | Complete authoritative game state. |
| `ERROR,<code>` | The preceding request was rejected. |

`<board>` is the nine-character board string. `<turn>` is `X`, `O`, or `-`
when the game is over. `<result>` is one of `PLAYING`, `X_WON`, `O_WON`, or
`DRAW`.

Example:

```text
STATE,X-O--O---,X,PLAYING
```

Error codes:

| Code | Meaning |
| --- | --- |
| `BAD_COMMAND` | Unknown command, malformed record, or invalid field count. |
| `BAD_CELL` | Cell is not a decimal digit from 1 to 9. |
| `CELL_OCCUPIED` | The requested cell is already occupied. |
| `GAME_OVER` | A move was requested after a win or draw. |

## 5. Session and game behavior

1. After boot, the server starts a fresh game: empty board, X to move,
   `PLAYING`.
2. On `HELLO`, the server replies with `READY,1` followed by its current
   `STATE`. Reconnecting the GUI does not erase the board.
3. For a legal `MOVE,<cell>`, the server marks the cell for the current
   player, evaluates all winning lines and a draw, then emits the resulting
   `STATE`.
4. If the game remains in progress after a legal move, the turn changes to
   the other player. After a win or draw, the turn is `-`.
5. A rejected move leaves the board and turn unchanged. The server emits
   `ERROR,<code>` followed by the unchanged `STATE`.
6. `RESET` clears the board, sets X to move, and emits the new `STATE`.
7. Once the game has ended, further moves are rejected with `GAME_OVER` until
   `RESET`.
8. The server processes complete commands in receive order. Duplicate move
   requests are not special-cased; after the first is accepted, a duplicate
   normally receives `CELL_OCCUPIED` or `GAME_OVER`.

## 6. Framing and parser requirements

- A command is processed only after its line terminator is received.
- The firmware parser must use a fixed-size receive buffer and enforce a
  maximum record length of 32 bytes, excluding the line terminator.
- On an overlong record, discard bytes through the next line terminator,
  return `ERROR,BAD_COMMAND`, and continue receiving subsequent commands.
- Empty lines and malformed records return `ERROR,BAD_COMMAND`.
- A command must not partially update game state. Validate the entire command
  before applying a move.
- The GUI must tolerate partial serial reads and assemble records by line
  terminator; a read call is not guaranteed to contain exactly one record.
- On connection, the GUI sends `HELLO` and waits for `READY,1` plus `STATE`
  before enabling move input.

## 7. Example exchange

```text
GUI -> STM32: HELLO
STM32 -> GUI: READY,1
STM32 -> GUI: STATE,---------,X,PLAYING
GUI -> STM32: MOVE,5
STM32 -> GUI: STATE,----X----,O,PLAYING
GUI -> STM32: MOVE,5
STM32 -> GUI: ERROR,CELL_OCCUPIED
STM32 -> GUI: STATE,----X----,O,PLAYING
GUI -> STM32: RESET
STM32 -> GUI: STATE,---------,X,PLAYING
```

## 8. Scope boundary

This protocol describes the Phase 1 serial interface. The corresponding UART
peripheral/pins, USB-to-UART adapter, GUI framework, and hardware wiring are
implementation details to document when selected. Any change to message
names, field order, cell numbering, or state values must be reflected in this
document and in both endpoints.