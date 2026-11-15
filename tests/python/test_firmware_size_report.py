# ABOUTME: Tests for tools/firmware_size_report.py: parsing a GNU ld map file and attributing the
# ABOUTME: firmware image's bytes to fonts, cat pictures, and the libraries that make up the rest.
import pytest

from firmware_size_report import (
    MERGED,
    InputSection,
    ReportError,
    classify,
    format_report,
    parse_map,
    summarize,
)

SKETCH = "/build/Weather_Station_2/build/WeatherStation/sketch"
IDF = "/root/.arduino15/packages/soldered-inkplate-boards/tools/esp32-arduino-libs/idf-release_v5.5/esp32/lib"

# Shaped like the real Weather_Station_2.ino.map: long input section names wrap onto a second line,
# short ones share a line with their address, size and object.
MAP = f"""\
Archive member included to satisfy reference by file (symbol)

Discarded input sections

 .rodata._ZL22Roboto_Light_12Bitmaps
                0x00000000      0x999 {SKETCH}/Weather_Station_2.ino.cpp.o

Memory Configuration

Linker script and memory map

.iram0.text     0x40080404       0x40
 *(.iram1 .iram1.*)
 .iram1.0       0x40080404       0x40 {IDF}/libfreertos.a(tasks.c.obj)

.dram0.bss      0x3ffbdb60     0x1000
 .bss.display   0x3ffbdb60     0x1000 {SKETCH}/Weather_Station_2.ino.cpp.o

.flash.rodata   0x3f400120      0x200
 *(.rodata .rodata.*)
 .rodata._ZL23Roboto_Medium_125Glyphs
                0x3f400120       0x20 {SKETCH}/Weather_Station_2.ino.cpp.o
 .rodata._ZL24Roboto_Medium_125Bitmaps
                0x3f400140       0x80 {SKETCH}/Weather_Station_2.ino.cpp.o
                0x3f400140                _ZL24Roboto_Medium_125Bitmaps
 .rodata._ZN9KittyPics3samE
                0x3f4001c0       0x60 {SKETCH}/src/display/KittyPics.cpp.o
 .rodata.setup.str1.1
                0x3f400220      0x400 {SKETCH}/Weather_Station_2.ino.cpp.o
 *fill*         0x3f400220        0x4

.flash.text     0x400d0020      0x100
 .text.esp_wifi_init
                0x400d0020       0x90 {IDF}/libnet80211.a(ieee80211.o)
 .text          0x400d00b0       0x30 {SKETCH}/../libraries/InkplateLibrary/src/Inkplate.cpp.o

.debug_info     0x00000000     0x5000
 .debug_info    0x00000000     0x5000 {SKETCH}/Weather_Station_2.ino.cpp.o
"""


def test_parse_map_reads_output_section_sizes_from_the_memory_map_only():
    layout = parse_map(MAP)
    assert layout.output_sizes[".flash.rodata"] == 0x200
    assert layout.output_sizes[".flash.text"] == 0x100
    assert layout.output_sizes[".dram0.bss"] == 0x1000


def test_parse_map_reads_wrapped_and_single_line_input_sections():
    inputs = parse_map(MAP).inputs
    assert InputSection(".flash.rodata", ".rodata._ZL24Roboto_Medium_125Bitmaps", 0x80,
                        f"{SKETCH}/Weather_Station_2.ino.cpp.o") in inputs
    assert InputSection(".iram0.text", ".iram1.0", 0x40, f"{IDF}/libfreertos.a(tasks.c.obj)") in inputs


def test_parse_map_ignores_discarded_sections_and_fill():
    names = [i.name for i in parse_map(MAP).inputs]
    assert ".rodata._ZL22Roboto_Light_12Bitmaps" not in names
    assert "*fill*" not in names


def test_parse_map_rejects_text_without_a_memory_map():
    with pytest.raises(ReportError, match="memory map"):
        parse_map("not a map file\n")


@pytest.mark.parametrize(
    "section, obj, expected",
    [
        (".rodata._ZL24Roboto_Medium_125Bitmaps", f"{SKETCH}/Weather_Station_2.ino.cpp.o",
         ("Fonts", "Roboto_Medium_125")),
        (".rodata._ZL23Roboto_Light_14Glyphs", f"{SKETCH}/Weather_Station_2.ino.cpp.o",
         ("Fonts", "Roboto_Light_14")),
        (".rodata._ZL17Roboto_Medium_125", f"{SKETCH}/Weather_Station_2.ino.cpp.o", ("Fonts", "Roboto_Medium_125")),
        (".rodata._ZN9KittyPics13thundercleeseE", f"{SKETCH}/src/display/KittyPics.cpp.o",
         ("Cat pictures", "thundercleese")),
        (".text.setup", f"{SKETCH}/Weather_Station_2.ino.cpp.o", ("Our code", "Weather_Station_2.ino")),
        (".text._ZN7Kitties12getNextKittyEv", f"{SKETCH}/src/display/Kitties.cpp.o",
         ("Our code", "src/display/Kitties.cpp")),
        (".text", f"{SKETCH}/../libraries/InkplateLibrary/src/Inkplate.cpp.o", ("Inkplate library", "InkplateLibrary")),
        (".text", "/b/libraries/HTTPClient/HTTPClient.cpp.o", ("Arduino libraries", "HTTPClient")),
        (".text", "/b/core/core.a(main.cpp.o)", ("Arduino ESP32 core", "core")),
        (".text.x", f"{IDF}/libnet80211.a(ieee80211.o)", ("WiFi driver", "libnet80211")),
        (".text.x", f"{IDF}/../ld/libphy.a(phy_chip_v7.o)", ("WiFi driver", "libphy")),
        (".text.x", f"{IDF}/liblwip.a(tcp.c.obj)", ("lwIP TCP/IP", "liblwip")),
        (".text.x", f"{IDF}/libmbedcrypto.a(aes.c.obj)", ("TLS/crypto (mbedTLS)", "libmbedcrypto")),
        (".text.x", "/x/no-rtti/libc.a(libc_a-vfprintf.o)", ("C/C++ runtime", "libc")),
        (".text.x", f"{IDF}/libfreertos.a(tasks.c.obj)", ("ESP-IDF system", "libfreertos")),
        (".text.x", "/tmp/linker-stub.o", ("Other", "/tmp/linker-stub.o")),
    ],
)
def test_classify(section, obj, expected):
    assert classify(section, obj) == expected


def test_summarize_counts_only_image_sections_and_attributes_merged_strings_as_a_residual():
    summary = summarize(parse_map(MAP))
    assert summary["Fonts"] == {"Roboto_Medium_125": 0xA0}
    assert summary["Cat pictures"] == {"sam": 0x60}
    assert summary["WiFi driver"] == {"libnet80211": 0x90}
    assert summary["Inkplate library"] == {"InkplateLibrary": 0x30}
    assert summary["ESP-IDF system"] == {"libfreertos": 0x40}
    # The .str1.1 input is listed at its pre-merge size (0x400), more than the whole output section.
    # What the linker actually kept is the output size minus every non-mergeable input.
    assert summary[MERGED] == {".flash.rodata": 0x200 - 0xA0 - 0x60, ".flash.text": 0x100 - 0x90 - 0x30}
    assert "Our code" not in summary  # the sketch's only non-font input here is the merged string
    total = sum(sum(items.values()) for items in summary.values())
    assert total == 0x40 + 0x200 + 0x100  # .iram0.text + .flash.rodata + .flash.text; no bss or debug


def test_format_report_lists_categories_largest_first_with_slot_share_and_header_overhead():
    summary = {"Fonts": {"Roboto_Medium_125": 600, "Roboto_Light_14": 100}, "Cat pictures": {"sam": 900}}
    lines = format_report(summary, image_size=1700, slot_size=2000, items=0)
    assert lines[0] == "Firmware image: 1,700 B of 2,000 B OTA slot (85.0%)"
    body = [line for line in lines[1:] if line.strip()]
    assert body[0].split() == ["Bytes", "Slot", "Image", "Category"]
    assert body[1].split() == ["900", "45.0%", "52.9%", "Cat", "pictures"]
    assert body[2].split() == ["700", "35.0%", "41.2%", "Fonts"]
    assert body[3].split() == ["100", "5.0%", "5.9%", "Image", "headers", "and", "padding"]


def test_format_report_shows_the_largest_items_under_each_category_when_asked():
    summary = {"Fonts": {"Roboto_Medium_125": 600, "Roboto_Light_14": 100, "Roboto_Light_12": 50}}
    lines = format_report(summary, image_size=750, slot_size=1000, items=2)
    fonts = lines.index(next(line for line in lines if line.endswith("Fonts")))
    assert lines[fonts + 1].split() == ["600", "60.0%", "80.0%", "Roboto_Medium_125"]
    assert lines[fonts + 2].split() == ["100", "10.0%", "13.3%", "Roboto_Light_14"]
    assert lines[fonts + 3].split() == ["(1", "more)"]


def test_format_report_rejects_a_map_larger_than_the_image():
    with pytest.raises(ReportError, match="stale"):
        format_report({"Fonts": {"x": 2000}}, image_size=1000, slot_size=4000, items=0)
