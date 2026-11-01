# ABOUTME: Guards against the host unit tests and the device firmware using different ArduinoJson versions.
# ABOUTME: A skew here once made host tests drop every parsed field while the device worked (issue #33).
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


def _single_match(pattern, path):
    matches = re.findall(pattern, path.read_text())
    assert len(matches) == 1, f"expected exactly one ArduinoJson pin in {path}, found {matches}"
    return matches[0]


def test_host_and_device_pin_the_same_arduinojson_version():
    device = _single_match(r'"ArduinoJson@([\d.]+)"', REPO_ROOT / "Containerfile")
    host = _single_match(
        r"GIT_REPOSITORY\s+https://github\.com/bblanchon/ArduinoJson\.git\s+GIT_TAG\s+v([\d.]+)",
        REPO_ROOT / "tests/unit/CMakeLists.txt",
    )

    assert host == device, f"host tests use ArduinoJson {host} but the device uses {device}"
