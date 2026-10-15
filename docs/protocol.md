# Serial Game Server Protocol

## Purpose and authority

The STM32 owns the game board, turn, legality checks, and terminal result. The
PC client sends commands and displays server replies; it does not authoritatively
change the board. This contract follows the Tic-Tac-Toe reference protocol in
the assignment brief.

## Transport and framing

| Setting | Value |
| --- | --- |
| Baud rate | 115200 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |
| Encoding | ASCII |
| Frame terminator | LF (`\n`); CRLF is also accepted |
| Maximum command length | 32 bytes, excluding the terminator |

The initial firmware uses USART1 at 115200 8N1. On the STM32F051C8T6, the
assumed pins are PA9 (USART1_TX) and PA10 (USART1_RX), alternate function 1.
Connect the board to a 3.3 V USB-to-UART adapter with TX and RX crossed and
ground shared. Do not connect a 5 V UART signal to the MCU. The firmware uses
the reset-default 8 MHz HSI clock; verify this clock and pin assignment against
the actual board before wiring.

Frames are newline-terminated and contain printable ASCII command bytes only.
The firmware buffers at most 32 command bytes.
After an overlong frame it discards input through the next LF and replies
`NACK:BAD`. An incomplete frame is discarded after a one-second inter-byte
timeout and also receives `NACK:BAD`. Replies are ASCII and LF-terminated.
Only complete, valid commands can change game state.

## Board representation

The board is represented by exactly nine characters, indexed from 0 through
8 in row-major order:

```text
0 | 1 | 2
--+---+--
3 | 4 | 5
--+---+--
6 | 7 | 8
```

`X` and `O` identify occupied cells; `-` identifies an empty cell. X moves
first. For example, `X-O--O---` represents X in cells 0 and 2 and O in cells
1 and 5. The board string is included in `OK:BOARD:` and `BOARD:` responses.

## Commands and replies

Each command is one line. Command names and reply tokens are case-sensitive.

| GUI to STM32 | STM32 to GUI | Meaning |
| --- | --- | --- |
| `CONNECT` | `OK` | Establish a session. The server is idle. |
| `NEW` | `OK:BOARD:<9 chars>` | Start a game and return the empty board. |
| `MOVE:<index>` | `OK:BOARD:<9 chars>` | Accept a legal move and return the updated board. |
| `MOVE:<index>` | `WIN:X`, `WIN:O`, or `DRAW` | Report a terminal move. |
| `MOVE:<index>` | `NACK:OCCUPIED`, `NACK:ILLEGAL`, or `NACK:RANGE` | Reject a move; board remains unchanged. |
| `STATE` | `BOARD:<9 chars>` | Return the current board. |
| `RESIGN` | `OK:GAMEOVER` | Forfeit the current game. |
| `DISCONNECT` | `OK` | End the session. |
| Malformed command / no game | `NACK:BAD` / `NACK:NOGAME` | Reject a malformed or unknown command / command requiring a game. |

`<index>` is a non-negative decimal integer. Indices 0 through 8 identify
board cells; any other numeric index receives `NACK:RANGE`. A non-decimal or
empty index is malformed and receives `NACK:BAD`. A move into an occupied
cell receives `NACK:OCCUPIED`. A move after a game has ended receives
`NACK:ILLEGAL`. Commands that require an active session/game when none exists
receive `NACK:NOGAME`.

`NEW` starts a fresh game with X to move. For a legal, non-terminal move the
server changes the turn and returns `OK:BOARD:<board>`. A winning or drawing
move returns `WIN:X`, `WIN:O`, or `DRAW` instead. Rejected requests do not
change the board or turn. `STATE` returns the board but does not start or
resume a game.

`RESIGN` ends a running game without changing the board. `DISCONNECT` ends
the session and clears its game. `CONNECT` is idempotent while connected; a
new connection starts idle and requires `NEW` before game commands can be
used.

## Example exchange

```text
GUI -> STM32: CONNECT
STM32 -> GUI: OK
GUI -> STM32: NEW
STM32 -> GUI: OK:BOARD:---------
GUI -> STM32: MOVE:4
STM32 -> GUI: OK:BOARD:----X----
GUI -> STM32: MOVE:4
STM32 -> GUI: NACK:OCCUPIED
GUI -> STM32: STATE
STM32 -> GUI: BOARD:----X----
GUI -> STM32: DISCONNECT
STM32 -> GUI: OK
```

The optional checksum suffix mentioned in the assignment is not used in this
baseline. The maximum frame length, newline terminator, timeout recovery, and
explicit rejection replies provide the initial framing and parser contract.
Any protocol changes must be documented here and kept consistent in the GUI,
firmware, and tests.
