#!/usr/bin/env python3
"""smartfarm-ai HTTP service — the container's AI 'brain'.

A tiny stdlib HTTP server (no pip deps, Python 3.6 compatible) that the
web-server calls to make AI decisions. Feature 1 is Water Stress; future vision
features (canopy coverage, disease detection) will add endpoints here.

Endpoints:
  GET  /health        -> {"status":"ok", ...}
  POST /water-stress  -> body {inputs:{soilMoisture,temperature,humidity},
                                thresholds:{...}}  =>  {band, risk, factors}

Web-server owns aggregation, smoothing, history, settings; this service is a
stateless decision function. Run as the container command (see docker-compose.ai.yaml).
"""
import json
import os
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

# Import sibling modules regardless of the process working directory.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from water_stress import decide  # noqa: E402

PORT = int(os.environ.get("AI_SERVICE_PORT", "8000"))


class Handler(BaseHTTPRequestHandler):
    def _send(self, code, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path.rstrip("/") in ("", "/health"):
            self._send(200, {"status": "ok", "service": "smartfarm-ai"})
        else:
            self._send(404, {"error": "not found"})

    def do_POST(self):
        if self.path.rstrip("/") != "/water-stress":
            self._send(404, {"error": "not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", 0) or 0)
            payload = json.loads(self.rfile.read(length) or b"{}")
            result = decide(payload.get("inputs"), payload.get("thresholds"))
            self._send(200, result)
        except Exception as exc:  # noqa: BLE001 - report any parse/decision error as 400
            self._send(400, {"error": str(exc)})

    def log_message(self, *args):
        pass  # quiet; the web-server logs its own calls


def main():
    server = HTTPServer(("0.0.0.0", PORT), Handler)
    print("[smartfarm-ai] decision service listening on :%d" % PORT, flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
