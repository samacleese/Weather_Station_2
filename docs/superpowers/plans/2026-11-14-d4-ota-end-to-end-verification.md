# D4 OTA End-to-End Verification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close #61 by running only the OTA cases D2/D3 never ran through the real sketch, recording a full coverage map for #61, and deleting the throwaway OTA spikes.

**Architecture:** One new host tool, `tools/ota_publish_fault.py`, publishes a deliberately broken manifest by reusing `tools/ota_publish.py`'s signing and upload functions. A hardware session with Jesse runs the four gaps against the shipping sketch. The spikes in `tests/spikes/` get deleted, and the `build-spike` plumbing stays for future spikes.

**Tech Stack:** Python 3 + `cryptography` + pytest (host tool), ESP32 Arduino firmware on the Inkplate 10, Pi json-store.

**Spec:** Issue #61, plus `docs/superpowers/plans/2026-07-26-ota-implementation-roadmap.md` (D4 section). The roadmap wins where it disagrees with `docs/superpowers/specs/2026-04-13-ota-firmware-update-design.md`.

## Scope decision (Jesse, 2026-11-14)

D2 and D3 already verified most of #61's matrix on hardware. D4 runs only the gaps, records the rest by citing the earlier runs, and deletes the spikes.

| #61 case | Status before D4 | D4 action |
|---|---|---|
| Confirm path | D3: reset wake and real deep-sleep wake, plus release 20261112001 | cite |
| Skip path, no hold, 30 s | D3: no hold, short taps, held through boot. `UpdateConfirmation::WINDOW_MS` is still 30000 | cite |
| Tampered `sha256` | D2 spike, and D3 through the sketch (binary swapped on the Pi) | cite |
| Tampered `signature` | D2 spike only | **run through the sketch** |
| Battery below `min_battery_mv` | D3 | cite |
| Missing manifest field (all six) | host tests only (`MissingFieldTest` in `tests/unit/OTAManagerTest.cpp`) | **run all six on the device** |
| WiFi dropped mid-stream | D2 spike only (`WiFi.disconnect()` from the callback) | **run through the sketch, with Jesse cutting the AP** |
| Same version already installed | D3 | cite, and re-check after the second install |
| Second consecutive OTA (`app0` → `app1` → `app0`) | probably happened in D3, but the partition labels weren't recorded | **run and record the erase-log labels** |
| Timer wake skips the check (added in D3) | D3 | cite |

## Global Constraints

- Every new code file starts with two `ABOUTME: ` lines. Python scripts then get a module docstring with a `Usage:` block and no shebang (see `tools/ota_publish.py`).
- Python tests live in `tests/python/` and run with `./build.sh test-python`. `tests/python/conftest.py` puts `tools/` on `sys.path`.
- Test output must be pristine.
- Commits are conventional commits referencing #61 and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Never use `git add -A`; add the named files.
- Don't guess the Pi host or the private key path. Ask Jesse. The docs' examples are `192.168.1.2:5000` and `keys/ecdsa_private.pem`.
- json-store binary ids must match `^[a-zA-Z0-9_-]+$`. The binary lives at `http://<pi-host>/ota-firmware/binaries/firmware` and the manifest at `http://<pi-host>/ota-firmware/data`.
- Worktree builds log `Firmware version: unknown` and show `unknown` on the splash. That's known. Prove which build is running with `FIRMWARE_VERSION`, via the prompt/no-prompt behaviour and the erase-log partition label.

---

## Setup (before Task 1)

- [ ] **Create the worktree and branch**

```bash
cd /home/jesse/Arduino/Weather_Station_2
git worktree add .worktrees/feat/issue-61-ota-e2e -b feat/issue-61-ota-e2e master
cp config.cmake .worktrees/feat/issue-61-ota-e2e/config.cmake
cp docs/superpowers/plans/2026-11-14-d4-ota-end-to-end-verification.md .worktrees/feat/issue-61-ota-e2e/docs/superpowers/plans/
cd .worktrees/feat/issue-61-ota-e2e
./build.sh configure && ./build.sh test-host && ./build.sh test-python
```

Expected: every existing test passes. If any fail, stop and fix them before going on.

- [ ] **Commit the plan on the branch**

```bash
git add docs/superpowers/plans/2026-11-14-d4-ota-end-to-end-verification.md
git commit -m "docs(ota): add D4 end-to-end verification plan (#61)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Then delete the untracked copy in the main checkout: `rm /home/jesse/Arduino/Weather_Station_2/docs/superpowers/plans/2026-11-14-d4-ota-end-to-end-verification.md`.

---

### Task 1: `tools/ota_publish_fault.py` fault publisher

**Files:**
- Create: `tools/ota_publish_fault.py`
- Create: `tests/python/test_ota_publish_fault.py`
- Modify: `tests/python/conftest.py` (gets `RecordingServer` and the `server` fixture)
- Modify: `tests/python/test_ota_publish.py` (drops its own `RecordingServer`/`server` and uses the conftest one)

**Interfaces:**
- Consumes, from `tools/ota_publish.py`: `PublishError`, `build_manifest(firmware, private_key, version, url, min_battery_mv) -> dict`, `publish(pi_host, firmware, manifest)`, `load_private_key(path)`, `read_version(repo_root) -> int`, `binary_url(pi_host) -> str`, `REPO_ROOT`, `DEFAULT_MIN_BATTERY_MV`.
- Produces: `MANIFEST_FIELDS: tuple[str, ...]`, `apply_fault(manifest: dict, fault: str) -> dict`, and a CLI: `python3 tools/ota_publish_fault.py --firmware <bin> --private-key <pem> --pi-host <host:port> --fault bad-signature|missing:<field>`.

Fault semantics:
- `bad-signature`: decode the base64 signature, flip the low bit of its last byte, re-encode. The result is still valid base64 of the same length, so the device gets past the manifest parse and download and fails at `mbedtls_pk_verify`. This is the D2 spike's "signature with last byte flipped" case.
- `missing:<field>`: drop exactly that key. `<field>` must be one of the six in `MANIFEST_FIELDS`: `version`, `size`, `sha256`, `signature`, `url`, `min_battery_mv`.
- Anything else raises `PublishError`. `apply_fault` returns a new dict and never mutates its input.

The binary is uploaded as-is. Only the manifest is broken.

- [ ] **Step 1: Move `RecordingServer` into conftest**

Cut the `RecordingServer` class and the `server` fixture out of `tests/python/test_ota_publish.py`, unchanged, into `tests/python/conftest.py`, along with the imports they need (`http.server`, `threading`, `pytest`). Change conftest's second ABOUTME line to:

```python
# ABOUTME: Makes tools/*.py importable as modules, and provides a real local HTTP server that records PUTs.
```

In `test_ota_publish.py`, `test_publish_raises_on_error_status` builds `RecordingServer(status=400)` directly. Import it with `from conftest import RecordingServer`, and drop the `http.server`/`threading` imports there if nothing else uses them.

Run: `./build.sh test-python`
Expected: PASS, same test count as Setup.

- [ ] **Step 2: Write the failing tests**

`tests/python/test_ota_publish_fault.py`:

```python
# ABOUTME: Tests for tools/ota_publish_fault.py: each fault breaks exactly one thing in an otherwise valid
# ABOUTME: manifest, and the CLI publishes the broken manifest to a real local HTTP server.
import base64
import json
import subprocess
import sys
from pathlib import Path

import pytest
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

from ota_publish import PublishError, build_manifest
from ota_publish_fault import MANIFEST_FIELDS, apply_fault

TOOL = Path(__file__).resolve().parent.parent.parent / "tools" / "ota_publish_fault.py"
FIRMWARE = b"\xe9" + bytes(range(256)) * 40


@pytest.fixture
def private_key():
    return ec.generate_private_key(ec.SECP256R1())


@pytest.fixture
def manifest(private_key):
    return build_manifest(
        firmware=FIRMWARE,
        private_key=private_key,
        version=20261114001,
        url="http://pi:5000/ota-firmware/binaries/firmware",
        min_battery_mv=3600,
    )


def verify(public_key, manifest):
    signature = base64.b64decode(manifest["signature"])
    digest = bytes.fromhex(manifest["sha256"])
    public_key.verify(signature, digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))


def test_manifest_fields_match_the_device_parser():
    # Same six fields OTAManagerTest's MissingFieldTest covers.
    assert MANIFEST_FIELDS == ("version", "size", "sha256", "signature", "url", "min_battery_mv")


def test_bad_signature_fails_verification_but_stays_well_formed(private_key, manifest):
    faulty = apply_fault(manifest, "bad-signature")
    with pytest.raises(InvalidSignature):
        verify(private_key.public_key(), faulty)
    original = base64.b64decode(manifest["signature"])
    flipped = base64.b64decode(faulty["signature"])
    assert len(flipped) == len(original)
    assert flipped[:-1] == original[:-1]
    assert flipped[-1] == original[-1] ^ 0x01


def test_bad_signature_changes_nothing_else(manifest):
    faulty = apply_fault(manifest, "bad-signature")
    assert {k: v for k, v in faulty.items() if k != "signature"} == {
        k: v for k, v in manifest.items() if k != "signature"
    }


@pytest.mark.parametrize("field", MANIFEST_FIELDS)
def test_missing_field_drops_exactly_that_field(manifest, field):
    faulty = apply_fault(manifest, f"missing:{field}")
    assert field not in faulty
    assert faulty == {k: v for k, v in manifest.items() if k != field}


def test_apply_fault_does_not_mutate_its_input(manifest):
    before = dict(manifest)
    apply_fault(manifest, "missing:url")
    apply_fault(manifest, "bad-signature")
    assert manifest == before


@pytest.mark.parametrize("fault", ["missing:colour", "missing:", "tamper", ""])
def test_unknown_fault_raises(manifest, fault):
    with pytest.raises(PublishError, match="fault"):
        apply_fault(manifest, fault)


def write_key(tmp_path, private_key):
    path = tmp_path / "key.pem"
    path.write_bytes(
        private_key.private_bytes(
            serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()
        )
    )
    return path


def run_tool(*args):
    return subprocess.run([sys.executable, str(TOOL), *args], capture_output=True, text=True)


def test_cli_publishes_binary_then_faulty_manifest(server, tmp_path, private_key):
    firmware = tmp_path / "fw.bin"
    firmware.write_bytes(FIRMWARE)
    result = run_tool(
        "--firmware", str(firmware),
        "--private-key", str(write_key(tmp_path, private_key)),
        "--pi-host", server.host,
        "--fault", "missing:url",
    )
    assert result.returncode == 0, result.stderr
    assert "missing:url" in result.stdout
    assert [path for path, _, _ in server.requests] == ["/ota-firmware/binaries/firmware", "/ota-firmware/data"]
    assert server.requests[0][2] == FIRMWARE
    published = json.loads(server.requests[1][2])
    assert "url" not in published
    assert published["size"] == len(FIRMWARE)
    # Nothing written next to the firmware: a good manifest.json from ota_publish.py must not be clobbered.
    assert not (tmp_path / "manifest.json").exists()


def test_cli_rejects_unknown_fault_without_uploading(server, tmp_path, private_key):
    firmware = tmp_path / "fw.bin"
    firmware.write_bytes(FIRMWARE)
    result = run_tool(
        "--firmware", str(firmware),
        "--private-key", str(write_key(tmp_path, private_key)),
        "--pi-host", server.host,
        "--fault", "missing:colour",
    )
    assert result.returncode == 1
    assert "ERROR:" in result.stderr and "fault" in result.stderr
    assert server.requests == []
```

- [ ] **Step 3: Run the tests to make sure they fail**

Run: `./build.sh test-python`
Expected: `test_ota_publish_fault.py` fails to collect with `ModuleNotFoundError: No module named 'ota_publish_fault'`. Everything else passes.

- [ ] **Step 4: Write the implementation**

`tools/ota_publish_fault.py`:

```python
# ABOUTME: Publishes a deliberately broken OTA manifest to the Pi json-store, for on-device negative tests.
# ABOUTME: Reuses ota_publish.py's signing and upload, then breaks exactly one thing in the manifest.
"""
Usage:
    python3 tools/ota_publish_fault.py \\
        --firmware build/WeatherStation/Weather_Station_2.ino.bin \\
        --private-key keys/ecdsa_private.pem \\
        --pi-host 192.168.1.2:5000 \\
        --fault bad-signature | missing:<field>

Use this for OTA negative tests on real hardware (roadmap D4, #61), never for a release. Faults:

    bad-signature    flip one bit of the signature; the device downloads, then rejects the image
    missing:<field>  drop one of: version, size, sha256, signature, url, min_battery_mv;
                     the device treats the manifest as "no update"

The version comes from the repo's VERSION file, like ota_publish.py, so bump VERSION above the
device's firmware first or the device ignores the manifest before reaching the fault. This leaves a
broken manifest on the Pi: put a good release back afterwards with tools/ota_publish.py, and
check it with tools/ota_verify.py --pi-host <host>.
"""

import argparse
import base64
import sys
from pathlib import Path

from ota_publish import (
    DEFAULT_MIN_BATTERY_MV,
    REPO_ROOT,
    PublishError,
    binary_url,
    build_manifest,
    load_private_key,
    publish,
    read_version,
)

# Every field OTAManager::checkForUpdate requires.
MANIFEST_FIELDS = ("version", "size", "sha256", "signature", "url", "min_battery_mv")
MISSING_PREFIX = "missing:"


def apply_fault(manifest, fault):
    faulty = dict(manifest)
    if fault == "bad-signature":
        signature = bytearray(base64.b64decode(faulty["signature"]))
        signature[-1] ^= 0x01
        faulty["signature"] = base64.b64encode(bytes(signature)).decode("ascii")
        return faulty
    if fault.startswith(MISSING_PREFIX) and fault[len(MISSING_PREFIX) :] in MANIFEST_FIELDS:
        del faulty[fault[len(MISSING_PREFIX) :]]
        return faulty
    fields = ", ".join(MANIFEST_FIELDS)
    raise PublishError(f"unknown fault {fault!r}: use bad-signature or missing:<field> with field one of {fields}")


def main():
    parser = argparse.ArgumentParser(
        description="Publish a deliberately broken OTA manifest for on-device negative tests.",
        epilog="Restore a good release afterwards with tools/ota_publish.py.",
    )
    parser.add_argument("--firmware", required=True, type=Path, help="built .bin to upload unchanged")
    parser.add_argument("--private-key", required=True, type=Path, help="ECDSA P-256 PEM from tools/ota_keygen.py")
    parser.add_argument("--pi-host", required=True, help="json-store host:port, e.g. 192.168.1.2:5000")
    parser.add_argument("--fault", required=True, help="bad-signature, or missing:<field>")
    args = parser.parse_args()

    try:
        if not args.firmware.is_file():
            raise PublishError(f"firmware not found: {args.firmware} (run ./build.sh build first)")
        firmware = args.firmware.read_bytes()
        manifest = build_manifest(
            firmware=firmware,
            private_key=load_private_key(args.private_key),
            version=read_version(REPO_ROOT),
            url=binary_url(args.pi_host),
            min_battery_mv=DEFAULT_MIN_BATTERY_MV,
        )
        faulty = apply_fault(manifest, args.fault)
        publish(args.pi_host, firmware, faulty)
        print(f"Published a broken manifest ({args.fault}), version {manifest['version']}, to {args.pi_host}")
        print("Restore a good release with tools/ota_publish.py when the test is done")
    except PublishError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
```

The CLI tests run the script as a subprocess, so `tools/` is the script's own directory and `from ota_publish import ...` resolves without conftest.

- [ ] **Step 5: Run the tests to make sure they pass**

Run: `./build.sh test-python`
Expected: PASS, with no warnings in the output. Also check `python3 tools/ota_publish_fault.py --help` prints the usage.

- [ ] **Step 6: Commit**

```bash
git add tools/ota_publish_fault.py tests/python/test_ota_publish_fault.py tests/python/conftest.py tests/python/test_ota_publish.py
git commit -m "feat(ota): add a fault publisher for on-device negative tests (#61)

D4 needs a tampered signature and each missing manifest field to reach the
real sketch, and ota_publish.py only publishes good manifests. The new tool
reuses its signing and upload and breaks exactly one thing. RecordingServer
moves to conftest so both test files share it.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Hardware run of the four gaps

**Files:** none changed, unless a case fails. A failure starts a systematic-debugging pass on the cause, not a workaround. Record every result and figure as you go. Task 4 needs them.

Jesse has to be at the device for every case, and at the access point for case 2d. Ask Jesse for the Pi host and the private key path first. Capture each case with `python3 tools/serial_capture.py --seconds <N>` (it resets the device, which counts as a reset/power-on wake, so the OTA check runs). Keep every capture's output for Task 4.

Test images (keep copies, because each build overwrites `build/WeatherStation/Weather_Station_2.ino.bin`):

```bash
mkdir -p build/ota-test
# Baseline = the committed VERSION (20261112001)
./build.sh build && cp build/WeatherStation/Weather_Station_2.ino.bin build/ota-test/baseline.bin
echo 20261114001 > VERSION && ./build.sh configure && ./build.sh build \
  && cp build/WeatherStation/Weather_Station_2.ino.bin build/ota-test/t1.bin
echo 20261114002 > VERSION && ./build.sh configure && ./build.sh build \
  && cp build/WeatherStation/Weather_Station_2.ino.bin build/ota-test/t2.bin
```

`build/` is gitignored. Never commit a bumped `VERSION`.

- [ ] **Step 2a: Baseline.** `git checkout VERSION && ./build.sh configure && ./build.sh build && ./build.sh flash`. Save what's published now: `curl -s http://<pi-host>/ota-firmware/data`. Capture 40 s. Expected: `No firmware update available` and a normal weather screen. The sketch doesn't log the running partition. Steps 2e and 2f prove the alternation by naming the target slot in their erase logs.

- [ ] **Step 2b: Tampered signature.** `echo 20261114001 > VERSION`, then:
  `python3 tools/ota_publish_fault.py --firmware build/ota-test/t1.bin --private-key <key> --pi-host <pi-host> --fault bad-signature`.
  Confirm the fault is live: `python3 tools/ota_verify.py --pi-host <pi-host>` should report the signature as invalid. Capture 120 s; Jesse holds WAKE 2 s at the prompt.
  Expected: download progress, `OTA signature rejected: -19968`, `OTA image failed verification; keeping the running firmware`, `OTA install failed`, "Update failed. Continuing to weather." for 5 s, a normal weather screen. Then capture another 40 s **without** holding. The prompt appears again, which proves the device is still on the baseline version and the running partition was untouched.

- [ ] **Step 2c: Each missing field.** With `VERSION` still 20261114001, for each `F` in `version size sha256 signature url min_battery_mv`: publish with `--fault missing:F`, capture 40 s, touch nothing. Expected: no prompt, no download, a normal weather screen, and this warning:

  | Missing field | Expected warning |
  |---|---|
  | `version`, `size`, `min_battery_mv` | `OTA manifest has a missing or non-integer version, size or min_battery_mv` |
  | `sha256` | `OTA manifest sha256 is missing or not 64 hex chars` |
  | `signature` | `OTA manifest signature is missing, empty or too long` |
  | `url` | `OTA manifest url is missing, empty or too long` |

- [ ] **Step 2d: WiFi drop mid-download.** Publish T1 for real: `python3 tools/ota_publish.py --firmware build/ota-test/t1.bin --private-key <key> --pi-host <pi-host>` (VERSION is still 20261114001). Confirm with `python3 tools/ota_verify.py --pi-host <pi-host>`. Make sure the dev machine reaches the Pi some way that survives the AP going down. If it can't, start a longer capture first. Capture 180 s; Jesse holds WAKE 2 s, then cuts the AP's WiFi once the screen shows 25%, and brings it back about 30 s later.
  Expected: `OTA download received nothing for 30000 ms` (or `OTA download stopped at N of M bytes`), `OTA install failed`, "Update failed. Continuing to weather." for 5 s, then the sketch moves on to the weather fetch. Whether WiFi reconnects in time for that fetch is **unknown**: record what happens. A normal weather screen and a network-error screen (300 s sleep) both pass. A hang or a crash fails. Then capture 40 s without holding: the prompt appears, so the running partition is untouched.

- [ ] **Step 2e: First install (T1).** Capture 120 s; Jesse holds WAKE 2 s. Expected: `Erased OTA partition <X> in N ms`, progress 0/25/51/76/100, `OTA image installed; it boots on the next restart`, a restart. Record `<X>` and the total install time.

- [ ] **Step 2f: Second install (T2).** `echo 20261114002 > VERSION`, publish T2 with `tools/ota_publish.py --firmware build/ota-test/t2.bin`, then verify. Capture 120 s and hold. Expected: `Erased OTA partition <Y>` with `<Y>` ≠ `<X>`, installed, restarted. That's the second consecutive OTA, back to the starting slot.

- [ ] **Step 2g: Same version installed.** Capture 40 s. Expected: `No firmware update available`, no prompt.

- [ ] **Step 2h: Restore.** `git checkout VERSION && ./build.sh configure && ./build.sh build && ./build.sh flash`. Republish the baseline so the Pi matches the device: `python3 tools/ota_publish.py --firmware build/ota-test/baseline.bin --private-key <key> --pi-host <pi-host>`, then `tools/ota_verify.py --pi-host <pi-host>`. Capture 40 s: `No firmware update available`. Confirm with Jesse that this is how they want the Pi left. `git status` should show `VERSION` unchanged.

---

### Task 3: Delete the throwaway OTA spikes

**Files:**
- Delete: `tests/spikes/OtaUpdate/`, `tests/spikes/WakeButton/`, `tests/spikes/OtaCheck/`, `tests/spikes/OtaInstall/` (including `stall_server.py` and `OtaInstallSettings.h.in`)
- Modify: `tests/spikes/CMakeLists.txt`
- Keep: `build.sh`'s `build-spike`/`flash-spike` and their tests in `tests/build-script/test_build_script.sh`. The tests use `OtaUpdate` only as a name that fails before any build, so they still pass.

Run this after Task 2, because Task 2 might need the `OtaInstall` spike to debug a failure.

- [ ] **Step 1: Delete and keep the mechanism**

```bash
git rm -r tests/spikes/OtaUpdate tests/spikes/WakeButton tests/spikes/OtaCheck tests/spikes/OtaInstall
```

Replace `tests/spikes/CMakeLists.txt` with:

```cmake
# CMakeLists.txt
# ABOUTME: Throwaway hardware spike sketches, each answering one feasibility question on the device.
# ABOUTME: Add one with add_subdirectory(<Name>); build with ./build.sh build-spike <Name>. Delete when done.
```

- [ ] **Step 2: Check nothing else references them**

```bash
grep -rn "OtaInstall\|OtaCheck\|WakeButton\|OTA_STALL_URL\|stall_server" --exclude-dir=build --exclude-dir=.worktrees --exclude-dir=.git . | grep -v "docs/superpowers/"
```

Expected: only `tests/build-script/test_build_script.sh` (the `OtaUpdate` name checks) and nothing else. Historical plans under `docs/superpowers/` keep their references. If `config.cmake` defines `OTA_STALL_URL`, tell Jesse it's now unused. It's gitignored, so don't edit it.

- [ ] **Step 3: Verify the build still configures and every suite passes**

```bash
./build.sh configure && ./build.sh build && ./build.sh test-host && ./build.sh test-python && bash tests/build-script/test_build_script.sh
```

Expected: all pass with pristine output.

- [ ] **Step 4: Commit**

```bash
git add tests/spikes/CMakeLists.txt
git commit -m "chore(ota): delete the throwaway OTA spikes (#61)

The shipping sketch and FirmwareInstaller now cover everything the four spikes
proved, and D4 ran the negative cases through the real sketch. build-spike stays
for future spikes.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Docs and PR

**Files:**
- Modify: `docs/superpowers/plans/2026-07-26-ota-implementation-roadmap.md` (D4 row to ✅, D4 section results)
- Modify: `CLAUDE.md` (only if the spike line needs a note; it stays valid as-is)

- [ ] **Step 1: Roadmap.** Set the D4 status row to ✅. Under `### D4. End-to-end verification`, replace the "30s elapses" wording with "no hold within the 30 s window", and add:
  - the coverage map from this plan's scope table, with each "cite" row pointing at its D2/D3 result
  - Task 2's results table: each case, the log lines seen, pass/fail
  - measurements: partition labels `<X>` → `<Y>`, both install times, and what WiFi did after the AP drop
  - a note that `tools/ota_publish_fault.py` exists for re-running the negative cases, and that the spikes were deleted

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/plans/2026-07-26-ota-implementation-roadmap.md
git commit -m "docs(ota): record D4 end-to-end results and coverage map (#61)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 3: PR.** Push over HTTPS (see `reference_git_push_https` memory), then `gh pr create`. The body says `Closes #61`, includes the coverage map, says which rows were cited and which were re-run, and ends with `🤖 Generated with [Claude Code](https://claude.com/claude-code)`. Ask Jesse before merging.

---

## Out of scope

- Signing the manifest version (#85) and a compiled-in battery floor (#84).
- Trickled response headers blocking `HTTPClient::GET()` (#82).
- Automatic rollback. A bad image is recovered by reflashing over USB.
- Fixing `unknown` as the version in worktree builds.
