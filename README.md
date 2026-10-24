# Serial Game Server

An STM32F051C8T6 hosts the authoritative game state and communicates with a
PC client over a serial link. The PC client will render the board and send
player input; it must not decide whether a move is legal or who won.

The assignment brief is [`EEE3095S_2026_Project_v2 (1).pdf`](./EEE3095S_2026_Project_v2%20(1).pdf).
The serial interface contract is in [`docs/protocol.md`](./docs/protocol.md).

## Repository layout

```text
Game_Server/   STM32CubeIDE firmware project
gui/           Tkinter PC client and serial transport
tests/         Host-compiled pytest harness
docs/          Protocol and project documentation
report/        Planned final report and submission files
```

## Current status

Phase 1 established the project structure. Phase 2 implements the STM32
Tic-Tac-Toe game engine, the assignment's newline-framed serial command
protocol, and a host-compiled pytest harness for game and protocol behavior.
The Tkinter GUI implements the current serial contract. Hardware integration
and the final report remain to be completed.

## Firmware setup and build

1. Install STM32CubeIDE with support for the STM32F0 series.
2. Import the existing `Game_Server` directory into the IDE as an existing
   Eclipse/CubeIDE project.
3. Select the `Debug` build configuration and build the project. The game
   engine and protocol are in `Game_Server/Src/`; the UART setup and main loop
   are in `Game_Server/Src/main.c`.
4. Connect a 3.3 V USB-to-UART adapter to PA9 (TX), PA10 (RX), and GND. Cross
   the adapter's TX/RX connections. USART1 is configured for 115200 8N1 and
   the firmware uses the reset-default 8 MHz HSI clock.
5. Connect and flash a compatible STM32F051C8T6 board using the configured
   debug probe.

Verify that the board exposes USART1 on PA9/PA10 and uses the expected clock
before wiring. The current firmware retains the initial GPIO LED setup.

## PC client and tests

To run the GUI, use Python 3.9 or newer with Tkinter installed (included in
the standard Windows Python installer). From the repository root:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r gui/requirements.txt
python gui/main.py
```

Select or type the board's serial port, select Connect, then New game. Both
players share the same window and alternate X/O. Occupied cells remain
clickable so rejection messages can be demonstrated. The STM32 decides all
moves and results; the GUI only draws boards received from the server.
After a win/draw, it requests STATE to retrieve the final board.

The client uses 115200 8N1 with a two-second reply timeout. A timeout or
invalid reply closes the connection to avoid assigning a late reply to the
next command. Reconnect and start a new game to recover. The command log
helps diagnose integration problems. Close other serial terminals before
connecting. Flashing through a debug probe does not necessarily expose
USART1 as a PC serial port; confirm the wiring described above.

The current protocol cannot report whether a STATE board is still in play;
therefore a new GUI connection requires New game. Hardware-button changes
and unsolicited updates need a jointly agreed protocol extension.

To run the firmware game/protocol tests on a PC, install GCC and Python:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r tests/requirements.txt
python -m pytest -q
```

The tests compile the same portable C game and protocol modules with GCC and
exercise them through a small host-side serial-line harness. They do not
emulate STM32 registers, UART timing, or physical hardware. The command and
reply formats are specified in [`docs/protocol.md`](./docs/protocol.md).

## Git notes

The CubeIDE workspace metadata and generated `Debug` output are ignored.
Do not commit `.metadata/` or generated build files.
