# ABOUTME: Tests for the pure header-formatting logic in tools/ota_keygen.py.
# ABOUTME: Key generation and file I/O are not covered here.
from ota_keygen import format_header


def test_format_header_emits_include_guard():
    header = format_header(b"\x01\x02")
    assert "#ifndef OTA_PUBLIC_KEY_H" in header
    assert "#define OTA_PUBLIC_KEY_H" in header
    assert header.strip().endswith("#endif  // OTA_PUBLIC_KEY_H")


def test_format_header_encodes_bytes_as_hex_array():
    header = format_header(bytes([0x00, 0xAB, 0xFF]))
    assert "const uint8_t OTA_PUBLIC_KEY_DER[] = {0x00, 0xab, 0xff};" in header


def test_format_header_handles_empty_bytes():
    header = format_header(b"")
    assert "const uint8_t OTA_PUBLIC_KEY_DER[] = {};" in header
