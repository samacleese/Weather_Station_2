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
    PublishError,
    load_release,
    publish,
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
    field = fault[len(MISSING_PREFIX) :]
    if fault.startswith(MISSING_PREFIX) and field in MANIFEST_FIELDS:
        del faulty[field]
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
        firmware, manifest = load_release(args.firmware, args.private_key, args.pi_host, DEFAULT_MIN_BATTERY_MV)
        faulty = apply_fault(manifest, args.fault)
        publish(args.pi_host, firmware, faulty)
        print(f"Published a broken manifest ({args.fault}), version {manifest['version']}, to {args.pi_host}")
        print("Restore a good release with tools/ota_publish.py when the test is done")
    except PublishError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
