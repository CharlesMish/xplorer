#!/usr/bin/env python3
"""Contract test for the first-run wizard. No browser required."""

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parent
HTML = (ROOT / "welcome.html").read_text()
JS = (ROOT / "welcome.js").read_text()
CSS = (ROOT / "welcome.css").read_text()
ONBOARDING = (
    ROOT.parents[1] / "src/chrome/browser/agent_gateway/xplorer_onboarding.h"
).read_text()
SIDEBAR = (
    ROOT.parents[1]
    / "src/chrome/browser/ui/views/xplorer/xplorer_sidebar_chrome_view.cc"
).read_text()


class WelcomeFlowTest(unittest.TestCase):
    def test_slogan_is_not_arc(self):
        self.assertIn("A calmer way to browse.", HTML)
        self.assertIn("A calmer way to browse.", JS)
        self.assertNotIn("A browser for you", HTML + JS)
        self.assertNotIn("browses for you", HTML + JS)

    def test_steps_in_order(self):
        self.assertIn(
            "const STEPS = ['intro', 'account', 'import', 'favorites', 'theme', 'default'];",
            JS,
        )
        for step in ("intro", "account", "import", "favorites", "theme", "default"):
            self.assertIn(f'data-step="{step}"', HTML)

    def test_login_stays_in_this_browser(self):
        self.assertIn("open_tab: false", JS)
        self.assertIn("location.assign(status.url)", JS)
        self.assertIn("/api/grok/login", JS)

    def test_safari_is_offered_even_when_macos_hides_it(self):
        self.assertIn("name: 'Safari'", JS)
        self.assertIn("name: 'Google Chrome'", JS)
        self.assertIn("Full Disk Access", JS)
        self.assertIn("/api/system/privacy", JS)
        self.assertIn("Allow Full Disk Access", JS)
        self.assertIn("syncImportAction", JS)

    def test_import_and_default_endpoints(self):
        self.assertIn("/api/import/browsers", JS)
        self.assertIn("/api/import", JS)
        self.assertIn("/api/default-browser", JS)
        self.assertIn("DetectImportBrowsers", ONBOARDING)
        self.assertIn("SetDefaultBrowser", ONBOARDING)

    def test_pins_and_theme_payload(self):
        self.assertIn("pinned_apps: selectedApps()", JS)
        self.assertIn("theme_color: state.theme", JS)
        self.assertIn("onboarding_version: 2", JS)
        self.assertRegex(JS, r"color: '#[0-9a-f]{6}'")

    def test_sidebar_has_address_field_and_pins(self):
        self.assertIn("Search or Enter URL", SIDEBAR)
        self.assertIn("NavigateFromField", SIDEBAR)
        self.assertIn("GetPinnedAppConfigs", SIDEBAR)
        self.assertIn("kArrowBackIcon", SIDEBAR)

    def test_palette_is_black_and_grey(self):
        self.assertIn("#0e0e10", CSS)
        self.assertNotIn("#3D7EFF", CSS + HTML)


if __name__ == "__main__":
    unittest.main()
