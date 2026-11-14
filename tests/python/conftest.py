# ABOUTME: Makes tools/*.py importable as modules, and provides a real local HTTP server that records PUTs.
# ABOUTME: The scripts live in tools/ as standalone CLI entry points, not a package.
import http.server
import sys
import threading
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / "tools"))


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
