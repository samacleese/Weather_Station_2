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
