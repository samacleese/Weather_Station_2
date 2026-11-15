# ABOUTME: Breaks the firmware image down by what uses it (fonts, cat pictures, WiFi, TLS, ...) by
# ABOUTME: parsing the linker map, and reports each share of the image and of its OTA slot.
"""
Usage:
    python3 tools/firmware_size_report.py [--items N]
        [--map build/WeatherStation/Weather_Station_2.ino.map]
        [--firmware build/WeatherStation/Weather_Station_2.ino.bin]
        [--partitions build/WeatherStation/Weather_Station_2.ino.partitions.bin]

Run after `./build.sh build` when tools/check_firmware_size.py warns that the image is growing, to
see where the bytes went before deciding what to shrink. --items N lists the N largest contributors
under each category: font faces, individual cat pictures, libraries, or our own source files.

Only output sections that are written into the .bin are counted (code, read-only data and
initialised RAM data, never .bss or debug info). The map lists mergeable string and constant
sections at their size before the linker deduplicates them, so those are not attributed to their
object; what the linker kept of them, plus alignment fill, is reported per output section as
"Merged strings and fill". The remainder of the .bin is the image and segment headers.

Fonts are recognised by fontconvert's Family_Style_Size naming (Roboto_Light_42Bitmaps,
Roboto_Light_42Glyphs, Roboto_Light_42); cat pictures by the KittyPics namespace.
"""

import argparse
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

from check_firmware_size import SizeCheckError, ota_slot_size

BUILD_DIR = Path("build/WeatherStation")

# The ESP32 output sections esptool writes into the app image. Everything else in the map (.bss,
# noinit, .rodata_noload, debug info) takes up RAM or nothing at all, never flash.
IMAGE_SECTIONS = {
    ".iram0.vectors",
    ".iram0.text",
    ".dram0.data",
    ".flash.appdesc",
    ".flash.rodata",
    ".flash.text",
    ".rtc.text",
    ".rtc.data",
    ".rtc.force_fast",
    ".rtc.force_slow",
}

MERGED = "Merged strings and fill"
HEADERS = "Image headers and padding"

LIBRARY_CATEGORIES = {
    "WiFi driver": {"libnet80211", "libpp", "libphy", "libwpa_supplicant", "libcoexist", "libesp_wifi", "libbt"},
    "lwIP TCP/IP": {"liblwip"},
    "TLS/crypto (mbedTLS)": {"libmbedcrypto", "libmbedtls", "libmbedtls_2", "libmbedx509"},
    "C/C++ runtime": {"libc", "libm", "libgcc", "libnewlib", "libstdc++", "libsupc++"},
}

MAP_START = "Linker script and memory map"
OUTPUT_HEADER = re.compile(r"^(\.\S+)(?:\s+0x[0-9a-f]+\s+0x([0-9a-f]+))?")
INPUT_ONE_LINE = re.compile(r"^ (\.\S+|COMMON)\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s+(\S.*)$")
INPUT_NAME_ONLY = re.compile(r"^ (\.\S+|COMMON)$")
WRAPPED_HEADER_OR_INPUT = re.compile(r"^\s+0x[0-9a-f]+\s+0x([0-9a-f]+)(?:\s+(\S.*))?$")
MERGEABLE = re.compile(r"\.(str|cst)\d+(\.|$)")
FONT = re.compile(r"^([A-Za-z]+_[A-Za-z]+_\d+)(?:Bitmaps|Glyphs)?$")
LOCAL_SYMBOL = re.compile(r"_ZL(\d+)(\w+)$")
CAT_PICTURE = re.compile(r"_ZN9KittyPics(\d+)(\w+)E$")
ARCHIVE = re.compile(r"/(lib[\w+-]+)\.a\(")


class ReportError(Exception):
    pass


@dataclass(frozen=True)
class InputSection:
    output: str
    name: str
    size: int
    obj: str


@dataclass
class MapLayout:
    output_sizes: dict = field(default_factory=dict)
    inputs: list = field(default_factory=list)


def parse_map(text):
    """Read output section sizes and every input section placed in them from a GNU ld map file."""
    lines = text.splitlines()
    try:
        start = lines.index(MAP_START) + 1
    except ValueError:
        raise ReportError(f"no '{MAP_START}' section; is this a GNU ld map file?")

    layout = MapLayout()
    output = None
    pending = None  # (kind, name) of a section whose size wrapped onto the next line
    for line in lines[start:]:
        if pending:
            kind, name = pending
            pending = None
            wrapped = WRAPPED_HEADER_OR_INPUT.match(line)
            if wrapped and kind == "output":
                layout.output_sizes[name] = int(wrapped.group(1), 16)
                continue
            if wrapped and kind == "input" and wrapped.group(2):
                layout.inputs.append(InputSection(output, name, int(wrapped.group(1), 16), wrapped.group(2)))
                continue
        if line.startswith("."):
            header = OUTPUT_HEADER.match(line)
            output = header.group(1)
            if header.group(2):
                layout.output_sizes[output] = int(header.group(2), 16)
            else:
                pending = ("output", output)
            continue
        if output is None:
            continue
        one_line = INPUT_ONE_LINE.match(line)
        if one_line:
            layout.inputs.append(InputSection(output, one_line.group(1), int(one_line.group(2), 16), one_line.group(3)))
            continue
        name_only = INPUT_NAME_ONLY.match(line)
        if name_only:
            pending = ("input", name_only.group(1))
    return layout


def _length_prefixed(match):
    """Decode an Itanium-mangled <length><identifier> pair, e.g. 3samE -> sam."""
    length, rest = int(match.group(1)), match.group(2)
    return rest[:length]


def classify(section, obj):
    """Return (category, item) for one input section of the image."""
    local = LOCAL_SYMBOL.search(section)
    if local:
        font = FONT.match(_length_prefixed(local))
        if font:
            return "Fonts", font.group(1)
    cat = CAT_PICTURE.search(section)
    if cat:
        return "Cat pictures", _length_prefixed(cat)

    library = re.search(r"/libraries/([^/]+)/", obj)
    if library:
        name = library.group(1)
        return ("Inkplate library" if name == "InkplateLibrary" else "Arduino libraries"), name
    sketch = re.search(r"/sketch/(.+)\.o$", obj)
    if sketch:
        source = sketch.group(1)
        # arduino-cli compiles the .ino as Weather_Station_2.ino.cpp; name it after the file we edit.
        return "Our code", source.removesuffix(".cpp") if source.endswith(".ino.cpp") else source
    if "/core/" in obj:
        return "Arduino ESP32 core", "core"
    archive = ARCHIVE.search(obj)
    if archive:
        name = archive.group(1)
        for category, members in LIBRARY_CATEGORIES.items():
            if name in members:
                return category, name
        return "ESP-IDF system", name
    return "Other", obj


def summarize(layout):
    """Return {category: {item: bytes}} for every byte of the image's output sections."""
    summary = defaultdict(lambda: defaultdict(int))
    attributed = defaultdict(int)
    for section in layout.inputs:
        if section.output not in IMAGE_SECTIONS or MERGEABLE.search(section.name):
            continue
        category, item = classify(section.name, section.obj)
        summary[category][item] += section.size
        attributed[section.output] += section.size
    for output, size in layout.output_sizes.items():
        if output in IMAGE_SECTIONS and size > attributed[output]:
            summary[MERGED][output] = size - attributed[output]
    return {category: dict(items) for category, items in summary.items()}


def _row(size, slot_size, image_size, label):
    return f"{size:>10,}  {100 * size / slot_size:5.1f}%  {100 * size / image_size:5.1f}%  {label}"


def format_report(summary, image_size, slot_size, items):
    """Return report lines: categories largest first, each with up to `items` of its contributors."""
    totals = {category: sum(contents.values()) for category, contents in summary.items()}
    headers = image_size - sum(totals.values())
    if headers < 0:
        raise ReportError("map accounts for more bytes than the firmware has; is the map stale? rebuild first")

    lines = [
        f"Firmware image: {image_size:,} B of {slot_size:,} B OTA slot ({100 * image_size / slot_size:.1f}%)",
        "",
        f"{'Bytes':>10}  {'Slot':>6}  {'Image':>6}  Category",
    ]
    for category, total in sorted(totals.items(), key=lambda kv: -kv[1]):
        lines.append(_row(total, slot_size, image_size, category))
        if items:
            contents = sorted(summary[category].items(), key=lambda kv: -kv[1])
            for item, size in contents[:items]:
                lines.append(_row(size, slot_size, image_size, f"    {item}"))
            if len(contents) > items:
                lines.append(f"{'':>30}    ({len(contents) - items} more)")
    lines.append(_row(headers, slot_size, image_size, HEADERS))
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--map", type=Path, default=BUILD_DIR / "Weather_Station_2.ino.map", help="linker map file")
    parser.add_argument("--firmware", type=Path, default=BUILD_DIR / "Weather_Station_2.ino.bin", help="firmware .bin")
    parser.add_argument(
        "--partitions", type=Path, default=BUILD_DIR / "Weather_Station_2.ino.partitions.bin", help="partition table"
    )
    parser.add_argument("--items", type=int, default=0, help="list the N largest contributors per category")
    args = parser.parse_args()

    try:
        summary = summarize(parse_map(args.map.read_text()))
        lines = format_report(
            summary,
            image_size=args.firmware.stat().st_size,
            slot_size=ota_slot_size(args.partitions.read_bytes()),
            items=args.items,
        )
    except (OSError, ReportError, SizeCheckError) as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
