# ABOUTME: Verifies an OTA release: manifest size, SHA-256 and ECDSA signature against the firmware,
# ABOUTME: checked with both the public key PEM and the key embedded in the firmware header.
"""
Usage:
    # Check what is actually published on the Pi (fetches manifest and binary):
    python3 tools/ota_verify.py --pi-host 192.168.1.2:5000

    # Check a local build before publishing (manifest.json from ota_publish.py --dry-run):
    python3 tools/ota_verify.py --firmware build/WeatherStation/Weather_Station_2.ino.bin \\
        --manifest build/WeatherStation/manifest.json

Run after tools/ota_publish.py to confirm a release is one the device will accept. The signature
is checked against keys/ecdsa_public.pem and against src/security/OTAPublicKey.h, which is the
key the firmware actually verifies with; a mismatch between those two means the device would
reject every update.
"""

import argparse
import base64
import hashlib
import json
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

from ota_publish import binary_url, manifest_url

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PUBLIC_KEY = REPO_ROOT / "keys" / "ecdsa_public.pem"
PUBLIC_KEY_HEADER = REPO_ROOT / "src" / "security" / "OTAPublicKey.h"


class VerifyError(Exception):
    pass


def parse_header_der(header_text):
    match = re.search(r"OTA_PUBLIC_KEY_DER\[\]\s*=\s*\{([^}]*)\}", header_text)
    if not match:
        raise VerifyError("no OTA_PUBLIC_KEY_DER array found in header")
    return bytes(int(h, 16) for h in re.findall(r"0x([0-9a-fA-F]{2})", match.group(1)))


def verify_release(firmware, manifest, public_keys):
    """Raise VerifyError on the first problem. public_keys maps a display name to a key."""
    if manifest["size"] != len(firmware):
        raise VerifyError(f"size mismatch: manifest says {manifest['size']}, firmware is {len(firmware)} bytes")
    digest = hashlib.sha256(firmware).digest()
    if manifest["sha256"] != digest.hex():
        raise VerifyError(f"sha256 mismatch: manifest says {manifest['sha256']}, firmware is {digest.hex()}")
    signature = base64.b64decode(manifest["signature"])
    for name, key in public_keys.items():
        try:
            key.verify(signature, digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
        except InvalidSignature:
            raise VerifyError(f"signature does not verify against {name}")


def fetch(url):
    try:
        with urllib.request.urlopen(url, timeout=30) as response:
            return response.read()
    except urllib.error.HTTPError as e:
        raise VerifyError(f"GET {url} failed: HTTP {e.code}")
    except urllib.error.URLError as e:
        raise VerifyError(f"GET {url} failed: {e.reason}")


def read_file(path, what):
    try:
        return path.read_bytes()
    except FileNotFoundError:
        raise VerifyError(f"{what} not found: {path}")


def main():
    parser = argparse.ArgumentParser(description="Verify an OTA firmware release against its manifest.")
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--pi-host", help="fetch the published manifest and binary from this json-store host:port")
    source.add_argument("--firmware", type=Path, help="local firmware .bin (requires --manifest)")
    parser.add_argument("--manifest", type=Path, help="local manifest.json, used with --firmware")
    parser.add_argument(
        "--public-key", type=Path, default=DEFAULT_PUBLIC_KEY, help=f"PEM public key (default {DEFAULT_PUBLIC_KEY})"
    )
    args = parser.parse_args()
    if args.firmware and not args.manifest:
        parser.error("--firmware requires --manifest")

    try:
        if args.pi_host:
            manifest = json.loads(fetch(manifest_url(args.pi_host)))
            firmware = fetch(manifest["url"])
            if manifest["url"] != binary_url(args.pi_host):
                print(f"WARNING: manifest url {manifest['url']} is not {binary_url(args.pi_host)}")
        else:
            manifest = json.loads(read_file(args.manifest, "manifest"))
            firmware = read_file(args.firmware, "firmware")

        public_keys = {
            str(args.public_key): serialization.load_pem_public_key(read_file(args.public_key, "public key")),
            "embedded header key": serialization.load_der_public_key(
                parse_header_der(read_file(PUBLIC_KEY_HEADER, "public key header").decode())
            ),
        }
        verify_release(firmware, manifest, public_keys)
    except VerifyError as e:
        print(f"FAIL: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"OK: version {manifest['version']}, {manifest['size']} bytes, sha256 {manifest['sha256']}")
    print(f"    signature verified against {', '.join(public_keys)}")


if __name__ == "__main__":
    main()
