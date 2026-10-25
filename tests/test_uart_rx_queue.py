"""Host checks for the ISR/main receive queue; hardware still verifies IRQ timing."""
import os
from pathlib import Path
import subprocess


def test_receive_queue(tmp_path):
    # Compile production buffering and check burst retention, wrap and overflow.
    root = Path(__file__).resolve().parents[1]
    executable = tmp_path / "test_rx.exe"
    result = subprocess.run([
        os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-pedantic", "-I", str(root / "Game_Server/Inc"),
        str(root / "Game_Server/Src/uart_rx_queue.c"),
        str(root / "tests/test_uart_rx_queue.c"), "-o", str(executable),
    ], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
    assert run.returncode == 0, run.stderr
