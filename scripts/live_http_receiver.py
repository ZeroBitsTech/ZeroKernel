#!/usr/bin/env python3
import argparse
import json
import signal
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class State:
    def __init__(self, status_file: Path) -> None:
        self._lock = threading.RLock()
        self.status_file = status_file
        self.started_at = time.time()
        self.requests = 0
        self.invalid = 0
        self.last_payload = {}
        self.last_path = ""
        self._flush()

    def record(self, path: str, payload: dict) -> None:
        with self._lock:
            self.requests += 1
            self.last_path = path
            self.last_payload = payload
            self._flush()

    def record_invalid(self) -> None:
        with self._lock:
            self.invalid += 1
            self._flush()

    def snapshot(self) -> dict:
        with self._lock:
            uptime_ms = int((time.time() - self.started_at) * 1000.0)
            return {
                "online": True,
                "uptime_ms": uptime_ms,
                "requests": self.requests,
                "invalid": self.invalid,
                "last_path": self.last_path,
                "last_payload": self.last_payload,
            }

    def _flush(self) -> None:
        payload = {
            "online": True,
            "uptime_ms": int((time.time() - self.started_at) * 1000.0),
            "requests": self.requests,
            "invalid": self.invalid,
            "last_path": self.last_path,
            "last_payload": self.last_payload,
        }
        self.status_file.parent.mkdir(parents=True, exist_ok=True)
        self.status_file.write_text(json.dumps(payload, indent=2), encoding="utf-8")


class Handler(BaseHTTPRequestHandler):
    state: State = None  # type: ignore[assignment]
    protocol_version = "HTTP/1.0"

    def _write_json(self, code: int, payload: dict) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if self.path == "/api/status":
            self._write_json(200, self.state.snapshot())
            return
        self._write_json(404, {"ok": False, "error": "not_found"})

    def do_POST(self) -> None:
        if self.path != "/api/data":
            self._write_json(404, {"ok": False, "error": "not_found"})
            return

        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length)
        try:
            payload = json.loads(raw.decode("utf-8"))
        except Exception:
            self.state.record_invalid()
            self._write_json(400, {"ok": False, "error": "invalid_json"})
            return

        self.state.record(self.path, payload)
        self._write_json(200, {"ok": True, "received": self.state.requests})

    def log_message(self, fmt: str, *args) -> None:
        sys.stdout.write("%s - - [%s] %s\n" % (self.address_string(), self.log_date_time_string(), fmt % args))
        sys.stdout.flush()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=3000)
    parser.add_argument("--status-file", required=True)
    args = parser.parse_args()

    status_file = Path(args.status_file)
    state = State(status_file)
    Handler.state = state
    server = ThreadingHTTPServer((args.host, args.port), Handler)

    def shutdown_handler(signum, frame):  # type: ignore[unused-argument]
        server.shutdown()

    signal.signal(signal.SIGTERM, shutdown_handler)
    signal.signal(signal.SIGINT, shutdown_handler)

    try:
        server.serve_forever()
    finally:
        server.server_close()
        state._flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
