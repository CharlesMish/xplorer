#!/usr/bin/env python3
"""Exercise both typing helpers against one local fixture tab in running Xplor.

From sdk/: env -u XPLORER_TOKEN python3 text_fields_smoke.py
No extra packages. Leaves the fixture tab open for inspection; no form submits.
"""
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import threading
import time
import uuid

from agent_context import Page
from xplorer_sdk import Browser
import xplorer_mcp


CASES = (
    ("single", "single", "new café text"),
    ("multi", "multi", "first line\nsecond line"),
    ("rich", "rich", "new editable text"),
    ("wrapper", "wrapped-input", "new wrapped text"),
    ("readonly", "readonly", "must not replace"),
)


def main():
    browser = Browser()
    browser.tabs()  # Check the connection before starting the fixture server.
    owner = "text-field-check-" + uuid.uuid4().hex
    route = "/" + owner
    html = (Path(__file__).parent / "fixtures/text_fields.html").read_bytes()

    class Fixture(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path != route:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(html)))
            self.end_headers()
            self.wfile.write(html)

        def log_message(self, *_):
            pass

    server = ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    url = f"http://127.0.0.1:{server.server_port}{route}"
    try:
        browser._req("POST", "/tabs", {
            "url": url, "owner": owner, "label": "Text-field check",
        })

        def fixture_tab():
            matches = [t for t in browser.tabs()
                       if t.get("owner") == owner and t.get("url") == url]
            if len(matches) != 1:
                raise RuntimeError("Could not uniquely locate the fixture tab")
            return matches[0]["id"]

        deadline = time.monotonic() + 15
        while True:
            try:
                tab = fixture_tab()
                if browser.eval(tab, "typeof window.fieldState==='function'"):
                    break
            except RuntimeError:
                pass
            if time.monotonic() >= deadline:
                raise RuntimeError("Timed out waiting for the local fixture page")
            time.sleep(0.1)

        print("Testing one local fixture tab. Please leave tabs in place until done.")
        failures = 0
        for helper in ("Page.type", "MCP xplorer_type"):
            for target, field, replacement in CASES:
                tab = fixture_tab()  # Never assume the newest tab is last.
                browser.eval(tab, "(()=>{"
                             f"resetField({json.dumps(field)});"
                             "document.querySelectorAll('[data-aref]').forEach("
                             "e=>e.removeAttribute('data-aref'));"
                             f"document.getElementById({json.dumps(target)})"
                             ".setAttribute('data-aref','0');return true;})()")
                rejected = False
                try:
                    if helper == "Page.type":
                        Page(browser, tab).type(0, replacement)
                    else:
                        xplorer_mcp.t_type({"tab": tab, "selector": "#" + target,
                                           "text": replacement})
                except RuntimeError:
                    if field != "readonly":
                        raise
                    rejected = True
                state = browser.eval(tab,
                                     f"fieldState({json.dumps(field)})")
                # Browser.eval returns JS values by value through the gateway.
                expected = "old text" if field == "readonly" else replacement
                passed = state["value"] == expected
                if field == "readonly":
                    passed = passed and rejected and not state["events"]
                else:
                    events = state["events"]
                    passed = (passed and len(events) >= 2
                              and events[0]["value"] == ""
                              and events[-1]["value"] == expected
                              and events[-1]["trusted"])
                print(f"{'PASS' if passed else 'FAIL'} {helper}: {field}")
                failures += not passed
        print(f"{'ALL PASS' if not failures else 'FAILED'}: {10 - failures}/10 checks")
        print("The test page stays open for inspection; close that tab when finished.")
        return int(bool(failures))
    finally:
        server.shutdown()
        server.server_close()
        worker.join()


if __name__ == "__main__":
    raise SystemExit(main())
