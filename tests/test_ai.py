"""Compile and exhaustively exercise the STM32 AI on the host."""
import os
import pathlib
import subprocess


def test_ai_is_legal_and_unbeatable(tmp_path):
    # Compile production modules, then enumerate every legal human continuation.
    root = pathlib.Path(__file__).resolve().parents[1]
    executable = tmp_path / "test_ai.exe"
    sources = [root / "Game_Server/Src" / name for name in
               ("game.c", "game_ai.c", "server_protocol.c")]
    build = subprocess.run([
        os.environ.get("CC", "gcc"), "-std=c11", "-O2", "-Wall", "-Wextra",
        "-Werror", "-pedantic", "-I", str(root / "Game_Server/Inc"),
        *map(str, sources), str(root / "tests/test_ai.c"), "-o", str(executable),
    ], capture_output=True, text=True, timeout=30)
    assert build.returncode == 0, build.stderr
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    print(result.stdout)
