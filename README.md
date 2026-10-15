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

Phase 1 establishes the project structure and serial protocol. The tracked
firmware currently contains the STM32 startup files, a linker script, and a
minimal `main.c` that configures the LED GPIO pins; it does not yet implement
the game server. The `gui/`, `tests/`, and `report/` directories are reserved
for later phases.

## Firmware setup and build

1. Install STM32CubeIDE with support for the STM32F0 series.
2. Import the existing `Game_Server` directory into the IDE as an existing
   Eclipse/CubeIDE project.
3. Select the `Debug` build configuration and build the project.
4. Connect and flash a compatible STM32F051C8T6 board using the configured
   debug probe.

The current firmware is only a GPIO baseline. No serial game server or
playable client is available yet.

## PC client and tests

The Python client and pytest harness are planned but have not been implemented.
Their install, run, and test commands will be added here when those components
are introduced. The serial command and response formats are specified in
[`docs/protocol.md`](./docs/protocol.md).

## Git notes

The CubeIDE workspace metadata and generated `Debug` output are ignored.
Do not commit `.metadata/` or generated build files.