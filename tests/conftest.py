"""Make the script-based GUI modules importable during unit tests."""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "gui"))
