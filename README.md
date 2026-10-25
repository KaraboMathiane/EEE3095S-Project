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
before wiring. The firmware assumes an active-low button on PA0 and active-low
red/green LEDs on PB10/PB11. During play, the LED identifies the server's
current turn; both LEDs blink after game over. Pressing the button toggles
these indicators without changing the game. Confirm pins and polarity against
the actual board before use.

## PC client and tests

### On-board LCD

The eight individual LEDs (PB0--PB7) show move progress: zero occupied cells
means all off, then each accepted mark adds one lit LED starting at PB0.
Eight and nine marks both display all eight LEDs. Rejected moves leave the
bar unchanged; AI exchanges normally add two marks. The bar stays visible at
game over and clears for a new game or disconnect. The debounced button now
toggles both the progress bar and the existing turn/game-over status LEDs;
the LCD stays active. Verify the physical LED ordering after flashing.

The fitted 16x2 LCD shows a boot/waiting screen, connection status, and the
authoritative board as `X--/-O-/---` followed by `AI` or `2P`. The second row
shows the turn, winner, draw or resignation. Updates occur only when the
displayed text changes, before the UART response is sent. The existing LED
toggle button does not disable the LCD.

The UCT schematic maps RS/E to PC14/PC15, D4/D5 to PB8/PB9, D6/D7 to
PA12/PA15 and backlight control to PC13. The driver uses the board's existing
8 MHz clock and SysTick; changing that timing requires reviewing the driver.
The LCD power and logic jumpers must match the fitted module. Do not change
powered jumpers. Adjust the contrast potentiometer if text is faint or bars
hide the characters. Rebuild/flash with `lcd.c` and `lcd_format.c` included.
Host tests check text formatting; pin mapping, LCD timing and visible output
require board verification and rerunning the serial regression suite.

### STM32 AI opponent

After connecting, choose **Play against STM32 (you are X)** for an AI game.
The MCU plays O using minimax; the PC only submits X moves and displays the
confirmed board. **New game** starts the original two-human mode.
Rebuild and flash the firmware after updating: `Src/game_ai.c` must be included
in the CubeIDE build (it is under the existing Src source directory).

The host AI test explores every legal human continuation against the chosen
deterministic strategy and checks that X never wins and no mark is overwritten.
Run it with `python -m pytest tests/test_ai.py -v -s`. Physical AI tests try all
nine opening cells; run the serial suite again after flashing. Existing XML
results predate the AI extension and do not validate this firmware revision.

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
therefore a new GUI connection requires New game. The on-board button toggles
the local turn/game-over LED indicators only; it does not modify the game or
send unsolicited serial messages.

The firmware assumes an active-low user button on PA0 and active-low red and
green LEDs on PB10 and PB11. Confirm those pin assignments and polarities
against the actual UCT STM board before relying on the hardware behaviour.
During play, the LED for the current server turn is on. After a win, draw, or
resignation, both LEDs blink. A debounced button press toggles these indicators
on or off.

To run the firmware game/protocol tests on a PC, install GCC and Python:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r tests/requirements.txt
python -m pytest -q
```

The tests compile the portable C game, protocol, and hardware-behaviour
modules with GCC. The host-side serial-line harness exercises game/protocol
commands, while dedicated unit tests exercise button debounce and LED state
selection. They do not emulate STM32 registers, UART timing, or physical
hardware. The command and reply formats are specified in
[`docs/protocol.md`](./docs/protocol.md).

## Git notes

### Physical UART regression tests

Flash the latest firmware and leave it running. Connect both USB cables and
close the GUI and serial terminal so the UART port is free. These tests replace
the current game and disconnect when each case finishes.

```powershell
python -m pip install -r tests/requirements.txt
python -m pytest tests/test_serial_hardware.py --serial-port COM5 -v --junitxml=hardware-results.xml
```

Replace COM5 if necessary. The suite checks all eight X winning lines, an O win,
a draw, rejected moves, resignation, restart, disconnect, CRLF, split input,
garbage/oversized frames and partial-frame timeout recovery. Without
`--serial-port`, hardware cases are skipped and no serial device is opened.
Run sequentially; do not use parallel pytest workers with one board. The XML
file records results for report evidence. LEDs and button operation still need
manual physical checks. A missing/busy selected port is a failure, not a skip.

The CubeIDE workspace metadata and generated `Debug` output are ignored.
Do not commit `.metadata/` or generated build files.
