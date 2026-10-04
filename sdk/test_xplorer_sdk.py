"""SDK connection regression tests; no Chromium build or external services needed.

Run from the repository root: python3 -m unittest discover -s sdk -p 'test_*.py'
"""
import json
import os
import pathlib
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, HTTPServer
from unittest.mock import patch

from xplorer_sdk import Browser


class BrowserDiscoveryTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.home = pathlib.Path(temp.name)
        home_patch = patch("pathlib.Path.home", return_value=self.home)
        home_patch.start()
        self.addCleanup(home_patch.stop)
        env_patch = patch.dict(os.environ, {}, clear=True)
        env_patch.start()
        self.addCleanup(env_patch.stop)

    def write_gateway(self, data=None, directory=".xplorer"):
        path = self.home / directory / "gateway.json"
        path.parent.mkdir(exist_ok=True)
        if data is None:
            data = {"url": "http://127.0.0.1:19434", "token": "discovered-token"}
        path.write_text(json.dumps(data), encoding="utf-8")
        return path

    def test_connects_to_discovered_gateway_on_nondefault_port(self):
        received = []

        class Gateway(BaseHTTPRequestHandler):
            def do_GET(self):
                received.append((self.path, self.headers.get("Authorization")))
                body = json.dumps({"tabs": [{"id": "12:0"}]}).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *_):
                pass

        server = HTTPServer(("127.0.0.1", 0), Gateway)
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            self.write_gateway({
                "url": f"http://127.0.0.1:{server.server_port}",
                "token": "discovered-token",
            })
            self.assertEqual(Browser().tabs(), [{"id": "12:0"}])
            self.assertEqual(received, [("/tabs", "Bearer discovered-token")])
        finally:
            server.shutdown()
            server.server_close()
            worker.join()

    def test_explicit_token_overrides_environment_and_discovery(self):
        self.write_gateway()
        os.environ["XPLORER_TOKEN"] = "environment-token"
        browser = Browser(token="explicit-token")
        self.assertEqual(browser.token, "explicit-token")
        self.assertEqual(browser.base, "http://127.0.0.1:19434")

    def test_environment_token_overrides_discovery(self):
        self.write_gateway()
        os.environ["XPLORER_TOKEN"] = "environment-token"
        browser = Browser()
        self.assertEqual(browser.token, "environment-token")
        self.assertEqual(browser.base, "http://127.0.0.1:19434")

    def test_empty_environment_token_uses_discovery(self):
        self.write_gateway()
        os.environ["XPLORER_TOKEN"] = ""
        self.assertEqual(Browser().token, "discovered-token")

    def test_explicit_port_overrides_discovery(self):
        self.write_gateway()
        browser = Browser(19435)
        self.assertEqual(browser.base, "http://127.0.0.1:19435")
        self.assertEqual(browser.token, "discovered-token")

    def test_explicit_connection_needs_no_discovery(self):
        browser = Browser(19435, "explicit-token")
        self.assertEqual(browser.base, "http://127.0.0.1:19435")
        self.assertEqual(browser.token, "explicit-token")

    def test_environment_token_and_explicit_port_need_no_discovery(self):
        os.environ["XPLORER_TOKEN"] = "environment-token"
        browser = Browser(19435)
        self.assertEqual(browser.base, "http://127.0.0.1:19435")
        self.assertEqual(browser.token, "environment-token")

    def test_manual_token_without_discovery_keeps_default_port(self):
        self.assertEqual(Browser(token="explicit-token").base,
                         "http://127.0.0.1:9334")
        os.environ["XPLORER_TOKEN"] = "environment-token"
        self.assertEqual(Browser().base, "http://127.0.0.1:9334")

    def test_legacy_discovery_location(self):
        self.write_gateway(directory=".xbrowser")
        browser = Browser()
        self.assertEqual(browser.base, "http://127.0.0.1:19434")
        self.assertEqual(browser.token, "discovered-token")

    def test_current_discovery_takes_precedence_over_legacy(self):
        self.write_gateway(directory=".xbrowser")
        self.write_gateway({"url": "http://127.0.0.1:19436", "token": "current"})
        browser = Browser()
        self.assertEqual(browser.base, "http://127.0.0.1:19436")
        self.assertEqual(browser.token, "current")

    def test_missing_discovery_has_actionable_error(self):
        with self.assertRaisesRegex(RuntimeError, r"gateway\.json.*Launch"):
            Browser()

    def test_malformed_discovery_has_actionable_error(self):
        path = self.write_gateway()
        for content in ("{", "\ufffd"):
            with self.subTest(content=content):
                path.write_text(content, encoding="utf-8")
                with self.assertRaisesRegex(RuntimeError, "gateway.json.*Restart"):
                    Browser()

    def test_invalid_discovery_fields_have_actionable_error(self):
        for data in ([], {}, {"url": 9334, "token": "secret"},
                     {"url": "", "token": "secret"},
                     {"url": "http://127.0.0.1:19434", "token": None},
                     {"url": "http://127.0.0.1:19434", "token": ""}):
            with self.subTest(data=data):
                self.write_gateway(data)
                with self.assertRaisesRegex(RuntimeError, "gateway.json.*Restart"):
                    Browser()

    def test_unreadable_discovery_has_actionable_error(self):
        with patch.object(pathlib.Path, "read_text", side_effect=PermissionError):
            with self.assertRaisesRegex(RuntimeError, "gateway.json"):
                Browser()

    def test_manual_connection_ignores_broken_discovery(self):
        self.write_gateway().write_text("{", encoding="utf-8")
        browser = Browser(19435, "explicit-token")
        self.assertEqual(browser.base, "http://127.0.0.1:19435")
        self.assertEqual(browser.token, "explicit-token")


if __name__ == "__main__":
    unittest.main()
