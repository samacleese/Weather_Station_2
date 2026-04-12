# ABOUTME: Validates and displays the battery voltage log stored on the local JSON store.
# ABOUTME: Run after flashing to confirm the board is posting data correctly.
"""
Usage:
    python3 tools/validate_battery_log.py [--host 192.168.1.2] [--port 5000]

Fetches the weather-station battery log from json-store and prints a summary.
"""

import argparse
import json
import sys
import urllib.request
from datetime import datetime, timezone


def main():
    parser = argparse.ArgumentParser(description="Validate battery voltage log")
    parser.add_argument("--host", default="192.168.1.2")
    parser.add_argument("--port", default=5000, type=int)
    args = parser.parse_args()

    url = f"http://{args.host}:{args.port}/weather-station/data"

    try:
        with urllib.request.urlopen(url, timeout=5) as resp:
            data = json.loads(resp.read())
    except urllib.error.HTTPError as e:
        if e.code == 404:
            print("No data yet — board hasn't posted a reading.")
        else:
            print(f"HTTP error: {e.code}")
        sys.exit(1)
    except Exception as e:
        print(f"Connection failed: {e}")
        sys.exit(1)

    if not isinstance(data, list):
        print(f"ERROR: expected JSON array, got {type(data).__name__}")
        sys.exit(1)

    print(f"Entries stored: {len(data)} / 500")
    print()

    required_keys = {"ts", "raw", "adj"}
    bad = [i for i, e in enumerate(data) if not required_keys.issubset(e.keys())]
    if bad:
        print(f"WARNING: entries missing required keys at indices: {bad}")

    if not data:
        print("Array is empty.")
        return

    print(f"{'Timestamp (UTC)':<25} {'Raw (V)':>8} {'Adj (V)':>8}")
    print("-" * 45)
    for entry in data[-20:]:  # show last 20
        ts = datetime.fromtimestamp(entry["ts"], tz=timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
        print(f"{ts:<25} {entry['raw']:>8.3f} {entry['adj']:>8.3f}")

    if len(data) > 20:
        print(f"  ... ({len(data) - 20} earlier entries not shown)")


if __name__ == "__main__":
    main()
