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
from urllib.parse import urlparse, parse_qs

# Import sibling modules regardless of the process working directory.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from water_stress import decide  # noqa: E402
from canopy import analyze as analyze_canopy  # noqa: E402
from disease import classify as classify_disease  # noqa: E402

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
        route = urlparse(self.path).path.rstrip("/")
        length = int(self.headers.get("Content-Length", 0) or 0)
        body = self.rfile.read(length) if length else b""
        try:
            if route == "/water-stress":
                # JSON body: { inputs, thresholds } -> { band, risk, factors }
                payload = json.loads(body or b"{}")
                self._send(200, decide(payload.get("inputs"), payload.get("thresholds")))
            elif route == "/canopy":
                # Raw JPEG body + HSV params as query string -> canopy result.
                if not body:
                    self._send(400, {"error": "empty image body"})
                    return
                q = parse_qs(urlparse(self.path).query)
                params = {k: v[0] for k, v in q.items()}
                self._send(200, analyze_canopy(body, params))
            elif route == "/disease":
                # Raw JPEG body -> PlantVillage top-k (lazy torch load).
                if not body:
                    self._send(400, {"error": "empty image body"})
                    return
                self._send(200, classify_disease(body))
            else:
                self._send(404, {"error": "not found"})
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
