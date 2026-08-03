# ABOUTME: Makes tools/*.py importable as modules in the test suite below.
# ABOUTME: The scripts live in tools/ as standalone CLI entry points, not a package.
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / "tools"))
