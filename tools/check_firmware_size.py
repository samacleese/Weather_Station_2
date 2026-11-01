# ABOUTME: Post-build check that the firmware .bin fits its OTA slot, read from the build's own
# ABOUTME: compiled partition table; warns as headroom erodes and fails the build on overflow.
"""
Usage:
    python3 tools/check_firmware_size.py \\
        --firmware build/WeatherStation/Weather_Station_2.ino.bin \\
        --partitions build/WeatherStation/Weather_Station_2.ino.partitions.bin \\
        --warn-percent 95

Run automatically by the WeatherStation CMake target after every compile. An OTA update writes
the .bin into the ota_1 partition, so that partition's size is the real limit; arduino-cli's own
"Sketch uses ..." line measures code sections against boards.txt instead. A warning means it is
time for the size-reduction work in the OTA roadmap's Track E. Exits 1 if the firmware does not
fit, and 2 on unreadable input.
"""

import argparse
import struct
import sys
from pathlib import Path

PARTITION_ENTRY = struct.Struct("<HBBII16sI")  # magic, type, subtype, offset, size, label, flags
PARTITION_MAGIC = 0x50AA
TYPE_APP = 0x00
SUBTYPE_OTA_1 = 0x11


class SizeCheckError(Exception):
    pass


def ota_slot_size(partition_table):
    """Return the size of the ota_1 app partition in an ESP32 binary partition table."""
    for start in range(0, len(partition_table) - PARTITION_ENTRY.size + 1, PARTITION_ENTRY.size):
        magic, type_, subtype, _offset, size, _label, _flags = PARTITION_ENTRY.unpack_from(partition_table, start)
        if magic != PARTITION_MAGIC:
            break  # MD5 entry or 0xFF padding ends the table
        if type_ == TYPE_APP and subtype == SUBTYPE_OTA_1:
            return size
    raise SizeCheckError("no ota_1 partition in partition table; is the firmware built with an OTA scheme?")


def check_firmware_size(firmware_size, slot_size, warn_percent):
    """Return (report lines, whether the firmware fits the slot)."""
    percent = 100 * firmware_size / slot_size
    lines = [f"Firmware: {firmware_size:,} B of {slot_size:,} B OTA slot ({percent:.1f}%)"]
    fits = firmware_size <= slot_size
    if not fits:
        lines.append(f"ERROR: firmware does not fit in its OTA slot (over by {firmware_size - slot_size:,} B)")
    elif percent > warn_percent:
        lines.append(
            f"WARNING: firmware is above {warn_percent:g}% of its OTA slot "
            f"({slot_size - firmware_size:,} B left); see Track E of the OTA roadmap"
        )
    return lines, fits


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--firmware", required=True, type=Path, help="compiled firmware .bin")
    parser.add_argument("--partitions", required=True, type=Path, help="compiled .partitions.bin")
    parser.add_argument("--warn-percent", required=True, type=float, help="warn above this %% of the slot")
    args = parser.parse_args()

    try:
        slot_size = ota_slot_size(args.partitions.read_bytes())
        firmware_size = args.firmware.stat().st_size
    except (OSError, SizeCheckError) as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2

    lines, fits = check_firmware_size(firmware_size, slot_size, args.warn_percent)
    for line in lines:
        print(line, file=sys.stdout if fits else sys.stderr)
    return 0 if fits else 1


if __name__ == "__main__":
    sys.exit(main())
