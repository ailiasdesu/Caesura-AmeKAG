// @vitest-environment jsdom
// End-to-end smoke test for the Caesura web player main.mjs UI layer.
// Drives the REAL main.mjs against a real jsdom DOM with a real wasmoon
// engine, mocking only the network (fetch) and pinning the local wasm so
// the fake browser can boot the engine offline. Each case interacts with
// the actual DOM controls (Run / Advance / Auto / save panel / backlog) and
// asserts on the visible status + DOM output — not on bridge calls.
//
// One page load per file (a single browser session), with ordered cases so
// later cases build on the parked scene, exactly like a user would.
import { describe, it, expect, beforeAll } from 'vitest'
import { readFileSync, existsSync, statSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))
const root = join(here, '..')
const wasmFile = join(here, 'node_modules', 'wasmoon', 'dist', 'glue.wasm')

// Map web/player fetch URLs (scripts base / demo / cache / assets) to real
// files so the player boots without a server. scripts-index.json lives in
// web/, everything else under the repo root (the vite publicDir layout).
function resolvePathFromUrl(urlStr) {
  let pathname
  try { pathname = new URL(urlStr, 'http://local').pathname } catch { pathname = urlStr }
  if (pathname === '/scripts/index.json') return join(here, 'scripts-index.json')
  for (const dir of ['scripts', 'demo', 'cache', 'assets']) {
    const prefix = '/' + dir + '/'
    if (pathname.startsWith(prefix)) return join(root, dir, pathname.slice(prefix.length))
  }
  return null
}

function makeFetch() {
  return async (input) => {
    const url = typeof input === 'string' ? input : String(input?.url ?? '')
    const p = resolvePathFromUrl(url)
    if (p && existsSync(p) && statSync(p).isFile()) {
      const text = () => readFileSync(p, 'utf8')
      return {
        ok: true,
        status: 200,
        text,
        json: async () => JSON.parse(text()),
        arrayBuffer: async () => readFileSync(p).buffer,
      }
    }
    return { ok: false, status: 404, text: async () => '', json: async () => ({}), arrayBuffer: async () => new ArrayBuffer(0) }
  }
}

// ---- DOM fixture: mirrors web/index.html body (minus the module <script>) ----
function setupDom() {
  document.head.innerHTML = '<title>caesura-e2e</title>'
  document.body.innerHTML = [
    '<div class="controls">',
    '  <select id="scene"></select>',
    '  <button id="run">▶ Run Scene</button>',
    '  <button id="advance">Click to Advance</button>',
    '  <button id="auto">⏩ Auto</button>',
    '  <span id="status"></span>',
    '</div>',
    '<div class="controls settings-bar">',
    '  <label>Lang <select id="settings-lang"><option value="en">English</option><option value="zh">中文</option><option value="zh-TW">繁體中文</option><option value="ja">日本語</option></select></label>',
    '  <label>Auto <input id="settings-auto" type="checkbox" /></label>',
    '  <label>Speed <input id="settings-speed" type="range" min="1" max="80" value="20" /></label>',
    '  <label>BGM <input id="settings-bgm" type="range" min="0" max="1" step="0.05" value="1" /></label>',
    '  <label>SE <input id="settings-se" type="range" min="0" max="1" step="0.05" value="1" /></label>',
    '  <label>Voice <input id="settings-voice" type="range" min="0" max="1" step="0.05" value="1" /></label>',
    '</div>',
    '<div id="stage"></div>',
    '<div class="status-line">Audio: <span id="audio-status">—</span></div>',
    '<div class="endings-wrap"><div class="backlog-title">Endings (<span id="endings-count">0</span>)</div><div id="endings"></div></div>',
    '<div class="saves-wrap">',
    '  <div class="backlog-title">Save Slots (<span id="saves-count">0</span>)</div>',
    '  <div class="saves-actions"><input id="save-slot" type="number" min="0" max="99" value="1" /><button id="save-now">💾 Save Current</button><button id="refresh-slots">⟳ Refresh</button></div>',
    '  <div id="saves"></div>',
    '</div>',
    '<div class="backlog-wrap"><div class="backlog-title">Backlog (<span id="backlog-count">0</span>)</div><div id="backlog"></div></div>',
    '<div id="log"></div>',
  ].join(String.fromCharCode(10))
}

// Poll until predicate() is truthy, else throw after timeout.
async function waitFor(predicate, label, timeout = 15000) {
  const start = Date.now()
  while (Date.now() - start < timeout) {
    const v = predicate()
    if (v) return v
    await new Promise((r) => setTimeout(r, 40))
  }
  throw new Error('waitFor timed out: ' + label)
}

const $ = (id) => document.getElementById(id)
const statusText = () => ($('status') ? $('status').textContent : '')

// status settles on an asynchronous runScene/advance once it stops being
// the transient "running…" placeholder.
async function waitStatus(fragment, label, timeout = 60000) {
  return waitFor(() => statusText().includes(fragment), label, timeout)
}

beforeAll(async () => {
  // Pin the local wasm so main.mjs boots offline (when unset, the global is
  // ignored and the production browser path keeps wasmoon's CDN default).
  globalThis.__CAESURA_WASM_FILE__ = wasmFile
  setupDom()
  // Fake the network for scripts / story bundle / demo sources.
  globalThis.fetch = makeFetch()
  if (typeof globalThis.requestAnimationFrame !== 'function') {
    globalThis.requestAnimationFrame = (cb) => setTimeout(() => cb(Date.now()), 16)
  }
  await import('./main.mjs')
  // The module auto-runs galgame_demo.ks; wait for it to park on a [p].
  await waitFor(() => /^parked: /i.test(statusText()), 'initial scene parks', 150000)
}, 180000)

// Pick the demo scene the module auto-ran so run/advance hit the same
// parked cursor (main.mjs reads #scene.value on Run/Advance).
const pickDemoScene = () => {
  const sel = $('scene')
  if (sel.value !== 'galgame_demo.ks') {
    sel.value = 'galgame_demo.ks'
    sel.dispatchEvent(new Event('change', { bubbles: true }))
  }
}

describe('main.mjs E2E (real DOM + real wasmoon)', () => {
  it('populates the scene picker and Run parks on a WAIT page', async () => {
    expect($('scene').options.length).toBeGreaterThan(0)
    pickDemoScene()
    // kick off a fresh run of the demo scene through the Run button
    $('run').click()
    await waitStatus('parked: ', 'run parks')
    const st = statusText()
    expect(st).toMatch(/^parked: (WAIT:|DONE:)/)
    // first page is visible in the stage
    const msg = document.querySelector('#stage .caesura-message')
    expect(msg && msg.textContent.length).toBeGreaterThan(0)
  }, 90000)

  it('Advance advances the parked scene by one page and updates status', async () => {
    const before = statusText()
    $('advance').click()
    await waitFor(() => {
      const s = statusText()
      return s.startsWith('advance:') && s !== before
    }, 'advance updates status', 60000)
    expect(statusText()).toMatch(/^advance: (WAIT:|DONE:)/)
  }, 90000)

  it('Auto toggles on, auto-advances, then can be turned off', async () => {
    const autoBtn = $('auto')
    autoBtn.click() // on
    expect(autoBtn.textContent).toContain('⏸')
    const before = statusText()
    // auto schedule advances every ~1200ms; wait for one automatic advance
    await waitFor(() => statusText() !== before && statusText().startsWith('advance:'), 'auto-advance fires', 10000)
    autoBtn.click() // off
    expect(autoBtn.textContent).toContain('⏩')
  }, 30000)

  it('saves the current position into a slot, lists it, and Load restores', async () => {
    $('save-slot').value = '3'
    $('save-now').click()
    // the save handler is async; poll for the slot appearing in the list
    await waitFor(() => Number($('saves-count').textContent) > 0, 'save slot listed', 30000)
    expect($('saves-count').textContent).toBe('1')
    expect($('saves').textContent).toContain('galgame_demo.ks')

    // click the Load button rendered in the save row
    const loadBtn = document.querySelector('#saves .save-entry button')
    expect(loadBtn).toBeTruthy()
    loadBtn.click()
    await waitStatus('load: ', 'load slot restores', 60000)
    expect(statusText()).toMatch(/^load: (WAIT:|DONE:)/)
  }, 120000)

  it('backlog accumulates across advances and the DOM mirrors core state', async () => {
    pickDemoScene()
    $('run').click()
    await waitStatus('parked: ', 'run parks')
    const countEl = $('backlog-count')
    const n0 = Number(countEl.textContent)
    // Drive real Advance clicks: each committed page pushes a backlog entry
    // (min 2 clicks) and the count must visibly grow.
    $('advance').click()
    await waitFor(() => Number(countEl.textContent) > n0, 'backlog grows on first advance', 60000)
    const n1 = Number(countEl.textContent)
    expect(n1).toBeGreaterThan(n0)
    $('advance').click()
    await waitFor(() => Number(countEl.textContent) >= n1, 'backlog keeps growing on next advance', 60000)
    // The sync clears + refills the container, so every DOM row maps to a
    // current core backlog entry — no stale/orphan rows remain.
    const nFinal = Number(countEl.textContent)
    await waitFor(() => document.querySelectorAll('#backlog .backlog-entry').length === nFinal, 'DOM backlog matches core count', 5000)
    expect(document.querySelectorAll('#backlog .backlog-entry').length).toBe(nFinal)
  }, 90000)
})

