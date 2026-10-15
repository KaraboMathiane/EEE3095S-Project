# Serial Game Server

An STM32F051C8T6 hosts the authoritative game state and communicates with a
PC client over a serial link. The PC client will render the board and send
player input; it must not decide whether a move is legal or who won.

The assignment brief is [`EEE3095S_2026_Project_v2 (1).pdf`](./EEE3095S_2026_Project_v2%20(1).pdf).
The serial interface contract is in [`docs/protocol.md`](./docs/protocol.md).

## Repository layout

```text
Game_Server/   STM32CubeIDE firmware project
gui/           Planned Python PC client
tests/         Planned pytest harness
docs/          Protocol and project documentation
report/        Planned final report and submission files
```

## Current status

Phase 1 established the project structure. Phase 2 implements the STM32
Tic-Tac-Toe game engine, the assignment's newline-framed serial command
protocol, and a host-compiled pytest harness for game and protocol behavior.
The Python GUI and final report are still planned for later phases.

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

The Python client is planned but has not yet been implemented. To run the
firmware game/protocol tests on a PC, install GCC and Python 3.8 or newer:

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