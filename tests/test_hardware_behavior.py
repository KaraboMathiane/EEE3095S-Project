import os
import pathlib
import subprocess

import pytest


ROOT = pathlib.Path(__file__).resolve().parents[1]


@pytest.fixture(scope="session")
def hardware_behavior_binary(tmp_path_factory):
    build_dir = tmp_path_factory.mktemp("hardware-behavior-build")
    executable = build_dir / "test_hardware_behavior"
    command = [
        os.environ.get("CC", "gcc"),
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        "-I",
        str(ROOT / "Game_Server" / "Inc"),
        str(ROOT / "Game_Server" / "Src" / "game.c"),
        str(ROOT / "Game_Server" / "Src" / "hardware_behavior.c"),
        str(ROOT / "tests" / "test_hardware_behavior.c"),
        "-o",
        str(executable),
    ]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    assert result.returncode == 0, result.stderr
    return executable


def test_hardware_behavior(hardware_behavior_binary):
    result = subprocess.run(
        [str(hardware_behavior_binary)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
