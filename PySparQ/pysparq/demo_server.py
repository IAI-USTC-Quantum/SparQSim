"""Local native simulator UI with one bounded subprocess per experiment."""
from __future__ import annotations

import hmac
import json
import secrets
import subprocess
import sys
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from importlib.resources import files

from .demo import Experiment, information


def make_server(port: int = 0) -> ThreadingHTTPServer:
    if type(port) is not int or not 0 <= port <= 65535:
        raise ValueError("port must be an integer in [0,65535]")
    token = secrets.token_urlsafe(32)
    busy = threading.Lock()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def handle(self):
            try:
                super().handle()
            except ConnectionError:
                pass  # Closing a tab must not cause a second HTTP response.

        def local(self):
            host = f"127.0.0.1:{self.server.server_port}"
            return (self.headers.get("Host") == host and
                    self.headers.get("Origin", f"http://{host}") == f"http://{host}")

        def reply(self, code, body, mime="application/json"):
            data = body if isinstance(body, bytes) else json.dumps(body, allow_nan=False).encode()
            self.send_response(code)
            self.send_header("Content-Type", mime+"; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Content-Security-Policy", "default-src 'none'; connect-src 'self'; "
                             f"script-src 'nonce-{token}'; style-src 'unsafe-inline'; "
                             "frame-ancestors 'none'; base-uri 'none'")
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            if not self.local():
                return self.reply(403, {"error": "local origin required"})
            if self.path == "/":
                text = files("pysparq").joinpath("demo.html").read_text(encoding="utf-8")
                return self.reply(200, text.replace("__SESSION__", token).encode(), "text/html")
            if self.path == "/api/info":
                return self.reply(200, information())
            self.reply(404, {"error": "not found"})

        def do_POST(self):
            if not self.local() or not hmac.compare_digest(self.headers.get("X-Session", ""), token):
                return self.reply(403, {"error": "local session required"})
            if self.path != "/api/run":
                return self.reply(404, {"error": "not found"})
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= 2048 or self.headers.get("Content-Type") != "application/json":
                    raise ValueError("a JSON request of at most 2 KiB is required")
                config = Experiment(**json.loads(self.rfile.read(length)))
                config.validate()
            except (ValueError, TypeError) as exc:
                return self.reply(400, {"error": str(exc)})
            if not busy.acquire(blocking=False):
                return self.reply(409, {"error": "an experiment is already running"})
            try:
                command = [sys.executable, "-m", "pysparq.demo", "run", "--case", config.case,
                           "--qubits", str(config.qubits), "--seed", str(config.seed),
                           "--basis", str(config.basis)]
                result = subprocess.run(command, capture_output=True, text=True, timeout=60, check=False)
                if result.returncode:
                    self.reply(422, {"error": result.stderr[-2048:]})
                else:
                    self.reply(200, json.loads(result.stdout))
            except ConnectionError:
                pass
            except subprocess.TimeoutExpired:
                self.reply(504, {"error": "experiment exceeded 60 seconds"})
            except (OSError, ValueError) as exc:
                self.reply(500, {"error": str(exc)})
            finally:
                busy.release()

    return ThreadingHTTPServer(("127.0.0.1", port), Handler)


def serve(*, port=0, open_browser=True):
    with make_server(port) as server:
        url = f"http://127.0.0.1:{server.server_port}/"
        print(url, flush=True)
        if open_browser:
            webbrowser.open(url)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
