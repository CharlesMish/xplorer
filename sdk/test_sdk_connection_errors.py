"""Actionable errors without silently replaying browser actions."""
import errno
import io
import json
import threading
import unittest
from http.server import BaseHTTPRequestHandler, HTTPServer
from unittest.mock import patch
import urllib.error
import urllib.request

from xplorer_sdk import Browser


class ConnectionErrorsTest(unittest.TestCase):
    def setUp(self):
        # Keep loopback fixtures independent of machine proxy settings, and
        # restore urllib's cached opener so other test modules are unaffected.
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        self.opener_patch = patch.object(urllib.request, "_opener", opener)
        self.opener_patch.start()
        self.addCleanup(self.opener_patch.stop)

    def test_401_retains_http_error_and_response_without_echoing_reason(self):
        requests = []

        class Gateway(BaseHTTPRequestHandler):
            def do_POST(self):
                requests.append(self.path)
                body = b'{"error":"invalid bearer token"}'
                self.send_response(401, "server-secret-do-not-print")
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
            b = Browser(port=server.server_port, token="client-secret-do-not-print")
            with self.assertRaises(urllib.error.HTTPError) as raised:
                b.open("https://example.com")
            error = raised.exception
            try:
                self.assertEqual(error.code, 401)
                self.assertEqual(error.headers["Content-Type"], "application/json")
                self.assertEqual(json.load(error), {"error": "invalid bearer token"})
                self.assertIn("XPLORER_TOKEN overrides discovery", str(error))
                self.assertIn("new Browser()", str(error))
                self.assertNotIn("server-secret", str(error))
                self.assertNotIn("client-secret", str(error))
                self.assertEqual(requests, ["/tabs"])
            finally:
                error.close()
        finally:
            server.shutdown()
            server.server_close()
            worker.join()

    def test_refused_post_has_guidance_and_is_not_retried(self):
        refused = urllib.error.URLError(OSError(errno.ECONNREFUSED, "refused"))
        with patch("urllib.request.urlopen", side_effect=refused) as request:
            with self.assertRaisesRegex(urllib.error.URLError, "Start Xplor") as raised:
                Browser(port=9334, token="secret").click("12:0", "button")
            self.assertIn("not retried", str(raised.exception))
            self.assertNotIn("secret", str(raised.exception))
            request.assert_called_once()

    def test_other_http_status_is_preserved(self):
        error = urllib.error.HTTPError("http://127.0.0.1:9334/tabs", 503,
                                       "Service Unavailable", {}, io.BytesIO(b"busy"))
        try:
            with patch("urllib.request.urlopen", side_effect=error) as request:
                with self.assertRaises(urllib.error.HTTPError) as raised:
                    Browser(port=9334, token="secret").tabs()
                self.assertIs(raised.exception, error)
                self.assertEqual(error.reason, "Service Unavailable")
                self.assertEqual(error.read(), b"busy")
                request.assert_called_once()
        finally:
            error.close()

    def test_other_transport_and_json_errors_are_preserved(self):
        errors = [urllib.error.URLError(OSError(errno.ECONNRESET, "reset")),
                  urllib.error.URLError("DNS failure"), TimeoutError("timed out")]
        for error in errors:
            with self.subTest(error=error):
                with patch("urllib.request.urlopen", side_effect=error) as request:
                    with self.assertRaises(type(error)) as raised:
                        Browser(port=9334, token="secret").tabs()
                    self.assertIs(raised.exception, error)
                    request.assert_called_once()
        with patch("urllib.request.urlopen", return_value=io.BytesIO(b"not json")):
            with self.assertRaises(json.JSONDecodeError):
                Browser(port=9334, token="secret").tabs()


if __name__ == "__main__":
    unittest.main()
