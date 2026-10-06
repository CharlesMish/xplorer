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
```

`PYTHON` can select a Python executable; `CHROMIUM_EXECUTABLE` can select an
already installed Chromium. The test obtains the JavaScript from the actual
Python helpers, executes it in Chromium, then uses a click and CDP
`Input.insertText`, matching `AgentSession::Type`. It checks replacement text,
newline/Unicode handling, input events, focus delegation, and read-only
preservation. This is not a substitute for the live Xplor check above.

Scope: ordinary HTML fields and plain contenteditable regions. Rich editors
with their own document model (for example, ProseMirror) need separate tests;
this patch does not claim general rich-editor compatibility.
