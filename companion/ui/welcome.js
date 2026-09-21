/** First-run wizard. The slogan is ours, not Arc's marketing line. */

const STEPS = ['intro', 'account', 'import', 'favorites', 'theme', 'default'];
const SLOGAN = 'A calmer way to browse.';

const APPS = [
  { id: 'gmail', label: 'Gmail', url: 'https://mail.google.com/' },
  { id: 'calendar', label: 'Calendar', url: 'https://calendar.google.com/' },
  { id: 'github', label: 'GitHub', url: 'https://github.com/' },
  { id: 'x', label: 'X', url: 'https://x.com/' },
  { id: 'youtube', label: 'YouTube', url: 'https://www.youtube.com/' },
  { id: 'docs', label: 'Docs', url: 'https://docs.google.com/' },
  { id: 'notion', label: 'Notion', url: 'https://www.notion.so/' },
  { id: 'slack', label: 'Slack', url: 'https://app.slack.com/' },
  { id: 'figma', label: 'Figma', url: 'https://www.figma.com/' },
  { id: 'maps', label: 'Maps', url: 'https://maps.google.com/' },
  { id: 'spotify', label: 'Spotify', url: 'https://open.spotify.com/' },
  { id: 'grok', label: 'Grok', url: 'https://grok.com/' },
];

const TONES = [
  { id: 'ink', name: 'Ink', color: '#1c1c1f' },
  { id: 'graphite', name: 'Graphite', color: '#3a3a40' },
  { id: 'slate', name: 'Slate', color: '#5c6570' },
  { id: 'steel', name: 'Steel', color: '#7d8794' },
  { id: 'fog', name: 'Fog', color: '#a8b0ba' },
  { id: 'frost', name: 'Frost', color: '#d5d8de' },
  { id: 'blue-steel', name: 'Blue steel', color: '#6d7f99' },
  { id: 'silver', name: 'Silver', color: '#c5c8ce' },
];

const state = {
  step: 'intro',
  browsers: [],
  browserIndex: 0,
  apps: new Set(['gmail', 'github', 'grok']),
  theme: '#7d8794',
};

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function show(step) {
  if (!STEPS.includes(step)) step = 'intro';
  state.step = step;
  document.querySelectorAll('.step').forEach((el) => {
    el.classList.toggle('hidden', el.dataset.step !== step);
  });
  if (step === 'import') loadBrowsers();
  if (step === 'default') prepareDefault();
}

function setError(id, message) {
  const el = document.getElementById(id);
  if (!el) return;
  el.hidden = !message;
  el.textContent = message || '';
}

async function startGrokLogin() {
  const button = document.getElementById('sign-in');
  setError('account-error', '');
  if (button) {
    button.disabled = true;
    button.textContent = 'Opening sign-in…';
  }
  try {
    const start = await fetch('/api/grok/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ open_tab: false }),
    });
    const first = await start.json();
    if (first.logged_in || first.has_token) {
      show('import');
      return;
    }
    // grok login can take up to a minute to print the auth URL.
    for (let i = 0; i < 240; i++) {
      const poll = await fetch('/api/grok/login', { cache: 'no-store' });
      const status = await poll.json();
      if (status.logged_in || status.has_token) {
        show('import');
        return;
      }
      if (status.url) {
        // Same tab, same browser. Native code returns here after the callback.
        location.assign(status.url);
        return;
      }
      if (status.done && status.error) {
        setError('account-error', status.error);
        break;
      }
      await sleep(250);
    }
    if (!document.getElementById('account-error')?.textContent) {
      setError('account-error', 'Sign-in did not open. Try again.');
    }
  } catch (err) {
    setError('account-error', 'Could not start Grok sign-in.');
    console.error(err);
  }
  if (button) {
    button.disabled = false;
    button.textContent = 'Continue with Grok';
  }
}

const KNOWN_MAC_BROWSERS = [
  { name: 'Safari', profileName: 'Bookmarks on this Mac', locked: true },
  { name: 'Google Chrome', profileName: 'Bookmarks on this Mac', locked: true },
];

function browserLabel(browser) {
  return (browser.name || '').toLowerCase();
}

async function loadBrowsers() {
  const list = document.getElementById('browser-list');
  if (!list) return;
  list.textContent = 'Looking for Safari and other browsers…';
  let detected = [];
  try {
    const res = await fetch('/api/import/browsers', { cache: 'no-store' });
    if (!res.ok) throw new Error('import list unavailable');
    const data = await res.json();
    detected = Array.isArray(data.browsers) ? data.browsers : [];
  } catch (err) {
    console.error(err);
  }
  const seen = new Set(detected.map(browserLabel));
  const offered = detected.slice();
  for (const known of KNOWN_MAC_BROWSERS) {
    if (!seen.has(browserLabel(known))) {
      offered.unshift({ ...known, index: -1 });
    }
  }
  state.browsers = offered;
  list.replaceChildren();
  if (!offered.length) {
    list.textContent = 'No other browsers were found. You can skip this step.';
    return;
  }
  offered.forEach((browser, i) => {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'choice' + (i === 0 ? ' selected' : '');
    button.innerHTML = `<strong></strong><small></small>`;
    button.querySelector('strong').textContent = browser.name || 'Browser';
    const locked = browser.index < 0;
    button.querySelector('small').textContent = locked
      ? 'macOS is hiding this. Allow Xplor in Full Disk Access, then check again.'
      : (browser.profileName || 'Default profile');
    button.addEventListener('click', () => {
      state.browserIndex = browser.index;
      state.browserLocked = locked;
      list.querySelectorAll('.choice').forEach((el) => el.classList.remove('selected'));
      button.classList.add('selected');
    });
    list.appendChild(button);
  });
  state.browserIndex = offered[0].index;
  state.browserLocked = offered[0].index < 0;
}

async function openPrivacySettings() {
  try {
    const res = await fetch('/api/system/privacy', { method: 'POST' });
    if (res.ok) return;
  } catch { /* older build: tell the user where to click */ }
  setError('import-error', 'Open System Settings → Privacy & Security → Full Disk Access, allow Xplor, then click Check again.');
}

async function runImport() {
  setError('import-error', '');
  const button = document.getElementById('do-import');
  if (state.browserLocked) {
    setError('import-error', 'macOS is hiding that browser. Allow Xplor in Full Disk Access, then check again.');
    openPrivacySettings();
    return;
  }
  if (!state.browsers.length) {
    show('favorites');
    return;
  }
  if (button) button.disabled = true;
  try {
    const res = await fetch('/api/import', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        index: state.browserIndex,
        bookmarks: document.getElementById('import-bookmarks').checked,
        history: document.getElementById('import-history').checked,
        passwords: document.getElementById('import-passwords').checked,
        search: false,
      }),
    });
    const start = await res.json();
    if (!res.ok || start.ok === false) {
      setError('import-error', start.error || 'Import did not start.');
      if (button) button.disabled = false;
      return;
    }
    for (let i = 0; i < 120; i++) {
      const poll = await fetch('/api/import/status', { cache: 'no-store' });
      const status = await poll.json();
      if (status.done) {
        if (!status.ok && status.error) setError('import-error', status.error);
        break;
      }
      await sleep(500);
    }
  } catch (err) {
    console.error(err);
    setError('import-error', 'Import failed. You can skip and continue.');
    if (button) button.disabled = false;
    return;
  }
  show('favorites');
}

function renderApps() {
  const grid = document.getElementById('app-grid');
  if (!grid) return;
  grid.replaceChildren();
  APPS.forEach((app) => {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'choice' + (state.apps.has(app.id) ? ' selected' : '');
    button.innerHTML = `<strong></strong><small></small>`;
    button.querySelector('strong').textContent = app.label;
    button.querySelector('small').textContent = 'Pin to sidebar';
    button.addEventListener('click', () => {
      if (state.apps.has(app.id)) state.apps.delete(app.id);
      else state.apps.add(app.id);
      button.classList.toggle('selected', state.apps.has(app.id));
    });
    grid.appendChild(button);
  });
}

function renderTones() {
  const grid = document.getElementById('tone-grid');
  if (!grid) return;
  grid.replaceChildren();
  TONES.forEach((tone) => {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'tone' + (tone.color === state.theme ? ' selected' : '');
    button.innerHTML = `<span class="swatch"></span><strong></strong>`;
    button.querySelector('.swatch').style.background = tone.color;
    button.querySelector('strong').textContent = tone.name;
    button.addEventListener('click', () => {
      state.theme = tone.color;
      document.documentElement.style.setProperty('--accent', tone.color);
      grid.querySelectorAll('.tone').forEach((el) => el.classList.remove('selected'));
      button.classList.add('selected');
    });
    grid.appendChild(button);
  });
}

function selectedApps() {
  return APPS.filter((app) => state.apps.has(app.id)).map((app) => ({
    id: app.id,
    label: app.label,
    url: app.url,
  }));
}

async function finish(makeDefault) {
  const payload = {
    welcome_completed: true,
    onboarding_version: 2,
    pinned_apps: selectedApps(),
    theme_color: state.theme,
  };
  await saveSettings(payload);
  if (makeDefault) {
    try {
      const res = await fetch('/api/default-browser', { method: 'POST' });
      const data = await res.json();
      if (data && data.is_default === false && data.error) {
        setError('default-error', data.error);
      }
    } catch (err) {
      console.error(err);
    }
  }
  location.assign('/search');
}

async function prepareDefault() {
  const note = document.getElementById('default-note');
  try {
    const res = await fetch('/api/default-browser', { cache: 'no-store' });
    if (!res.ok) return;
    const data = await res.json();
    if (data.is_default && note) {
      note.textContent = 'Xplor is already the default browser. The sidebar will use a blurred window with your apps pinned at the top left.';
    }
  } catch { /* the button still completes onboarding */ }
}

document.getElementById('intro-next')?.addEventListener('click', () => show('account'));
document.addEventListener('keydown', (event) => {
  if (state.step !== 'intro') return;
  if (event.key !== 'Enter' && event.key !== 'ArrowRight') return;
  event.preventDefault();
  show('account');
});
document.getElementById('sign-in')?.addEventListener('click', startGrokLogin);
document.getElementById('skip-sign-in')?.addEventListener('click', () => show('import'));
document.getElementById('do-import')?.addEventListener('click', runImport);
document.getElementById('skip-import')?.addEventListener('click', () => show('favorites'));
document.getElementById('recheck-import')?.addEventListener('click', () => loadBrowsers());
document.getElementById('save-apps')?.addEventListener('click', () => show('theme'));
document.getElementById('save-theme')?.addEventListener('click', () => show('default'));
document.getElementById('make-default')?.addEventListener('click', () => finish(true));
document.getElementById('skip-default')?.addEventListener('click', () => finish(false));

renderApps();
renderTones();

const params = new URLSearchParams(location.search);
const requested = params.get('step');
if (params.get('failed') === '1') {
  setError('account-error', 'Sign-in did not finish. Try again, or skip for now.');
}
show(STEPS.includes(requested) ? requested : 'intro');
if (state.step === 'intro') document.getElementById('intro-next')?.focus();

document.querySelector('.slogan') && (document.querySelector('.slogan').textContent = SLOGAN);
