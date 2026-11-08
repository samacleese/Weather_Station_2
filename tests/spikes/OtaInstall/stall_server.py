#!/usr/bin/env python3
# ABOUTME: Throwaway HTTP server for the OtaInstall spike (#59): answers every GET, then stalls mid-body.
# ABOUTME: Proves HttpFirmwareSource's 30s read timeout turns a silent server into a failed install.
"""Stalling firmware server for the OtaInstall spike's timeout case.

Run it on the dev machine (not in the build container) before flashing the spike:

    python3 tests/spikes/OtaInstall/stall_server.py --port 8000

Then set OTA_STALL_URL in config.cmake to http://<this machine's LAN IP>:8000/firmware and re-run
./build.sh configure. Each GET gets 200 headers advertising --content-length bytes, --send-bytes of
body, then nothing for --hold-seconds before the connection closes. If the device can't connect, check
that the host firewall allows the port.
"""
import argparse
import http.server
import time


def make_handler(content_length, send_bytes, hold_seconds):
    class StallHandler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(content_length))
            self.end_headers()
            self.wfile.write(bytes(send_bytes))
            self.wfile.flush()
            client = self.client_address[0]
            print(f"{client}: sent {send_bytes} of {content_length} bytes, holding {hold_seconds}s", flush=True)
            time.sleep(hold_seconds)
            print(f"{client}: closing", flush=True)

    return StallHandler


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=8000, help="port to listen on (default: 8000)")
    parser.add_argument("--content-length", type=int, default=1758455, help="advertised body size")
    parser.add_argument(
        "--send-bytes",
        type=int,
        default=65536,
        help="body bytes sent before stalling; 64KB lines up with the installer's first progress report",
    )
    parser.add_argument("--hold-seconds", type=float, default=120, help="silence before closing")
    args = parser.parse_args()

    server = http.server.HTTPServer(
        ("0.0.0.0", args.port), make_handler(args.content_length, args.send_bytes, args.hold_seconds)
    )
    print(f"Stall server listening on port {args.port}; Ctrl-C to stop", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
