"""Compile pure LCD content formatting on the host; no physical display required."""
import os
import pathlib
import subprocess


def test_lcd_screen_content(tmp_path):
    # Exercise boot, session, both turns, all results and both game modes.
    root = pathlib.Path(__file__).resolve().parents[1]
    executable = tmp_path / "test_lcd.exe"
    result = subprocess.run([
        os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-pedantic", "-I", str(root / "Game_Server/Inc"),
        str(root / "Game_Server/Src/game.c"),
        str(root / "Game_Server/Src/lcd_format.c"),
        str(root / "tests/test_lcd_format.c"), "-o", str(executable),
    ], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
    assert run.returncode == 0, run.stderr
