"""A failed preparation must never proceed to inserting text."""
import unittest
from unittest.mock import Mock, patch

from agent_context import Page
import xplorer_mcp


class TextFieldErrors(unittest.TestCase):
    def test_mcp_does_not_clear_another_field_after_failed_click(self):
        with patch.object(xplorer_mcp, "api", return_value={"error": "selector not found"}) as api:
            with self.assertRaisesRegex(RuntimeError, "could not click"):
                xplorer_mcp.t_type({"tab": "fixture", "selector": "#missing",
                                    "text": "replacement"})
            api.assert_called_once()

    def test_page_does_not_insert_after_failed_preparation(self):
        for result in (None, "no-host", "no-input", "not-editable"):
            with self.subTest(result=result):
                browser = Mock()
                browser.eval.return_value = result
                with self.assertRaises(RuntimeError):
                    Page(browser, "fixture").type(0, "replacement")
                browser.type.assert_not_called()

    def test_mcp_does_not_insert_after_failed_preparation(self):
        for response in ({}, {"exceptionDetails": {"text": "TypeError"}},
                         {"result": {"value": "no-input"}},
                         {"result": {"value": "no-host"}},
                         {"result": {"value": "no-focus"}},
                         {"result": {"value": "not-editable"}}):
            with self.subTest(response=response):
                with patch.object(xplorer_mcp, "api", side_effect=[{}, response]) as api:
                    with self.assertRaisesRegex(RuntimeError, "writable text field"):
                        xplorer_mcp.t_type({"tab": "fixture", "selector": "#field",
                                            "text": "replacement"})
                    self.assertEqual(api.call_count, 2)
                    self.assertTrue(api.call_args_list[-1].args[1].endswith("/eval"))


if __name__ == "__main__":
    unittest.main()
