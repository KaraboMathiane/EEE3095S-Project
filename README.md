# STM32 Serial Tic-Tac-Toe

A Tic-Tac-Toe game with an STM32F051 server and a Python desktop GUI.
The STM32 enforces the rules and supports both two-player and AI modes.
The board also displays game information on its LCD and LEDs.

## Requirements

- UCT STM32F051 development board with LCD and USB-to-UART adapter.
- STM32CubeIDE with STM32F0 support.
- Python 3.10 or newer with Tkinter (tested with Python 3.13).
- GCC on PATH for host-side C tests.

## 1. Build and flash the firmware

1. Import `Game_Server/` into STM32CubeIDE as an existing project.
2. Select the **Debug** configuration and build the complete project.
3. Connect the board's programming USB cable and flash it.
4. Leave the program running; resume execution if the debugger is paused.
5. Connect the UART adapter's USB cable and identify its COM port.

The programming and serial links use separate USB connections. USART1 uses
**PA9 (TX), PA10 (RX), 115200 baud, 8N1, no flow control**. For a separate
3.3 V UART adapter, cross TX/RX and share GND. Firmware timing assumes the
reset-default 8 MHz HSI clock.

## 2. Set up Python

Run these commands from the repository root in PowerShell:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r gui/requirements.txt -r tests/requirements.txt
```

## 3. Run the GUI

```powershell
.\.venv\Scripts\python.exe gui/main.py
```

Select the UART port (for example **COM5**) and click **Connect**.

| Control | Action |
|---|---|
| New game | Start a two-player game; X moves first |
| Play against STM32 | Start an AI game; you are X and the STM32 is O |
| Board cell | Submit a move; rejected moves show a specific error |
| Resign | End the current game |
| Disconnect | End the session and release the serial port |

The GUI displays server-confirmed state and disables moves after game over.
A new game can be started without reconnecting.

## Board feedback

| Hardware | Behaviour |
|---|---|
| LCD | Board and turn/result, or a game summary |
| Eight LEDs | Accepted move count; all eight light for eight or nine marks |
| Red/green status LEDs | Turn indication; both blink after game over |
| SW0 / PA0 | Toggle all game LEDs |
| SW1 / PA1 | Switch LCD between board and summary views |

The buttons change local displays only; the game state stays unchanged.

## Run tests

**Host tests ? no board required:**

```powershell
.\.venv\Scripts\python.exe -m pytest -q
```

These test the C logic and mocked Python client/GUI. Physical tests are skipped.

**Hardware tests ? flash the board and close the GUI/serial terminal first:**

```powershell
.\.venv\Scripts\python.exe -m pytest tests/test_serial_hardware.py --serial-port COM5 -v --junitxml=hardware-results-ai.xml
```

Replace COM5 with your port. These tests reset the game and must run
sequentially. Buttons, LEDs and LCD appearance also need manual checking.

## Troubleshooting

- **Port busy:** close other programs using the COM port.
- **No response:** check both USB cables, the selected port and that firmware is running.
- **Timeout:** reconnect in the GUI, then start a new game.
- **LCD bars or faint text:** adjust the LCD contrast potentiometer.

## Files

| Folder | Contents |
|---|---|
| `Game_Server/` | STM32CubeIDE firmware |
| `gui/` | Python GUI and serial client |
| `tests/` | Host and physical-UART tests |
| `docs/protocol.md` | Commands, responses and hardware behaviour |
| `report/` | LaTeX report |

See [the protocol specification](docs/protocol.md) for implementation details.
