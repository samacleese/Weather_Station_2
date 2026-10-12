# ABOUTME: Signs a built firmware image and publishes it plus a manifest to the Pi json-store.
# ABOUTME: The device polls the manifest, verifies the ECDSA signature, and downloads the binary.
"""
Usage:
    python3 tools/ota_publish.py \\
        --firmware build/WeatherStation/Weather_Station_2.ino.bin \\
        --private-key keys/ecdsa_private.pem \\
        --pi-host 192.168.1.2:5000 [--min-battery-mv 3600] [--dry-run]

Run after `./build.sh build` to cut an OTA release. The version comes from the repo's
VERSION file, so bump that first or devices will see nothing new.

Writes manifest.json next to the firmware, then uploads the binary to
http://<pi-host>/ota-firmware/binaries/firmware and the manifest to
http://<pi-host>/ota-firmware/data. The binary goes first: the manifest is what devices
poll, so publishing it last means it never points at a binary that isn't there yet.

Sign/verify contract: the signature is over the 32-byte SHA-256 digest (Prehashed), because
mbedtls_ecdsa_verify() on the device receives the digest, not the whole image.
"""

import argparse
import base64
import hashlib
import json
import sys
import urllib.error
import urllib.request
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

REPO_ROOT = Path(__file__).resolve().parent.parent
APP_NAME = "ota-firmware"
# json-store ids must match ^[a-zA-Z0-9_-]+$ — "firmware.bin" would be rejected with 400.
BINARY_ID = "firmware"
DEFAULT_MIN_BATTERY_MV = 3600


class PublishError(Exception):
    pass


def read_version(repo_root):
    text = (repo_root / "VERSION").read_text().strip()
    try:
        return int(text)
    except ValueError:
        raise PublishError(f"VERSION must be an integer, got {text!r}")


def binary_url(pi_host):
    return f"http://{pi_host}/{APP_NAME}/binaries/{BINARY_ID}"


def manifest_url(pi_host):
    return f"http://{pi_host}/{APP_NAME}/data"


def build_manifest(firmware, private_key, version, url, min_battery_mv):
    digest = hashlib.sha256(firmware).digest()
    signature = private_key.sign(digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
    return {
        "version": version,
        "size": len(firmware),
        "sha256": digest.hex(),
        "signature": base64.b64encode(signature).decode("ascii"),
        "url": url,
        "min_battery_mv": min_battery_mv,
    }


def put(url, body, content_type):
    request = urllib.request.Request(url, data=body, method="PUT", headers={"Content-Type": content_type})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            status = response.status
    except urllib.error.HTTPError as e:
        raise PublishError(f"PUT {url} failed: HTTP {e.code} {e.read().decode(errors='replace').strip()}")
    except urllib.error.URLError as e:
        raise PublishError(f"PUT {url} failed: {e.reason}")
    if not 200 <= status < 300:
        raise PublishError(f"PUT {url} failed: HTTP {status}")


def publish(pi_host, firmware, manifest):
    put(binary_url(pi_host), firmware, "application/octet-stream")
    put(manifest_url(pi_host), json.dumps(manifest).encode(), "application/json")


def load_private_key(path):
    try:
        return serialization.load_pem_private_key(path.read_bytes(), password=None)
    except FileNotFoundError:
        raise PublishError(f"private key not found: {path} (generate one with tools/ota_keygen.py)")


def main():
    parser = argparse.ArgumentParser(
        description="Sign firmware and publish it with a manifest to the Pi json-store.",
        epilog="The manifest version is read from the repo's VERSION file.",
    )
    parser.add_argument(
        "--firmware", required=True, type=Path, help="built .bin, e.g. build/WeatherStation/Weather_Station_2.ino.bin"
    )
    parser.add_argument("--private-key", required=True, type=Path, help="ECDSA P-256 PEM from tools/ota_keygen.py")
    parser.add_argument("--pi-host", required=True, help="json-store host:port, e.g. 192.168.1.2:5000")
    parser.add_argument(
        "--min-battery-mv",
        type=int,
        default=DEFAULT_MIN_BATTERY_MV,
        help=f"device skips the update below this battery voltage (default {DEFAULT_MIN_BATTERY_MV})",
    )
    parser.add_argument("--dry-run", action="store_true", help="write manifest.json but do not upload")
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
            min_battery_mv=args.min_battery_mv,
        )
        manifest_path = args.firmware.parent / "manifest.json"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
        print(f"Wrote {manifest_path}: version {manifest['version']}, {manifest['size']} bytes")
        print(f"  sha256 {manifest['sha256']}")

        if args.dry_run:
            print("Dry run: nothing uploaded")
            return
        publish(args.pi_host, firmware, manifest)
        print(f"Published {binary_url(args.pi_host)}")
        print(f"Published {manifest_url(args.pi_host)}")
    except PublishError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
