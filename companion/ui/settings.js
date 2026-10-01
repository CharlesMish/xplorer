const statusEl = document.getElementById('settings-status');
const searchHome = document.getElementById('search-home');
const chatModel = document.getElementById('chat-model');
const searchModel = document.getElementById('search-model');
const browserTheme = document.getElementById('browser-theme');
const maxTurns = document.getElementById('max-turns');

function setStatus(msg, kind = '') {
  if (!statusEl) return;
  statusEl.textContent = msg;
  statusEl.className = 'settings-status' + (kind ? ` ${kind}` : '');
}

function fillSelect(select, models, selected) {
  populateModelSelect(select, models, selected);
}

// Provider cards: a status badge and the "show in chat" switch.
function setBadge(id, text, kind) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = text;
  el.className = `provider-badge ${kind}`;
}

(function setupProviderToggles() {
  const hidden = new Set(getHiddenProviders());
  document.querySelectorAll('[data-provider-toggle]').forEach((box) => {
    box.checked = !hidden.has(box.dataset.providerToggle);
    box.addEventListener('change', () => {
      setProviderHidden(box.dataset.providerToggle, !box.checked);
      setStatus(box.checked ? 'Shown in the chat model list' : 'Hidden from the chat model list', 'ok');
    });
  });
})();

document.getElementById('open-chrome-settings')?.addEventListener('click', () => {
  fetch('/api/open-chrome-settings', { method: 'POST' }).catch(() => {});
});

async function loadTheme() {
  try {
    const res = await fetch('/api/theme');
    if (!res.ok) return;
    const data = await res.json();
    if (browserTheme && data.color_scheme) {
      browserTheme.value = data.color_scheme;
    }
  } catch { /* ignore */ }
}

async function init() {
  startThemeWatcher();
  const settings = await fetchSettings();
  const models = settings.models || await fetchModels();

  if (searchHome) searchHome.value = settings.search_home || SEARCH_HOME_BUILD;
  fillSelect(chatModel, models, settings.model || DEFAULT_MODEL);
  fillSelect(searchModel, models, settings.search_model || SEARCH_DEFAULT_MODEL);
  if (maxTurns) maxTurns.value = settings.max_turns || 50;
  await loadAccount();
  await loadClaude();
  await loadTheme();
}

// Claude sign-in: Anthropic's ant CLI prints a Console authorize URL (opened
// in an Xplor tab by the gateway); the user pastes back the code it shows.
async function claudeStatus() {
  try {
    const res = await fetch('/api/claude/status', { cache: 'no-store' });
    if (res.ok) return await res.json();
  } catch { /* offline */ }
  return {};
}

async function loadClaude(prefetched) {
  const st = prefetched || await claudeStatus();
  const $c = (id) => document.getElementById(id);
  const status = $c('claude-status');
  if (!status) return;
  const install = $c('claude-install');
  const codeRow = $c('claude-code-row');
  const signin = $c('claude-signin');
  const signout = $c('claude-signout');
  const recheck = $c('claude-recheck');
  if (st.install_command) $c('claude-install-cmd').textContent = st.install_command;
  if (st.install_help) $c('claude-install-help').href = st.install_help;

  install.hidden = st.installed !== false;
  recheck.hidden = st.installed !== false;
  codeRow.hidden = !(st.running && st.url);
  signin.hidden = !st.installed || st.signed_in || (st.running && st.url);
  signout.hidden = !st.signed_in;
  setBadge('claude-badge',
    st.signed_in ? 'Connected' : (st.installed === false ? 'Needs setup' : 'Not connected'),
    st.signed_in ? 'on' : 'off');
  if (st.installed === false) status.textContent = 'The ant tool is not installed.';
  else if (st.signed_in) status.textContent = 'Signed in. Claude models are in the chat model list.';
  else if (st.running && st.url) status.textContent = 'Waiting for the code from the Claude tab.';
  else status.textContent = st.error ? `Sign-in failed: ${st.error}` : 'Not signed in';

  $c('claude-copy').onclick = async () => {
    try {
      await navigator.clipboard.writeText($c('claude-install-cmd').textContent);
      $c('claude-copy').textContent = 'Copied';
      setTimeout(() => { $c('claude-copy').textContent = 'Copy'; }, 1500);
    } catch { /* select it instead */ }
  };
  recheck.onclick = () => loadClaude();
  signin.onclick = async () => {
    signin.disabled = true;
    status.textContent = 'Opening the Claude sign-in tab…';
    try {
      const res = await fetch('/api/claude/login', { method: 'POST', cache: 'no-store' });
      const next = await res.json();
      if (next.error && !next.running) throw new Error(next.error);
      await loadClaude(next);
      $c('claude-code')?.focus();
    } catch (e) {
      status.textContent = e.message || 'Could not start sign-in.';
    } finally {
      signin.disabled = false;
    }
  };
  const submit = $c('claude-code-submit');
  submit.onclick = async () => {
    const code = $c('claude-code').value.trim();
    if (!code) return;
    submit.disabled = true;
    status.textContent = 'Finishing sign-in…';
    try {
      const res = await fetch('/api/claude/code', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ code }),
      });
      const next = await res.json();
      $c('claude-code').value = '';
      await loadClaude(next);
      if (next.error && !next.signed_in) status.textContent = `Sign-in failed: ${next.error}`;
    } finally {
      submit.disabled = false;
    }
  };
  $c('claude-code').onkeydown = (e) => { if (e.key === 'Enter') submit.click(); };
  signout.onclick = async () => {
    signout.disabled = true;
    try {
      const res = await fetch('/api/claude/logout', { method: 'POST', cache: 'no-store' });
      await loadClaude(await res.json());
    } finally {
      signout.disabled = false;
    }
  };
}

async function loadAccount() {
  const status = document.getElementById('account-status');
  const btn = document.getElementById('account-signin');
  const out = document.getElementById('account-signout');
  if (!status) return;
  let st = {};
  try {
    const res = await fetch('/api/grok/login', { cache: 'no-store' });
    if (res.ok) st = await res.json();
  } catch { /* unsigned */ }
  const signed = st.logged_in === true || st.has_token === true;
  setBadge('grok-badge', signed ? 'Connected' : 'Not connected', signed ? 'on' : 'off');
  status.textContent = signed
    ? (st.account ? `Signed in as ${st.account}` : 'Signed in to Grok')
    : 'Not signed in';
  if (out) {
    out.hidden = !signed;
    out.onclick = async () => {
      out.disabled = true;
      status.textContent = 'Signing out…';
      try {
        const res = await fetch('/api/grok/logout', { method: 'POST', cache: 'no-store' });
        if (!res.ok) throw new Error('Could not sign out.');
      } catch (e) {
        status.textContent = e.message || 'Could not sign out.';
        out.disabled = false;
        return;
      }
      out.disabled = false;
      await loadAccount();
    };
  }
  if (btn) {
    btn.hidden = signed;
    btn.onclick = async () => {
      btn.disabled = true;
      status.textContent = 'Opening sign-in…';
      try {
        const start = await fetch('/api/grok/login', { method: 'POST', cache: 'no-store' });
        if (!start.ok) throw new Error('Sign-in is unavailable.');
        let cur = await start.json();
        const deadline = Date.now() + 10 * 60 * 1000;
        while (Date.now() < deadline) {
          if (cur.ok || cur.logged_in) break;
          if (cur.done && !cur.ok) throw new Error(cur.error || 'Sign-in did not finish.');
          if (cur.message) status.textContent = cur.message;
          await new Promise((r) => setTimeout(r, 1200));
          const poll = await fetch('/api/grok/login', { cache: 'no-store' });
          cur = await poll.json();
        }
      } catch (e) {
        status.textContent = e.message || 'Could not sign in.';
        btn.disabled = false;
        return;
      }
      btn.disabled = false;
      await loadAccount();
    };
  }
}

async function persist(partial) {
  setStatus('Saving…');
  try {
    await saveSettings(partial);
    setStatus('Saved', 'ok');
    setTimeout(() => setStatus(''), 2000);
  } catch (e) {
    setStatus(e.message, 'err');
  }
}

searchHome?.addEventListener('change', () => {
  persistSearchHome(searchHome.value);
  persist({ search_home: searchHome.value });
});

chatModel?.addEventListener('change', () => {
  persistModel(chatModel.value);
  persist({ model: chatModel.value });
});

searchModel?.addEventListener('change', () => {
  persistSearchModel(searchModel.value);
  persist({ search_model: searchModel.value });
});

maxTurns?.addEventListener('change', () => {
  let n = parseInt(maxTurns.value, 10);
  if (!Number.isFinite(n)) n = 50;
  n = Math.min(200, Math.max(1, n));
  maxTurns.value = String(n);
  persist({ max_turns: n });
});

browserTheme?.addEventListener('change', async () => {
  setStatus('Saving theme…');
  try {
    const res = await fetch('/api/theme', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ color_scheme: browserTheme.value }),
    });
    const data = await res.json();
    if (!res.ok) throw new Error(data.error || res.statusText);
    setStatus('Theme updated', 'ok');
    setTimeout(() => setStatus(''), 2000);
  } catch (e) {
    setStatus(e.message, 'err');
  }
});

document.getElementById('replay-welcome')?.addEventListener('click', () => {
  window.location.href = '/welcome';
});

// --------------------------------------------------------------------------
// Sidebar pane switching (General / Bookmarks). Simple class/hidden toggle.
// --------------------------------------------------------------------------
const navButtons = document.querySelectorAll('.settings-nav-btn');
const panes = document.querySelectorAll('.settings-pane');
function showPane(name) {
  navButtons.forEach((b) => b.classList.toggle('active', b.dataset.pane === name));
  panes.forEach((p) => {
    const on = p.dataset.pane === name;
    p.classList.toggle('active', on);
    p.hidden = !on;
  });
}
navButtons.forEach((b) => {
  b.addEventListener('click', () => showPane(b.dataset.pane));
});
// Deep-link to a pane via URL fragment, e.g. /settings#bookmarks (the native
// "Manage bookmarks" group-header button opens this). Only honor a hash that
// names a real pane; otherwise leave the default (first) pane active.
function showPaneFromHash() {
  const name = (location.hash || '').replace(/^#/, '');
  if (name && [...panes].some((p) => p.dataset.pane === name)) {
    showPane(name);
  }
}
showPaneFromHash();
window.addEventListener('hashchange', showPaneFromHash);

// --------------------------------------------------------------------------
// Bookmarks editor. Config contract (stored top-level under "bookmarks") is an
// ORDERED ARRAY: [ {id, label, url} ]. Order in the array == tab order in the
// native "Bookmarks" group. The string "id" is parsed to an int64 by the C++
// seeder and stamped on the tab as TabOwnership::bookmark_node_id, so it must
// be a stable, unique, positive integer. Existing ids are preserved; a newly
// added row gets max(existing numeric ids)+1. Empty/invalid-url rows are
// dropped server-side. Empty list → built-in defaults re-seeded next launch.
// --------------------------------------------------------------------------
const bookmarksEditor = document.getElementById('bookmarks-editor');
const bookmarksStatus = document.getElementById('bookmarks-status');
const bookmarksAdd = document.getElementById('bookmarks-add');

function setBookmarksStatus(msg, kind = '') {
  if (!bookmarksStatus) return;
  bookmarksStatus.textContent = msg;
  bookmarksStatus.className = 'settings-status' + (kind ? ` ${kind}` : '');
}

/** Render one bookmark row: ▲▼ reorder + label + url + remove. */
function makeBookmarkRow(bm = {}) {
  const card = document.createElement('div');
  card.className = 'tb-pill';
  // Stash the id on the row so collect can preserve it (blank for new rows).
  card.dataset.bmId = bm.id != null ? String(bm.id) : '';

  const head = document.createElement('div');
  head.className = 'tb-pill-head';

  const order = document.createElement('div');
  order.className = 'tb-order';
  const up = document.createElement('button');
  up.type = 'button';
  up.className = 'tb-order-btn tb-up';
  up.title = 'Move up';
  up.textContent = '▲';
  up.addEventListener('click', () => {
    const prev = card.previousElementSibling;
    if (prev) card.parentNode.insertBefore(card, prev);
  });
  const down = document.createElement('button');
  down.type = 'button';
  down.className = 'tb-order-btn tb-down';
  down.title = 'Move down';
  down.textContent = '▼';
  down.addEventListener('click', () => {
    const next = card.nextElementSibling;
    if (next) card.parentNode.insertBefore(next, card);
  });
  order.append(up, down);

  const fields = document.createElement('div');
  fields.className = 'tb-fields';
  const label = document.createElement('input');
  label.type = 'text';
  label.className = 'tb-label';
  label.placeholder = 'Label';
  label.value = bm.label || '';
  const url = document.createElement('input');
  url.type = 'text';
  url.className = 'tb-href';
  url.placeholder = 'https://…';
  url.value = bm.url || '';
  fields.append(label, url);

  const remove = document.createElement('button');
  remove.type = 'button';
  remove.className = 'tb-pill-remove';
  remove.title = 'Remove bookmark';
  remove.textContent = '×';
  remove.addEventListener('click', () => card.remove());

  head.append(order, fields, remove);
  card.appendChild(head);
  return card;
}

/** Render the editor from a list of {id,label,url} objects. */
function renderBookmarksEditor(bookmarks) {
  if (!bookmarksEditor) return;
  bookmarksEditor.innerHTML = '';
  for (const bm of bookmarks) bookmarksEditor.appendChild(makeBookmarkRow(bm));
}

/** Largest numeric id currently in the editor (0 if none) — for new-row ids. */
function maxBookmarkId() {
  let max = 0;
  bookmarksEditor?.querySelectorAll('.tb-pill').forEach((card) => {
    const n = parseInt(card.dataset.bmId, 10);
    if (Number.isFinite(n) && n > max) max = n;
  });
  return max;
}

/** Collect [{id,label,url}], preserving ids and assigning ids to new rows. */
function collectBookmarks() {
  const out = [];
  if (!bookmarksEditor) return out;
  let nextId = maxBookmarkId() + 1;
  bookmarksEditor.querySelectorAll('.tb-pill').forEach((card) => {
    const label = card.querySelector('.tb-label')?.value.trim() || '';
    const url = card.querySelector('.tb-href')?.value.trim() || '';
    let id = card.dataset.bmId;
    if (!id) {
      id = String(nextId);
      nextId += 1;
      card.dataset.bmId = id;
    }
    out.push({ id, label, url });
  });
  return out;
}

async function loadBookmarksEditor() {
  let bookmarks = [];
  try {
    const settings = await fetchSettings();
    const stored = settings.bookmarks;
    if (Array.isArray(stored)) {
      bookmarks = stored.map((b) => ({
        id: b.id != null ? String(b.id) : '',
        label: b.label || '',
        url: b.url || '',
      }));
    }
  } catch (e) {
    setBookmarksStatus(e.message, 'err');
  }
  renderBookmarksEditor(bookmarks);
}

bookmarksAdd?.addEventListener('click', () => {
  const card = makeBookmarkRow({ id: String(maxBookmarkId() + 1) });
  bookmarksEditor.appendChild(card);
  card.querySelector('.tb-label')?.focus();
});

document.getElementById('bookmarks-save')?.addEventListener('click', async () => {
  setBookmarksStatus('Saving…');
  try {
    await saveSettings({ bookmarks: collectBookmarks() });
    setBookmarksStatus('Saved — changes apply immediately in Xplor', 'ok');
    setTimeout(() => setBookmarksStatus(''), 3000);
  } catch (e) {
    setBookmarksStatus(e.message, 'err');
  }
});

loadBookmarksEditor();

// --------------------------------------------------------------------------
// Favorites editor: the pinned app row, stored under "pinned_apps" as an
// ordered [{id,label,url}] (max 12). Same row UI as bookmarks.
// --------------------------------------------------------------------------
const favoritesEditor = document.getElementById('favorites-editor');
const favoritesStatus = document.getElementById('favorites-status');
const FAVORITES_MAX = 12;

function setFavoritesStatus(msg, kind = '') {
  if (!favoritesStatus) return;
  favoritesStatus.textContent = msg;
  favoritesStatus.className = 'settings-status' + (kind ? ` ${kind}` : '');
}

function syncFavoritesAdd() {
  const add = document.getElementById('favorites-add');
  if (add) add.disabled = favoritesEditor.children.length >= FAVORITES_MAX;
}

async function loadFavoritesEditor() {
  if (!favoritesEditor) return;
  let favorites = [];
  try {
    const settings = await fetchSettings();
    if (Array.isArray(settings.pinned_apps)) {
      favorites = settings.pinned_apps.map((f, i) => ({
        id: f.id != null ? String(f.id) : String(i + 1),
        label: f.label || '',
        url: f.url || '',
      }));
    }
  } catch (e) {
    setFavoritesStatus(e.message, 'err');
  }
  favoritesEditor.innerHTML = '';
  for (const f of favorites) favoritesEditor.appendChild(makeBookmarkRow(f));
  syncFavoritesAdd();
}

document.getElementById('favorites-add')?.addEventListener('click', () => {
  if (favoritesEditor.children.length >= FAVORITES_MAX) return;
  const card = makeBookmarkRow({ id: String(favoritesEditor.children.length + 1) });
  favoritesEditor.appendChild(card);
  card.querySelector('.tb-label')?.focus();
  syncFavoritesAdd();
});
favoritesEditor?.addEventListener('click', (e) => {
  if (e.target.closest('.tb-pill-remove')) setTimeout(syncFavoritesAdd, 0);
});

document.getElementById('favorites-save')?.addEventListener('click', async () => {
  const list = [];
  favoritesEditor.querySelectorAll('.tb-pill').forEach((card, i) => {
    let url = card.querySelector('.tb-href')?.value.trim() || '';
    if (!url) return;
    if (!/^https?:\/\//i.test(url)) url = 'https://' + url;
    const label = card.querySelector('.tb-label')?.value.trim() || '';
    list.push({ id: String(i + 1), label, url });
  });
  setFavoritesStatus('Saving…');
  try {
    await saveSettings({ pinned_apps: list });
    setFavoritesStatus('Saved. The sidebar updates right away.', 'ok');
    setTimeout(() => setFavoritesStatus(''), 3000);
    loadFavoritesEditor();
  } catch (e) {
    setFavoritesStatus(e.message, 'err');
  }
});

loadFavoritesEditor();
// Right-click edits in the sidebar change the same list.
window.addEventListener('focus', () => {
  if (!document.activeElement?.closest?.('#favorites-editor')) loadFavoritesEditor();
});

// --------------------------------------------------------------------------
// Updates pane. Driven by the macOS Sparkle bridge endpoints
// (/api/update/sparkle*). Those exist on macOS only; on other platforms the
// GET 404s (or reports unavailable) and we present a graceful read-only note.
// --------------------------------------------------------------------------
const updatesAutoCheck = document.getElementById('updates-auto-check');
const updatesCheckBtn = document.getElementById('updates-check-btn');
const updatesVersion = document.getElementById('updates-version');
const updatesStatusEl = document.getElementById('updates-status');

function setUpdatesStatus(msg, kind = '') {
  if (!updatesStatusEl) return;
  updatesStatusEl.textContent = msg;
  updatesStatusEl.className = 'settings-status' + (kind ? ` ${kind}` : '');
}

async function loadUpdates() {
  try {
    const res = await fetch('/api/update/controls');
    if (!res.ok) throw new Error('unavailable');
    const d = await res.json();
    if (!d.supported) throw new Error('unavailable');
    if (updatesAutoCheck) updatesAutoCheck.checked = !!d.auto_check;
    if (updatesVersion) updatesVersion.textContent = d.current_version || '—';
  } catch {
    // Not macOS, or the updater hasn't started — degrade gracefully.
    if (updatesAutoCheck) updatesAutoCheck.disabled = true;
    if (updatesCheckBtn) updatesCheckBtn.disabled = true;
    setUpdatesStatus('Updates are managed by your platform’s installer on this system.');
  }
}

updatesAutoCheck?.addEventListener('change', async () => {
  setUpdatesStatus('Saving…');
  try {
    const res = await fetch('/api/update/controls/auto-check', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ enabled: updatesAutoCheck.checked }),
    });
    const d = await res.json();
    if (!res.ok) throw new Error(d.error || res.statusText);
    updatesAutoCheck.checked = !!d.auto_check;
    setUpdatesStatus(
      updatesAutoCheck.checked
        ? 'Automatic update checks are on.'
        : 'Automatic update checks are off.',
      'ok',
    );
    setTimeout(() => setUpdatesStatus(''), 2500);
  } catch (e) {
    setUpdatesStatus(e.message, 'err');
  }
});

updatesCheckBtn?.addEventListener('click', async () => {
  setUpdatesStatus('Checking for updates…');
  try {
    const res = await fetch('/api/update/controls/check', { method: 'POST' });
    if (!res.ok) throw new Error(res.statusText);
    setUpdatesStatus('Checking — Xplor will show a dialog if an update is available.', 'ok');
    setTimeout(() => setUpdatesStatus(''), 4000);
  } catch (e) {
    setUpdatesStatus(e.message, 'err');
  }
});

loadUpdates();

init().catch((e) => setStatus(e.message, 'err'));