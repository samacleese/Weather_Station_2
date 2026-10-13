# ABOUTME: Tests for tools/ota_publish.py: manifest construction, the sign/verify contract,
# ABOUTME: and the HTTP upload against a real local HTTP server.
import base64
import hashlib
import http.server
import json
import threading

import pytest
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec, utils

from ota_publish import PublishError, build_manifest, publish, read_version

FIRMWARE = b"\xe9" + bytes(range(256)) * 40


@pytest.fixture
def private_key():
    return ec.generate_private_key(ec.SECP256R1())


def make_manifest(private_key, **overrides):
    args = dict(
        firmware=FIRMWARE,
        private_key=private_key,
        version=20260728001,
        url="http://pi:5000/ota-firmware/binaries/firmware",
        min_battery_mv=3600,
    )
    args.update(overrides)
    return build_manifest(**args)


def test_read_version_parses_integer(tmp_path):
    (tmp_path / "VERSION").write_text("20260728001\n")
    assert read_version(tmp_path) == 20260728001


def test_read_version_rejects_non_integer(tmp_path):
    (tmp_path / "VERSION").write_text("1.2.3\n")
    with pytest.raises(PublishError, match="VERSION"):
        read_version(tmp_path)


def test_manifest_records_size_digest_version_url_and_battery(private_key):
    manifest = make_manifest(private_key)
    assert manifest["version"] == 20260728001
    assert manifest["size"] == len(FIRMWARE)
    assert manifest["sha256"] == hashlib.sha256(FIRMWARE).hexdigest()
    assert manifest["url"] == "http://pi:5000/ota-firmware/binaries/firmware"
    assert manifest["min_battery_mv"] == 3600


def test_signature_verifies_over_prehashed_digest(private_key):
    # The device's mbedtls_ecdsa_verify() receives the 32-byte digest, so the signature must be
    # over the digest itself, not over the raw firmware hashed again by the library.
    manifest = make_manifest(private_key)
    digest = bytes.fromhex(manifest["sha256"])
    signature = base64.b64decode(manifest["signature"])
    private_key.public_key().verify(signature, digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))


def test_signature_rejects_tampered_digest(private_key):
    manifest = make_manifest(private_key)
    tampered = hashlib.sha256(FIRMWARE + b"\x00").digest()
    signature = base64.b64decode(manifest["signature"])
    with pytest.raises(InvalidSignature):
        private_key.public_key().verify(signature, tampered, ec.ECDSA(utils.Prehashed(hashes.SHA256())))


class RecordingServer:
    """A real HTTP server on localhost that records PUTs and answers with a chosen status."""

    def __init__(self, status=204):
        self.requests = []
        recorder = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_PUT(self):
                body = self.rfile.read(int(self.headers["Content-Length"]))
                recorder.requests.append((self.path, self.headers["Content-Type"], body))
                self.send_response(status)
                self.end_headers()

            def log_message(self, *args):
                pass

        self.httpd = http.server.HTTPServer(("127.0.0.1", 0), Handler)
        self.host = f"127.0.0.1:{self.httpd.server_address[1]}"
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def close(self):
        self.httpd.shutdown()
        self.httpd.server_close()


@pytest.fixture
def server():
    s = RecordingServer()
    yield s
    s.close()


def test_publish_uploads_binary_before_manifest(server, private_key):
    manifest = make_manifest(private_key)
    publish(server.host, FIRMWARE, manifest)

    paths = [path for path, _, _ in server.requests]
    assert paths == ["/ota-firmware/binaries/firmware", "/ota-firmware/data"]

    _, binary_type, binary_body = server.requests[0]
    assert binary_type == "application/octet-stream"
    assert binary_body == FIRMWARE

    _, manifest_type, manifest_body = server.requests[1]
    assert manifest_type == "application/json"
    assert json.loads(manifest_body) == manifest


def test_publish_raises_on_error_status(private_key):
    s = RecordingServer(status=400)
    try:
        with pytest.raises(PublishError, match="400"):
            publish(s.host, FIRMWARE, make_manifest(private_key))
        # Manifest must not be published if the binary upload failed.
        assert [path for path, _, _ in s.requests] == ["/ota-firmware/binaries/firmware"]
    finally:
        s.close()
