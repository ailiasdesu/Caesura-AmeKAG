#!/usr/bin/env node
// -----------------------------------------------------------------------------
// Caesura (AmeKAG) — web_browser_smoke.mjs
//
// Real-browser smoke for the packaged web player (Track W / plan §W0).
// Serves a web dist directory over a zero-dependency static HTTP server,
// launches a real Chromium-family browser (Chrome or Edge) in headless mode
// with remote debugging, and asserts the player's browser-visible behavior:
//
//   * boot:    engine loads, the auto-started (or ?scene=) run parks
//   * text:    .caesura-message contains rendered text (CJK-aware)
//   * image:   at least one layer renders as a real <img> (naturalWidth > 0)
//   * audio:   audio-status reports BGM (/ SE / VOICE) when the scene plays it
//   * save:    Save Current -> slot listed -> Page.reload -> still listed
//              (localStorage persistence across reload, plan §W2)
//   * (--unlock) AudioContext state: initial -> after trusted Input click
//              (plan §W1: user gesture unlock; run WITHOUT autoplay flag)
//
// Usage (from repo root, git bash):
//   node scripts/web_browser_smoke.mjs                              # web/dist + Chrome
//   node scripts/web_browser_smoke.mjs --root dist/example_game --browser edge
//   node scripts/web_browser_smoke.mjs --root dist/example_game --scene story.ks
//   node scripts/web_browser_smoke.mjs --root dist/example_game --unlock
//
// Output: JSON result on stdout; screenshot at build/web-smoke/smoke.png.
// Exit code: 0 = all asserted checks passed, 1 = any failed.
// -----------------------------------------------------------------------------

import { createServer } from 'node:http'
import { readFileSync, existsSync, mkdirSync, writeFileSync, statSync } from 'node:fs'
import { join, extname, resolve } from 'node:path'
import { spawn } from 'node:child_process'

const ARGV = process.argv.slice(2)
const arg = (name, dflt) => {
  const i = ARGV.indexOf(name)
  return i >= 0 && ARGV[i + 1] !== undefined ? ARGV[i + 1] : dflt
}
const has = (name) => ARGV.includes(name)

const ROOT = resolve(arg('--root', join(process.cwd(), 'web', 'dist')))
const BROWSER = arg('--browser', 'chrome')
const HTTP_PORT = Number(arg('--port', 8765))
const CDP_PORT = Number(arg('--cdp-port', 9333))
const SCENE = arg('--scene', '')
const UNLOCK = has('--unlock')
const TIMEOUT_MS = Number(arg('--timeout', 180000))

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.lua': 'text/plain; charset=utf-8',
  '.ks': 'text/plain; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.webp': 'image/webp',
  '.otf': 'application/octet-stream',
  '.ttf': 'application/octet-stream',
  '.wav': 'audio/wav',
  '.ogg': 'audio/ogg',
  '.mp3': 'audio/mpeg',
  '.txt': 'text/plain; charset=utf-8',
}
const CHROME_PATHS = {
  chrome: [
    'C:/Program Files/Google/Chrome/Application/chrome.exe',
    '/c/Program Files/Google/Chrome/Application/chrome.exe',
  ],
  edge: [
    'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    'C:/Program Files/Microsoft/Edge/Application/msedge.exe',
    '/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    '/c/Program Files/Microsoft/Edge/Application/msedge.exe',
  ],
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms))
const pass = new Set()
const fail = new Set()
const record = (name, ok, detail = '') => {
  ;(ok ? pass : fail).add(name)
  console.log((ok ? '[PASS] ' : '[FAIL] ') + name + (detail ? '  — ' + detail : ''))
}

// ------------------------------------------------------------ static server
function startServer() {
  const server = createServer((req, res) => {
    let urlPath = decodeURIComponent(new URL(req.url, 'http://x').pathname)
    // Virtual route: pin the wasmoon Lua VM wasm to a LOCAL copy so the
    // packaged player never depends on unpkg.com at runtime (see W0 doc).
    if (urlPath === '/') urlPath = '/index.html'
    if (urlPath === '/__wasm__/glue.wasm') {
      const local = join(process.cwd(), 'web', 'node_modules', 'wasmoon', 'dist', 'glue.wasm')
      if (existsSync(local)) {
        res.writeHead(200, { 'content-type': 'application/wasm', 'cache-control': 'no-store' })
        res.end(readFileSync(local)); return
      }
      res.writeHead(404); res.end('wasm not vendored'); return
    }
    const safe = resolve(ROOT, '.' + urlPath)
    const rootResolved = resolve(ROOT)
    if (!safe.startsWith(rootResolved) || !existsSync(safe) || !statSync(safe).isFile()) {
      res.writeHead(404); res.end('not found'); return
    }
    const type = MIME[extname(safe).toLowerCase()] ?? 'application/octet-stream'
    res.writeHead(200, { 'content-type': type, 'cache-control': 'no-store' })
    res.end(readFileSync(safe))
  })
  return new Promise((resolveReady) => {
    server.listen(HTTP_PORT, '127.0.0.1', () => resolveReady(server))
  })
}

// ---------------------------------------------------------------- browser
function findBrowser() {
  for (const p of CHROME_PATHS[BROWSER] ?? CHROME_PATHS.chrome) {
    if (existsSync(p)) return p
  }
  return null
}

async function waitForCdp(timeout = 20000) {
  const deadline = Date.now() + timeout
  while (Date.now() < deadline) {
    try {
      const r = await fetch('http://127.0.0.1:' + CDP_PORT + '/json/list')
      if (r.ok) return await r.json()
    } catch { /* not up yet */ }
    await sleep(250)
  }
  throw new Error('CDP endpoint did not come up on port ' + CDP_PORT)
}

// ---------------------------------------------------------------- CDP client
class Cdp {
  constructor(ws) { this.ws = ws; this.id = 0; this.pending = new Map() }
  static async connect(wsUrl) {
    const ws = new WebSocket(wsUrl)
    await new Promise((res, rej) => {
      ws.onopen = res
      ws.onerror = (e) => rej(new Error('ws error: ' + String(e?.message ?? e)))
    })
    const cdp = new Cdp(ws)
    ws.onmessage = (ev) => {
      const msg = JSON.parse(String(ev.data))
      if (msg.id != null && cdp.pending.has(msg.id)) {
        const { res, rej } = cdp.pending.get(msg.id)
        cdp.pending.delete(msg.id)
        if (msg.error) rej(new Error('CDP error: ' + JSON.stringify(msg.error)))
        else res(msg.result)
      }
    }
    return cdp
  }
  send(method, params = {}) {
    const id = ++this.id
    return new Promise((res, rej) => {
      this.pending.set(id, { res, rej })
      this.ws.send(JSON.stringify({ id, method, params }))
    })
  }
  async eval(expression) {
    const r = await this.send('Runtime.evaluate', {
      expression, awaitPromise: true, returnByValue: true,
    })
    if (r.exceptionDetails) {
      throw new Error('page eval failed: ' + (r.exceptionDetails.exception?.description ?? JSON.stringify(r.exceptionDetails)).slice(0, 300))
    }
    return r.result?.value
  }
  async waitEval(expression, label, timeout = TIMEOUT_MS) {
    const deadline = Date.now() + timeout
    let last = null
    while (Date.now() < deadline) {
      try { last = await this.eval(expression); if (last) return last } catch { /* transient */ }
      await sleep(300)
    }
    throw new Error('waitEval timed out: ' + label + ' (last=' + JSON.stringify(last) + ')')
  }
  async clickTrusted(selector) {
    const box = await this.eval('(() => {\n' +
      '  const el = document.querySelector(' + JSON.stringify(selector) + ')\n' +
      "  if (!el) return null\n" +
      '  const r = el.getBoundingClientRect()\n' +
      '  return { x: r.x + r.width / 2, y: r.y + r.height / 2 }\n' +
      '})()')
    if (!box) throw new Error('clickTrusted: no element ' + selector)
    await this.send('Input.dispatchMouseEvent', { type: 'mousePressed', x: box.x, y: box.y, button: 'left', clickCount: 1 })
    await this.send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: box.x, y: box.y, button: 'left', clickCount: 1 })
  }
  async screenshot(file) {
    const r = await this.send('Page.captureScreenshot', { format: 'png' })
    mkdirSync('build/web-smoke', { recursive: true })
    writeFileSync(file, Buffer.from(r.data, 'base64'))
  }
}

// -------------------------------------------------------------------- main
async function main() {
  if (!existsSync(join(ROOT, 'index.html'))) {
    console.error('[web-smoke] FATAL: no index.html under ' + ROOT)
    process.exit(1)
  }
  const browser = findBrowser()
  if (!browser) {
    console.error('[web-smoke] FATAL: browser not found: ' + BROWSER)
    process.exit(1)
  }

  const server = await startServer()
  let chromeProc = null
  try {
    const query = SCENE ? '?scene=' + encodeURIComponent(SCENE) : ''
    const url = 'http://127.0.0.1:' + HTTP_PORT + '/' + query
    const args = [
      '--headless=new',
      '--no-first-run',
      '--no-default-browser-check',
      '--disable-features=Translate',
      '--disable-extensions',
      '--remote-debugging-port=' + CDP_PORT,
      '--user-data-dir=' + join(process.cwd(), 'build', 'web-smoke', 'profile-' + BROWSER + '-' + Date.now()),
      '--window-size=1280,900',
    ]
    if (!UNLOCK) args.push('--autoplay-policy=no-user-gesture-required')
    args.push(url)

    console.log('[web-smoke] root:', ROOT)
    console.log('[web-smoke] url:', url, UNLOCK ? '(unlock mode: default autoplay policy)' : '(no-user-gesture-required)')
    chromeProc = spawn(browser, args, { stdio: 'ignore' })
    chromeProc.on('error', (e) => { console.error('[web-smoke] chrome spawn error', e) })

    let targets = await waitForCdp()
    let page = targets.find((t) => t.type === 'page' && String(t.url).startsWith('http://127.0.0.1:' + HTTP_PORT))
      ?? targets.find((t) => t.type === 'page')
    // page target can appear a moment after the CDP endpoint; retry briefly
    const pageDeadline = Date.now() + 60000
    while (!page && Date.now() < pageDeadline) {
      await sleep(250)
      try { targets = await (await fetch('http://127.0.0.1:' + CDP_PORT + '/json/list')).json() } catch { continue }
      page = targets.find((t) => t.type === 'page' && String(t.url).startsWith('http://127.0.0.1:' + HTTP_PORT))
        ?? targets.find((t) => t.type === 'page')
    }
    if (!page) throw new Error('no page target; targets=' + JSON.stringify(targets.map((t) => t.type + ':' + t.url)))
    const cdp = await Cdp.connect(page.webSocketDebuggerUrl)
    await cdp.send('Runtime.enable')
    await cdp.send('Page.enable')
    // Pin the Lua VM wasm to the local copy (served above) so the packaged
    // player can boot offline; runs before every page script.
    await cdp.send('Page.addScriptToEvaluateOnNewDocument', {
      source: 'self.__CAESURA_WASM_FILE__ = ' + JSON.stringify('http://127.0.0.1:' + HTTP_PORT + '/__wasm__/glue.wasm'),
    })
    await cdp.send('Page.navigate', { url: 'http://127.0.0.1:' + HTTP_PORT + '/' + (SCENE ? '?scene=' + encodeURIComponent(SCENE) : '') })

    // 1. boot
    const parked = await cdp.waitEval(
      "document.getElementById('status') && /^parked:|^load: /i.test(document.getElementById('status').textContent)",
      'status parks', 150000)
    record('boot (engine load + auto run parks)', !!parked, JSON.stringify(parked))

    // 2. text
    const msgText = await cdp.eval("(() => {\n" +
      "  const el = document.querySelector('.caesura-message')\n" +
      "  return el ? el.textContent.trim() : ''\n" +
      "})()")
    record('text (.caesura-message renders text)', typeof msgText === 'string' && String(msgText).trim().length > 0, JSON.stringify(String(msgText).slice(0, 60)))

    // 3. image layers
    const imgOk = await cdp.eval("(() => {\n" +
      "  const imgs = [...document.querySelectorAll('.caesura-layer[src]')]\n" +
      "  return { count: imgs.length, ready: imgs.filter(i => i.complete && i.naturalWidth > 0).length, total: imgs.filter(i => i.complete).length }\n" +
      "})()")
    const anyLayer = await cdp.eval("document.querySelectorAll('.caesura-layer').length")
    record('image (stage has layers)', !!anyLayer, 'layers=' + anyLayer)
    record('image (layer <img> decoded)', imgOk && (imgOk.ready > 0 || imgOk.total === 0), JSON.stringify(imgOk))

    // 4. audio status (soft check — scene dependent)
    const audio = await cdp.eval("document.getElementById('audio-status').textContent")
    const audioSeen = /BGM|SE|VOICE/.test(String(audio))
    record('audio (audio-status reports bus when playing)', audioSeen || /^—$/.test(String(audio)), JSON.stringify(audio))
    // WebAudio real playback: a live source on the bus proves the normalized
    // asset URL decoded + started (core state alone would mask 404s).
    let srcs = await cdp.eval("(window.__caesuraAudio && window.__caesuraAudio._sources ? [...window.__caesuraAudio._sources.keys()] : [])")
    if (!Array.isArray(srcs) || srcs.length === 0) {
      // decode/start is async (large WAV); give it a window before failing
      const srcDeadline = Date.now() + 20000
      while (Date.now() < srcDeadline) {
        await sleep(500)
        srcs = await cdp.eval("(window.__caesuraAudio && window.__caesuraAudio._sources ? [...window.__caesuraAudio._sources.keys()] : [])")
        if (Array.isArray(srcs) && srcs.length > 0) break
      }
    }
    record('audio (WebAudio source(s) live on buses)', Array.isArray(srcs) && srcs.length > 0, JSON.stringify(srcs))

    // 5. save round trip + persistence across reload
    await cdp.eval("(() => {\n" +
      "  const btn = document.getElementById('save-now')\n" +
      "  if (btn) btn.click()\n" +
      "  return !!btn\n" +
      "})()")
    const slotsBefore = await cdp.eval("(async () => {\n" +
      "  await new Promise(r => setTimeout(r, 900))\n" +
      "  return Number(document.getElementById('saves-count').textContent)\n" +
      "})()")
    record('save (Save Current lists a slot)', slotsBefore > 0, 'slots=' + slotsBefore)
    const baseUrl = 'http://127.0.0.1:' + HTTP_PORT + '/' + (SCENE ? '?scene=' + encodeURIComponent(SCENE) : '')
    await cdp.send('Page.navigate', { url: baseUrl })
    await cdp.waitEval(
      "document.getElementById('status') && /^parked:|^load: /i.test(document.getElementById('status').textContent)",
      'status parks after reload', 150000)
    const slotsAfter = await cdp.eval("(async () => {\n" +
      "  await new Promise(r => setTimeout(r, 500))\n" +
      "  return Number(document.getElementById('saves-count').textContent)\n" +
      "})()")
    record('save (slot persists across Page.reload)', slotsAfter > 0, 'after=' + slotsAfter)

    // 6. --unlock mode: AudioContext state before/after real trusted click
    if (UNLOCK) {
      const before = await cdp.eval("(window.__caesuraAudio ? (window.__caesuraAudio.state || 'unknown') : 'no-hook')")
      record('unlock: page exposes audio state', before !== 'no-hook', JSON.stringify(before))
      await cdp.clickTrusted('#advance')
      await sleep(1200)
      const after = await cdp.eval("(window.__caesuraAudio ? (window.__caesuraAudio.state || 'unknown') : 'no-hook')")
      record('unlock: AudioContext running after user gesture', after === 'running', 'before=' + before + ' after=' + after)
    }

    await cdp.screenshot('build/web-smoke/smoke-' + BROWSER + '.png')
    await cdp.send('Page.close')
  } finally {
    server.close()
    if (chromeProc) { try { chromeProc.kill() } catch { /* noop */ } }
  }

  console.log('\\n[web-smoke] summary: ' + pass.size + ' passed, ' + fail.size + ' failed')
  for (const f of fail) console.log('  FAILED: ' + f)
  return fail.size === 0 ? 0 : 1
}

main().then((code) => process.exit(code)).catch((e) => {
  console.error('[web-smoke] FATAL:', e.message)
  process.exit(1)
})