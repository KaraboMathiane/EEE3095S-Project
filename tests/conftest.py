"""Make the script-based GUI modules importable during unit tests."""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "gui"))


def pytest_addoption(parser):
    parser.addoption(
        "--serial-port", default=None,
        help="Run physical STM32 tests on this port (for example COM5). Resets games.",
    )
