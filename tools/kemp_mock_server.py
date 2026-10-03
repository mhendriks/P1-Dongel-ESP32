#!/usr/bin/env python3
"""Small local KEMP protocol mock server.

Run on a MacBook on the same Wi-Fi network as the dongle:
  python3 tools/kemp_mock_server.py

Open http://<macbook-ip>:8080/ in a browser.  The selected command is returned
once to the next POST from the dongle, then the server returns only interval_s.
This prevents an accidental reboot or update loop during testing.
"""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from html import escape
import json
from threading import Lock
from urllib.parse import parse_qs, urlparse


class State:
    interval_s = 10
    next_response = None
    last_request = "No request received yet."
    lock = Lock()


def normal_response():
    return {"interval_s": State.interval_s}


def queue_command(form):
    command = form.get("command", [""])[0]
    command_id = form.get("command_id", ["mock-1"])[0] or "mock-1"
    interval = form.get("interval_s", ["10"])[0]
    try:
        State.interval_s = max(1, min(3600, int(interval)))
    except ValueError:
        State.interval_s = 10

    response = normal_response()
    if command:
        response.update({"command": command, "command_id": command_id})
        if command == "APIupdate":
            response["api_key_new"] = form.get("api_key_new", ["new-test-key"])[0]
        elif command == "URLupdate":
            response["url_new"] = form.get("url_new", [""])[0]
        elif command == "OTAupdate":
            response["firmware_version"] = form.get("firmware_version", ["99.99.99"])[0]
    State.next_response = response
    return response


def page():
    with State.lock:
        queued = json.dumps(State.next_response or normal_response(), indent=2)
        last = State.last_request
    return f"""<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>KEMP mock server</title><style>body{{font-family:system-ui;margin:2rem;max-width:48rem}}label{{display:block;margin:.65rem 0}}input,select,button{{font:inherit;padding:.35rem}}pre{{white-space:pre-wrap;background:#f4f4f4;padding:1rem}}</style></head><body>
<h1>KEMP mock server</h1>
<p>Het geselecteerde command wordt precies één keer teruggestuurd bij de volgende POST van de dongle.</p>
<form method='post' action='/queue'>
<label>Command <select name='command'><option value=''>Geen command</option><option>REQconfig</option><option>APIupdate</option><option>URLupdate</option><option>OTAupdate</option><option>REBOOT</option></select></label>
<label>Command ID <input name='command_id' value='mock-1'></label>
<label>Interval (seconden) <input name='interval_s' type='number' min='1' max='3600' value='{State.interval_s}'></label>
<label>Nieuwe API-key <input name='api_key_new' value='new-test-key'></label>
<label>Nieuwe URL (alleen HTTPS) <input name='url_new' placeholder='https://192.168.1.50:8443/'></label>
<label>Firmwareversie <input name='firmware_version' value='99.99.99'></label>
<button type='submit'>Command klaarzetten</button></form>
<h2>Volgende response</h2><pre>{escape(queued)}</pre>
<h2>Laatste ontvangen POST</h2><pre>{escape(last)}</pre>
</body></html>"""


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print("HTTP", fmt % args)

    def send_json(self, value):
        body = json.dumps(value).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        body = page().encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if self.path == "/queue":
            length = int(self.headers.get("Content-Length", "0"))
            form = parse_qs(self.rfile.read(length).decode())
            with State.lock:
                response = queue_command(form)
            print("Queued one-shot response:", json.dumps(response))
            self.send_response(303)
            self.send_header("Location", "/")
            self.end_headers()
            return

        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length).decode(errors="replace")
        try:
            request = json.dumps(json.loads(raw), indent=2)
        except json.JSONDecodeError:
            request = raw
        with State.lock:
            State.last_request = request
            response = State.next_response or normal_response()
            State.next_response = None
        print("\nReceived POST:\n", request)
        print("Response:", json.dumps(response))
        self.send_json(response)


if __name__ == "__main__":
    server = ThreadingHTTPServer(("0.0.0.0", 8080), Handler)
    print("KEMP mock server listening on http://0.0.0.0:8080/")
    print("Press Ctrl-C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
