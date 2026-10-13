# ABOUTME: Tests for tools/ota_verify.py: checking a firmware release against its manifest and
# ABOUTME: public keys, and recovering the embedded key from the generated C++ header.
import pytest
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec

from ota_keygen import format_header
from ota_publish import build_manifest
from ota_verify import VerifyError, parse_header_der, verify_release

FIRMWARE = b"\xe9" + bytes(range(256)) * 40


@pytest.fixture
def private_key():
    return ec.generate_private_key(ec.SECP256R1())


@pytest.fixture
def manifest(private_key):
    return build_manifest(
        firmware=FIRMWARE, private_key=private_key, version=1, url="http://pi/x", min_battery_mv=3600
    )


def test_parse_header_der_round_trips_ota_keygen_output(private_key):
    der = private_key.public_key().public_bytes(
        encoding=serialization.Encoding.DER, format=serialization.PublicFormat.SubjectPublicKeyInfo
    )
    assert parse_header_der(format_header(der)) == der


def test_parse_header_der_rejects_header_without_key_array():
    with pytest.raises(VerifyError, match="OTA_PUBLIC_KEY_DER"):
        parse_header_der("// nothing here\n")


def test_valid_release_passes(manifest, private_key):
    verify_release(FIRMWARE, manifest, {"test key": private_key.public_key()})


def test_size_mismatch_fails(manifest, private_key):
    manifest["size"] += 1
    with pytest.raises(VerifyError, match="size"):
        verify_release(FIRMWARE, manifest, {"test key": private_key.public_key()})


def test_corrupted_firmware_fails_digest_check(manifest, private_key):
    corrupted = b"\x00" + FIRMWARE[1:]
    with pytest.raises(VerifyError, match="sha256"):
        verify_release(corrupted, manifest, {"test key": private_key.public_key()})


def test_wrong_key_fails_and_names_the_key(manifest):
    other = ec.generate_private_key(ec.SECP256R1()).public_key()
    with pytest.raises(VerifyError, match="embedded header key"):
        verify_release(FIRMWARE, manifest, {"embedded header key": other})
