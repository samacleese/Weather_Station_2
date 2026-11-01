# ABOUTME: Tests for tools/serial_capture.py's pure logic: cleaning raw serial bytes into text
# ABOUTME: and deciding when a capture can stop early on a marker line.
from serial_capture import clean_serial_text, reached_marker


def test_clean_strips_boot_noise_nuls_and_carriage_returns():
    raw = b"\x00\x00\xff\xfe\x00ets Jun  8 2016\r\nI: hello\r\n"
    assert clean_serial_text(raw) == "ets Jun  8 2016\nI: hello\n"


def test_clean_keeps_text_without_noise_unchanged():
    assert clean_serial_text(b"plain line\n") == "plain line\n"


def test_reached_marker_true_when_marker_line_seen():
    assert reached_marker("a\nSPIKE-END\n", "SPIKE-END")


def test_reached_marker_false_before_marker():
    assert not reached_marker("a\nSPIKE-BEGIN\n", "SPIKE-END")


def test_reached_marker_false_without_marker():
    assert not reached_marker("SPIKE-END\n", None)
