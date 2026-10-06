# Text-field replacement

The `Page.type` and MCP `xplorer_type` helpers replace existing text before
asking the native gateway to insert new text. The low-level `Browser.type`
method still performs insertion; its contract is unchanged.

## Live Xplor check (stdlib only)

Launch Xplor, then from `sdk/` run:

```sh
env -u XPLORER_TOKEN python3 text_fields_smoke.py
```

The runner serves only the fixture on an ephemeral loopback port, opens one
uniquely owned tab, and checks both helpers against an input, a textarea, a
plain contenteditable region, a wrapper that delegates focus, and a read-only
input. Leave tabs in place while it runs. Expect `ALL PASS: 10/10 checks`.
Each case also verifies that the other fixture fields and their input-event
histories are unchanged. A failed case prints expected/observed text, any
helper rejection, the focused element, and the fixture's field states.
The runner temporarily sets its own process's `XPLORER_AGENT_ID` to the unique
fixture-tab owner so MCP requests satisfy the native gateway's ownership check.
It restores the previous value when finished; your shell configuration is unchanged.
The tab stays open for inspection, and the local server stops when the runner
exits. It does not submit forms or use external websites.

The fixture can also be opened manually (`sdk/fixtures/text_fields.html`), but
manually typing in it does **not** test the Python/MCP helpers.

## Automated regression checks

```sh
python3 -m unittest discover -s sdk -p 'test_*.py' -v
```

For real DOM/typing checks, install the development-only `playwright` npm
package and its Chromium browser, then run:

```sh
node sdk/tests/text_fields.browser.cjs
node sdk/tests/text_fields_runner.browser.cjs
```

`PYTHON` can select a Python executable; `CHROMIUM_EXECUTABLE` can select an
already installed Chromium. The test obtains the JavaScript from the actual
Python helpers, executes it in Chromium, then uses a click and CDP
`Input.insertText`, matching `AgentSession::Type`. It checks replacement text,
newline/Unicode handling, input events, focus delegation, and read-only
preservation. This is not a substitute for the live Xplor check above.

The second check runs the complete Python smoke runner against Chromium behind
a local HTTP fixture that rejects identified agents acting on another owner's
tab. It seeds an unrelated inherited agent identity to catch ownership mistakes.
It uses a fresh CDP session per request and raw mouse events, rather than
Playwright's auto-waiting click. It runs both with normal click focus and with
the default mousedown focus action suppressed (click handlers still run).
The latter reproduces the reported 6/10 pattern with the earlier MCP helper:
direct fields are left unchanged while the previously focused field is
overwritten, including when targeting a read-only field. The corrected helper
explicitly focuses direct targets and checks focus before clearing them;
wrappers still use their click-delegated editable field. This models a failure
mode, not the established cause of native macOS focus behavior.

The first browser check also exercises disabled/read-only targets with stale
focus and a field that redirects focus elsewhere. No field is cleared when
preparation rejects the target.

Scope: ordinary HTML fields and plain contenteditable regions. Rich editors
with their own document model (for example, ProseMirror) need separate tests;
this patch does not claim general rich-editor compatibility.
