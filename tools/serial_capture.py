# ABOUTME: Resets the Inkplate over its USB serial adapter and captures boot output for a fixed time,
# ABOUTME: for non-interactive use where ./build.sh monitor (which needs a TTY) exits immediately.
"""
Usage:
    # Reset the board and capture 40s of a normal weather cycle:
    python3 tools/serial_capture.py --seconds 40

    # Capture a spike, stopping as soon as it prints its end marker; keep the full log:
    python3 tools/serial_capture.py --seconds 120 --until SPIKE-END --log /tmp/spike.log

Runs on the host (pyserial), not in the build container. Opening the port pulses RTS, which resets
the ESP32 via the CH340's auto-reset wiring; DTR is held low so it boots the app rather than the
ROM bootloader. Output is cleaned of the NUL/garbage bytes the ESP32 emits at reset and of
carriage returns, then printed. Exits 1 if --until was given and the marker never appeared.
"""

import argparse
import sys
import time
from pathlib import Path

DEFAULT_PORT = "/dev/ttyUSB0"
BAUD_RATE = 115200


def clean_serial_text(raw):
    """Decode raw serial bytes, dropping reset-time garbage bytes and carriage returns."""
    text = raw.decode("utf-8", errors="replace")
    return text.replace("\x00", "").replace("�", "").replace("\r", "")


def reached_marker(text, marker):
    return marker is not None and any(line.strip() == marker for line in text.splitlines())


def capture(port, seconds, marker):
    import serial  # imported here so the pure functions above are testable without pyserial

    with serial.Serial(port, BAUD_RATE, timeout=1) as conn:
        conn.dtr = False
        conn.rts = True
        time.sleep(0.1)
        conn.rts = False

        raw = bytearray()
        deadline = time.time() + seconds
        while time.time() < deadline:
            raw += conn.read(512)
            if reached_marker(clean_serial_text(bytes(raw)), marker):
                break
    return clean_serial_text(bytes(raw))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", default=DEFAULT_PORT, help=f"serial device (default: {DEFAULT_PORT})")
    parser.add_argument("--seconds", type=float, required=True, help="maximum capture time")
    parser.add_argument("--until", metavar="MARKER", help="stop early once a line equal to MARKER appears")
    parser.add_argument("--log", type=Path, help="also write the cleaned capture to this file")
    args = parser.parse_args()

    try:
        text = capture(args.port, args.seconds, args.until)
    except OSError as e:
        print(f"ERROR: cannot open {args.port}: {e}", file=sys.stderr)
        return 2

    print(text, end="" if text.endswith("\n") else "\n")
    if args.log:
        args.log.write_text(text)
    if args.until and not reached_marker(text, args.until):
        print(f"ERROR: marker {args.until!r} not seen within {args.seconds:g}s", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
