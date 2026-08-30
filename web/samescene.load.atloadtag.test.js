// @vitest-environment jsdom
// t69: same-scene direct-load guard must NOT skip the saved token (atLoadTag
// discriminator, native t62 mirror). A pause-point direct SaveCommands.load
// of the CURRENT scene resumes EXACTLY at the saved cursor; the round-94
// +1 skip applies only when the token at the saved cursor IS a [load] tag
// (a self-referential [save]->[load] sequence). Without the discriminator
// the guard blindly cursor+1s every same-scene load, dropping the saved
// token's replay (native diverge, t64 note 2).
import { describe, it, expect, beforeAll, beforeEach } from 'vitest'
import { readFileSync, existsSync, statSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))
const root = join(here, '..')

// Map web/player fetch URLs (scripts base / demo / cache / assets) to real
// files so the player boots without a server. scripts-index.json lives in
// web/, everything else under the repo root (the vite publicDir layout).
// Copied 1:1 from e2e.main.test.js (t122: mock fidelity alignment — the
// former fileFetch mapped /scripts/index.json to a nonexistent repo path and
// returned fake json on 404; this resolver maps it to web/scripts-index.json
// exactly, the four prefixes to repo files, and every response carries the
// full text/json/arrayBuffer shape).
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
const store = new Map()
const storageBackend = { get: (k) => (store.has(k) ? store.get(k) : null), set: (k, v) => { store.set(k, v); return true }, del: (k) => { store.delete(k) } }
const SAVE_PREFIX = 'caesura.save.'
const clearSlots = () => { for (const k of [...store.keys()]) if (k.startsWith(SAVE_PREFIX)) store.delete(k) }
let player = null
beforeAll(async () => {
  setupDom()
  // CI checkouts never carry cache/story/story.lua; opt into dev mode like
  // the green wasm e2e suites (main.mjs bundle-missing fallback is DEV-gated,
  // b6bfcd98). Deferred bridge import: the green e2e files load the
  // bridge/wasmoon graph only inside beforeAll (e2e.main:119) — never at
  // collection time (t122/t120: collection-phase static import of the full
  // graph was the only structural delta vs the three CI-green e2e files).
  globalThis.__CAESURA_DEV__ = true
  globalThis.fetch = makeFetch()
  if (typeof globalThis.requestAnimationFrame !== 'function') {
    globalThis.requestAnimationFrame = (cb) => setTimeout(() => cb(Date.now()), 16)
  }
  const { createPlayer } = await import('./bridge.js')
  player = await createPlayer({ scriptsBase: 'http://localhost/scripts/', wasmFile: join(here, 'node_modules', 'wasmoon', 'dist', 'glue.wasm') })
})
beforeEach(() => { clearSlots(); player.core.draws = []; player.core.backlog.length = 0 })
const backlogOf = () => (player.core.backlog || []).map((b) => b.text).join(' | ')
const NL = String.fromCharCode(10)
// DOM fixture: mirrors web/index.html body (minus the module <script>) —
// copied 1:1 from e2e.main.test.js setupDom (t129: the ONLY untested
// behavioral delta vs the green e2e files is the presence of this fixture
// before createPlayer boots; without it the worker reached bridge-imported
// then stalled inside createPlayer (t124 marks)).
function setupDom() {
  document.head.innerHTML = '<title>caesura-e2e</title>'
  document.body.innerHTML = [
    '<div class="controls">',
    '  <select id="scene"></select>',
    '  <button id="run">&#9654; Run Scene</button>',
    '  <button id="advance">Click to Advance</button>',
    '  <button id="auto">&#9193; Auto</button>',
    '  <span id="status"></span>',
    '</div>',
    '<div class="controls settings-bar">',
    '  <label>Lang <select id="settings-lang"></select></label>',
    '  <label>Auto <input id="settings-auto" type="checkbox" /></label>',
    '  <label>Speed <input id="settings-speed" type="range" min="1" max="80" value="20" /></label>',
    '  <label>BGM <input id="settings-bgm" type="range" min="0" max="1" step="0.05" value="1" /></label>',
    '  <label>SE <input id="settings-se" type="range" min="0" max="1" step="0.05" value="1" /></label>',
    '  <label>Voice <input id="settings-voice" type="range" min="0" max="1" step="0.05" value="1" /></label>',
    '</div>',
    '<div id="stage"></div>',
    '<div class="status-line">Audio: <span id="audio-status">&#8212;</span></div>',
    '<div class="endings-wrap"><div class="backlog-title">Endings (<span id="endings-count">0</span>)</div><div id="endings"></div></div>',
    '<div class="saves-wrap">',
    '  <div class="backlog-title">Save Slots (<span id="saves-count">0</span>)</div>',
    '  <div class="saves-actions"><input id="save-slot" type="number" min="0" max="99" value="1" /><button id="save-now">&#128190; Save Current</button><button id="refresh-slots">&#8635; Refresh</button></div>',
    '  <div id="saves"></div>',
    '</div>',
    '<div class="backlog-wrap"><div class="backlog-title">Backlog (<span id="backlog-count">0</span>)</div><div id="backlog"></div></div>',
    '<div id="log"></div>',
  ].join(String.fromCharCode(10))
}

const SCENE = [
  '[ch name="N" text="ANCHOR"]',
  '[save slot=9]',
  '[ch name="N" text="IN-BETWEEN"]',
  '[set f.rtReady = 1]',
  '[p]',
  '[ch name="N" text="POST"]',
  '[p]',
  '[end]',
].join(NL)

describe('web same-scene direct load (atLoadTag discriminator, t69)', () => {
  it('pause-point direct load of the same scene replays the saved token (no skip)', async () => {
    // 1) park at the first [ch] (the pause point) and save there.
    let out = await player.runScene(SCENE, 'demo/sr_rt.ks', { maxFrames: 60000, autoClick: false, sceneSources: { 'demo/sr_rt.ks': SCENE } })
    expect(out.startsWith('WAIT:')).toBe(true)
    expect(await player.saveCurrent(9)).toBe(true)
    const s = player.listSlots().find((x) => x.slot === 9)
    expect(s).toBeTruthy()
    // The saved cursor is the [ch] ANCHOR token (a NON-load token); the
    // demo/-prefixed scene name makes ctx.current_scene == the stored
    // scene_path, so the guard's same-scene branch genuinely fires.
    expect(s.token).toBe(1)
    // 2) mutate state: click through ANCHOR to the [p] park.
    await player.click()
    out = await player.runScene(SCENE, 'demo/sr_rt.ks', { maxFrames: 60000, advance: true, advanceScene: 'demo/sr_rt.ks', autoClick: false, sceneSources: { 'demo/sr_rt.ks': SCENE } })
    expect(out.startsWith('WAIT:')).toBe(true)
    // 3) direct same-scene load from the pause point (golden-driver path:
    //    SaveCommands.load — the exact [load] tag handler).
    const lr = await player.lua.doString(
      "local S=require('kag.commands.save'); local c=_G.__CTXREF; local ok,err=pcall(function() S.load(c,{slot=9}) end); return tostring(ok)..'|'..tostring(err)")
    expect(lr).toBe('true|nil')
    // 4) drive the restored scene to the end.
    out = await player.runScene(SCENE, 'demo/sr_rt.ks', { maxFrames: 60000, advance: true, advanceScene: 'demo/sr_rt.ks', autoClick: true, sceneSources: { 'demo/sr_rt.ks': SCENE } })
    expect(out.startsWith('DONE:')).toBe(true)
    // 5) native-mirror semantics: the resume is EXACT (no load tag at the
    //    saved cursor), so the saved token itself replays. [load] also
    //    restores the saved backlog (empty at the save point), so the fixed
    //    guard yields exactly ONE ANCHOR (the replay), while the pre-fix
    //    cursor+1 guard resumed AFTER ANCHOR and yielded ZERO. The 1-vs-0
    //    gap is what this assertion discriminates.
    const texts = backlogOf()
    const anchors = texts.split('ANCHOR').length - 1
    // Saved backlog was empty -> the ONLY ANCHOR must come from the replay
    // (the exact-restore of the saved token; the pre-fix guard skipped it).
    expect(anchors).toBe(1)
    expect(texts).toContain('IN-BETWEEN')
  }, 120000)

  it('self-referential [save]->[load] still advances past the load (no loop)', async () => {
    const SELF = [
      '[ch name="N" text="SELF"]',
      '[save slot=9]',
      '[load slot=9]',
      '[ch name="N" text="SELF-END"]',
      '[p]',
      '[end]',
    ].join(NL)
    // Bounded loop guard: the run must END (not exhaust) even if the guard
    // regresses to an exact restore that re-executes the [load] forever.
    let out = await player.runScene(SELF, 'demo/sr_self.ks', { maxFrames: 4000, autoClick: true, sceneSources: { 'demo/sr_self.ks': SELF } })
    expect(out.startsWith('DONE:')).toBe(true)
    expect(backlogOf()).toContain('SELF-END')
  }, 120000)
})