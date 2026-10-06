// Runs the actual helpers' generated JavaScript in Chromium, followed by the
// same click + CDP Input.insertText used by AgentSession::Type. No live Xplor.
// Prerequisites: npm install playwright; npx playwright install chromium
// Run: node sdk/tests/text_fields.browser.cjs
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');
const {execFileSync} = require('node:child_process');
const {chromium} = require('playwright');

const sdk = path.resolve(__dirname, '..');
const expressions = JSON.parse(execFileSync(process.env.PYTHON || 'python3', ['-c', `
import json, sys
sys.path.insert(0, sys.argv[1])
from agent_context import Page
import xplorer_mcp as mcp
captured = {}
class Capture:
    def eval(self, tab, expression):
        captured['page'] = expression
        return 'ok'
    def type(self, *args): pass
Page(Capture(), 'fixture').type(0, 'replacement')
def api(method, path, body=None):
    if path.endswith('/eval'):
        captured['mcp'] = body['expression']
        return {'result': {'value': 'ok'}}
    return {}
mcp.api = api
mcp.t_type({'tab': 'fixture', 'ref': 0, 'text': 'replacement'})
print(json.dumps(captured))
`, sdk], {encoding: 'utf8'}));

(async () => {
  const browser = await chromium.launch({headless: true,
    ...(process.env.CHROMIUM_EXECUTABLE ? {executablePath: process.env.CHROMIUM_EXECUTABLE} : {})});
  let failures = 0;
  try {
    const page = await browser.newPage();
    const cdp = await page.context().newCDPSession(page);
    for (const [helper, expression] of Object.entries(expressions)) {
      for (const [target, field, replacement] of [
        ['single', 'single', 'new café text'],
        ['multi', 'multi', 'first line\nsecond line'],
        ['rich', 'rich', 'new editable text'],
        ['wrapper', 'wrapped-input', 'new wrapped text'],
        ['readonly', 'readonly', 'must not replace'],
      ]) {
        try {
          await page.goto('about:blank');
          await page.setContent(fs.readFileSync(path.join(sdk, 'fixtures/text_fields.html'), 'utf8'));
          await page.locator('#' + target).evaluate(el => el.setAttribute('data-aref', '0'));
          // MCP uses a trusted click before preparing the active field.
          if (helper === 'mcp') await page.locator('#' + target).click();
          const result = await page.evaluate(expression);
          if (field === 'readonly') {
            assert.equal(result, 'not-editable');
            assert.equal(await page.evaluate(id => fieldState(id).value, field), 'old text');
          } else {
            assert.equal(result, 'ok');
            await page.locator('[data-atype="1"]').click();
            await cdp.send('Input.insertText', {text: replacement});
            const state = await page.evaluate(id => fieldState(id), field);
            assert.equal(state.value, replacement);
            assert.equal(state.events[0].value, '');
            assert.equal(state.events.at(-1).value, replacement);
            assert.equal(state.events.at(-1).trusted, true);
          }
          console.log(`PASS ${helper}: ${field}`);
        } catch (error) {
          failures++;
          console.error(`FAIL ${helper}: ${field}: ${error.message}`);
        }
      }
    }
    // A stale, writable activeElement must not let a disabled/read-only
    // target or a target that refuses focus clear another field.
    for (const [target, condition, expected] of [
      ['single', 'disabled', 'not-editable'],
      ['multi', 'readOnly', 'not-editable'],
      ['multi', 'disabled', 'not-editable'],
      ['single', 'redirect-focus', 'no-focus'],
    ]) {
      try {
        await page.goto('about:blank');
        await page.setContent(fs.readFileSync(path.join(sdk, 'fixtures/text_fields.html'), 'utf8'));
        await page.evaluate(({target, condition}) => {
          const el = document.getElementById(target);
          const other = document.getElementById('wrapped-input');
          el.setAttribute('data-aref', '0');
          other.focus();
          if (condition === 'redirect-focus') el.addEventListener('focus', () => other.focus());
          else el[condition] = true;
        }, {target, condition});
        const before = await page.evaluate(() => [fieldState('single'), fieldState('multi'), fieldState('wrapped-input')]);
        assert.equal(await page.evaluate(expressions.mcp), expected);
        assert.deepEqual(await page.evaluate(() => [fieldState('single'), fieldState('multi'), fieldState('wrapped-input')]), before);
        console.log(`PASS mcp guard: ${target} ${condition}`);
      } catch (error) {
        failures++;
        console.error(`FAIL mcp guard: ${target} ${condition}: ${error.message}`);
      }
    }
    // Wrappers must not turn unrelated stale focus into a replacement target.
    for (const [condition, expected] of [
      ['empty', 'no-input'],
      ['ambiguous', 'ambiguous-input'],
      ['disabled', 'not-editable'],
      ['readOnly', 'not-editable'],
      ['redirect-focus', 'no-focus'],
      ['delegated-second', 'ok'],
    ]) {
      try {
        await page.goto('about:blank');
        await page.setContent(fs.readFileSync(path.join(sdk, 'fixtures/text_fields.html'), 'utf8'));
        await page.evaluate(condition => {
          const host = document.getElementById('wrapper');
          const field = document.getElementById('wrapped-input');
          const other = document.getElementById('rich');
          host.setAttribute('data-aref', '0');
          other.focus();
          if (condition === 'empty') host.replaceChildren();
          else if (condition === 'ambiguous' || condition === 'delegated-second') {
            const second = document.createElement('input');
            second.id = 'second'; second.value = 'second old text';
            host.appendChild(second);
            if (condition === 'delegated-second') second.focus();
          } else if (condition === 'redirect-focus') field.addEventListener('focus', () => other.focus());
          else field[condition] = true;
        }, condition);
        const snapshot = () => Array.from(document.querySelectorAll('input,textarea,[contenteditable]'))
          .map(el => ({id: el.id, value: el.isContentEditable ? el.textContent : el.value}));
        const before = await page.evaluate(snapshot);
        assert.equal(await page.evaluate(expressions.mcp), expected);
        if (expected === 'ok') {
          await cdp.send('Input.insertText', {text: 'selected second field'});
          before.find(el => el.id === 'second').value = 'selected second field';
        }
        assert.deepEqual(await page.evaluate(snapshot), before);
        assert.deepEqual(await page.evaluate(() => fieldState('rich').events), []);
        console.log(`PASS mcp wrapper guard: ${condition}`);
      } catch (error) {
        failures++;
        console.error(`FAIL mcp wrapper guard: ${condition}: ${error.message}`);
      }
    }
  } finally { await browser.close(); }
  process.exitCode = failures ? 1 : 0;
})().catch(error => {console.error(error); process.exitCode = 1;});
