# ABOUTME: Tests for tools/check_firmware_size.py: reading the OTA slot size from a compiled ESP32
# ABOUTME: partition table and reporting firmware usage against it.
import struct

import pytest

from check_firmware_size import SizeCheckError, check_firmware_size, ota_slot_size

APP = 0x00
DATA = 0x01
OTA_0 = 0x10
OTA_1 = 0x11
NVS = 0x02


def entry(type_, subtype, offset, size, label):
    return struct.pack("<HBBII16sI", 0x50AA, type_, subtype, offset, size, label.encode(), 0)


def table(*entries):
    # Real tables are terminated by an MD5 entry (magic 0xEBEB) and padded with 0xFF.
    md5_entry = struct.pack("<H", 0xEBEB) + b"\xff" * 14 + b"\x00" * 16
    return b"".join(entries) + md5_entry + b"\xff" * 64


MIN_SPIFFS = table(
    entry(DATA, NVS, 0x9000, 0x5000, "nvs"),
    entry(APP, OTA_0, 0x10000, 0x1E0000, "app0"),
    entry(APP, OTA_1, 0x1F0000, 0x1E0000, "app1"),
)


def test_ota_slot_size_reads_ota_1_partition():
    assert ota_slot_size(MIN_SPIFFS) == 0x1E0000


def test_ota_slot_size_rejects_table_without_ota_1():
    huge_app = table(entry(DATA, NVS, 0x9000, 0x5000, "nvs"), entry(APP, OTA_0, 0x10000, 0x300000, "app0"))
    with pytest.raises(SizeCheckError, match="no ota_1"):
        ota_slot_size(huge_app)


def test_ota_slot_size_rejects_garbage():
    with pytest.raises(SizeCheckError, match="no ota_1"):
        ota_slot_size(b"\x00" * 96)


def test_under_threshold_reports_usage_without_warning():
    lines, fits = check_firmware_size(firmware_size=1_757_408, slot_size=1_966_080, warn_percent=95)
    assert fits
    assert lines == ["Firmware: 1,757,408 B of 1,966,080 B OTA slot (89.4%)"]


def test_over_threshold_warns_but_fits():
    lines, fits = check_firmware_size(firmware_size=1_900_000, slot_size=1_966_080, warn_percent=95)
    assert fits
    assert lines[0] == "Firmware: 1,900,000 B of 1,966,080 B OTA slot (96.6%)"
    assert lines[1].startswith("WARNING: firmware is above 95% of its OTA slot")


def test_exactly_full_slot_fits():
    _, fits = check_firmware_size(firmware_size=1_966_080, slot_size=1_966_080, warn_percent=95)
    assert fits


def test_over_slot_does_not_fit():
    lines, fits = check_firmware_size(firmware_size=1_966_081, slot_size=1_966_080, warn_percent=95)
    assert not fits
    assert lines[1].startswith("ERROR: firmware does not fit in its OTA slot")
