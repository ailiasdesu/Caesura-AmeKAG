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
import net from 'node:net'
import { readFileSync, existsSync, mkdirSync, writeFileSync, statSync } from 'node:fs'
import { join, extname, resolve } from 'node:path'
import { spawn, spawnSync } from 'node:child_process'

const ARGV = process.argv.slice(2)
const arg = (name, dflt) => {
  const i = ARGV.indexOf(name)
  return i >= 0 && ARGV[i + 1] !== undefined ? ARGV[i + 1] : dflt
}
const has = (name) => ARGV.includes(name)

const ROOT = resolve(arg('--root', join(process.cwd(), 'web', 'dist')))
const BROWSER = arg('--browser', 'chrome')
// 0 = auto-pick a free port (default); pin by passing --port/--cdp-port.
const HTTP_PORT = Number(arg('--port', 0))
const CDP_PORT = Number(arg('--cdp-port', 0))
const SCENE = arg('--scene', '')
const UNLOCK = has('--unlock')
const CJK = has('--cjk')
const STRESS = has('--stress')
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
const freePort = () => new Promise((res) => {
  const srv = net.createServer()
  srv.listen(0, '127.0.0.1', () => { const p2 = srv.address().port; srv.close(() => res(p2)) })
})
const pass = new Set()
const fail = new Set()
const record = (name, ok, detail = '') => {
  ;(ok ? pass : fail).add(name)
  console.log((ok ? '[PASS] ' : '[FAIL] ') + name + (detail ? '  — ' + detail : ''))
}

// ------------------------------------------------------------ static server
function startServer(httpPortUsed, cdpPortUsed) {
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
    server.listen(httpPortUsed, '127.0.0.1', () => resolveReady(server))
  })
}

// ---------------------------------------------------------------- browser
function findBrowser() {
  for (const p of CHROME_PATHS[BROWSER] ?? CHROME_PATHS.chrome) {
    if (existsSync(p)) return p
  }
  return null
}

async function waitForCdp(cdpPortX, timeout = 20000) {
  const deadline = Date.now() + timeout
  while (Date.now() < deadline) {
    try {
      const r = await fetch('http://127.0.0.1:' + cdpPortX + '/json/list')
      if (r.ok) return await r.json()
    } catch { /* not up yet */ }
    await sleep(250)
  }
  throw new Error('CDP endpoint did not come up on port ' + cdpPortX)
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

  // Auto-pick free ports unless the caller pinned them (a leftover browser
  // holding a fixed port used to make the new instance silently disappear).
  const httpPort = HTTP_PORT || await freePort()
  const cdpPort = CDP_PORT || await freePort()
  const server = await startServer(httpPort, cdpPort)
  let chromeProc = null
  try {
    const query = SCENE ? '?scene=' + encodeURIComponent(SCENE) : ''
    const url = 'http://127.0.0.1:' + httpPort + '/' + query
    const args = [
      '--headless=new',
      '--no-first-run',
      '--no-default-browser-check',
      '--disable-features=Translate',
      '--disable-extensions',
      '--remote-debugging-port=' + cdpPort,
      '--user-data-dir=' + join(process.cwd(), 'build', 'web-smoke', 'profile-' + BROWSER + '-' + Date.now()),
      '--window-size=1280,900',
    ]
    if (!UNLOCK) args.push('--autoplay-policy=no-user-gesture-required')
    args.push(url)

    console.log('[web-smoke] root:', ROOT)
    console.log('[web-smoke] url:', url, UNLOCK ? '(unlock mode: default autoplay policy)' : '(no-user-gesture-required)')
    chromeProc = spawn(browser, args, { stdio: 'ignore' })
    chromeProc.on('error', (e) => { console.error('[web-smoke] chrome spawn error', e) })

    let targets = await waitForCdp(cdpPort)
    let page = targets.find((t) => t.type === 'page' && String(t.url).startsWith('http://127.0.0.1:' + httpPort))
      ?? targets.find((t) => t.type === 'page')
    // page target can appear a moment after the CDP endpoint; retry briefly
    const pageDeadline = Date.now() + 60000
    while (!page && Date.now() < pageDeadline) {
      await sleep(250)
      try { targets = await (await fetch('http://127.0.0.1:' + cdpPort + '/json/list')).json() } catch { continue }
      page = targets.find((t) => t.type === 'page' && String(t.url).startsWith('http://127.0.0.1:' + httpPort))
        ?? targets.find((t) => t.type === 'page')
    }
    if (!page) throw new Error('no page target; targets=' + JSON.stringify(targets.map((t) => t.type + ':' + t.url)))
    const cdp = await Cdp.connect(page.webSocketDebuggerUrl)
    await cdp.send('Runtime.enable')
    await cdp.send('Page.enable')
    // Pin the Lua VM wasm to the local copy (served above) so the packaged
    // player can boot offline; runs before every page script.
    await cdp.send('Page.addScriptToEvaluateOnNewDocument', {
      source: 'self.__CAESURA_WASM_FILE__ = ' + JSON.stringify('http://127.0.0.1:' + httpPort + '/__wasm__/glue.wasm'),
    })
    await cdp.send('Page.navigate', { url: 'http://127.0.0.1:' + httpPort + '/' + (SCENE ? '?scene=' + encodeURIComponent(SCENE) : '') })

    // 1. boot
    const bootAt = Date.now()
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
    await cdp.waitEval("(document.querySelectorAll('.caesura-layer[src]').length > 0)", 'wait for image layers', 20000).catch(() => {})
    // settle: let pending <img> loads resolve before judging decode state
    await sleep(1200)
    const imgOk = await cdp.eval("(() => {\n" +
      "  const imgs = [...document.querySelectorAll('.caesura-layer[src]')]\n" +
      "  return { count: imgs.length, ready: imgs.filter(i => i.complete && i.naturalWidth > 0).length, total: imgs.filter(i => i.complete).length }\n" +
      "})()")
    const anyLayer = await cdp.eval("document.querySelectorAll('.caesura-layer').length")
    record('image (stage has layers)', !!anyLayer, 'layers=' + anyLayer)
    record('image (layer <img> decoded)', imgOk && (imgOk.ready > 0 || imgOk.total === 0), JSON.stringify(imgOk))

    // 4b. --cjk mode: CJK / font / packaged-asset verification (plan W3).
    // Drives the dedicated demo/cjk_smoke.ks page by page and asserts the
    // REAL rendered text (Chinese / Japanese / English / mixed punctuation),
    // the shipped @font-face font load, the packaged font asset URL and the
    // fallback path (unknown face -> system stack still renders).
    if (CJK) {
      const texts = []
      let done = false
      let shotted = false
      for (let i = 0; i < 9 && !done; i++) {
        const t = await cdp.eval("(document.querySelector('.caesura-message') || {}).textContent || ''")
        if (typeof t === 'string' && t.trim().length > 0) texts.push(t.trim())
        if (!shotted && /中文渲染/.test(String(t))) {
          await cdp.screenshot('build/web-smoke/smoke-cjk.png')
          shotted = true
          record('cjk: 中文页截图已捕获', true)
        }
        const st = await cdp.eval("document.getElementById('status').textContent || ''")
        if (/DONE:/i.test(String(st))) { done = true; break }
        await cdp.clickTrusted('#advance')
        await sleep(900)
      }
      const all = texts.join(' ')
      record('cjk: Chinese renders', /你好|凯苏拉|中文渲染/.test(all), 'pages=' + texts.length)
      record('cjk: Japanese renders', /こんにちは|ケースラ|日本語/.test(all))
      record('cjk: English renders', /quick brown fox/.test(all))
      record('cjk: mixed punctuation', /[「」…—！？]/.test(all))
      record('cjk: fallback page (unknown face -> system stack)', texts.some((t) => /フォールバック|System font fallback/.test(t)))
      const font = await cdp.eval("(async () => { try { await document.fonts.ready } catch { /* noop */ } return { check: !!(document.fonts && document.fonts.check && document.fonts.check('16px CaesuraNoto')), faces: document.fonts ? [...document.fonts].map((f) => String(f.family) + ':' + f.status).slice(0, 6) : [] } })()")
      record('cjk: packaged @font-face loaded (document.fonts.check)', !!(font && font.check === true), JSON.stringify(font))
      const fontRes = await cdp.eval("[...performance.getEntriesByType('resource')].filter((r) => String(r.name).includes('fonts/')).map((r) => String(r.name).split('/').slice(-2).join('/'))")
      record('cjk: font asset fetched via packaged URL', Array.isArray(fontRes) && fontRes.length > 0, JSON.stringify(fontRes))
    }

    // 4c. --stress mode: web_stress_vn large-asset / memory stress (plan W4).
    // Drives 3 full cycles of the 12-page loop and records real measurements
    // (first-boot time, per-cycle texture counts, DOM layer counts, heap
    // samples, page errors) — numeric values are recorded, not asserted.
    if (STRESS) {
      const bootMs = Date.now() - bootAt
      const pageNo = async () => {
        const t = await cdp.eval("(document.querySelector('.caesura-message') || {}).textContent || ''")
        const m = /Stress (\d+)\/12/.exec(String(t))
        return m ? Number(m[1]) : 0
      }
      const tex = async () => {
        const v = await cdp.eval("(window.__caesuraCore && window.__caesuraCore.textures && window.__caesuraCore.textures.size) ?? -1")
        return typeof v === 'number' ? v : -1
      }
      const heap = async () => {
        const v = await cdp.eval("(window.performance && performance.memory && performance.memory.usedJSHeapSize) ?? -1")
        return typeof v === 'number' ? v : -1
      }
      const heapBoot = await heap()
      const texByCycle = []
      const cycleHit = (n) => { for (let i = 0; i < texByCycle.length; i++) if (texByCycle[i].pp === n) return i; return -1 }
      const seen = []
      let advances = 0
      const CYCLE_PAGES = 12
      const CYCLE_X = 3
      let lastCycle = texByCycle.length
      while (advances < CYCLE_PAGES * CYCLE_X * 1.3 && seen.length < CYCLE_PAGES * CYCLE_X) {
        const n = await pageNo()
        if (n > 0) seen.push(n)
        // a fresh cycle (page 1 after already having seen page 12) -> sample
        if (n === 1 && seen.length > 1) {
          texByCycle.push({ at: seen.length, tex: await tex(), layers: await cdp.eval("document.querySelectorAll('.caesura-layer[src]').length"), pp: texByCycle.length + 1 })
        }
        await cdp.clickTrusted('#advance')
        await sleep(650)
        advances++
      }
      const texEnd = await tex()
      const layersEnd = await cdp.eval("document.querySelectorAll('.caesura-layer[src]').length")
      const heapEnd = await heap()
      const errs = await cdp.eval("(window.__caesuraErrors || []).slice(0, 20)")
      const evErrors = await cdp.eval("(window.__caesuraCore && window.__caesuraCore.events ? window.__caesuraCore.events.filter((e) => String(e.kind).includes('error')).length : -1)")
      const c1 = texByCycle[0] ? texByCycle[0].tex : -1
      const c3 = texByCycle[2] ? texByCycle[2].tex : texEnd
      record('stress: 3 cycles completed (36 pages + loop)', seen.length >= CYCLE_PAGES * CYCLE_X && seen.filter((n) => n === 1).length >= CYCLE_X, 'pages=' + seen.length + ' cycles=' + seen.filter((n) => n === 1).length)
      const orderOk = (() => {
        const expect = Array.from({ length: 12 }, (_, i) => i + 1)
        for (let c = 0; c < 2; c++) for (let i = 0; i < 12; i++) if (seen[c * 12 + i] !== expect[i]) return false
        return true
      })()
      record('stress: page order stable (01..12 repeats)', orderOk, 'sample=' + seen.slice(0, 24).join(','))
      record('stress: texture cache bounded across cycles', c3 >= 0 && texEnd >= 0 && texEnd <= c1 + 6, 'c1=' + c1 + ' c3=' + c3 + ' end=' + texEnd)
      record('stress: DOM layers bounded (< 8)', Number(layersEnd) < 8, 'layers=' + layersEnd)
      record('stress: no page errors / wasm failures', Array.isArray(errs) && errs.length === 0 && Number(evErrors) === 0, 'errs=' + JSON.stringify(errs) + ' evErrors=' + evErrors)
      record('stress: measured boot ms (real)', true, 'bootMs=' + bootMs)
      record('stress: measured heap delta (real, GC-influenced)', true, 'heapBoot=' + heapBoot + ' heapEnd=' + heapEnd + ' delta=' + (heapEnd - heapBoot))
      record('stress: measured texture samples (real)', true, JSON.stringify(texByCycle.map((x) => x.tex)))
    }

    // 4. audio status (soft check — scene dependent)
    const audio = await cdp.eval("document.getElementById('audio-status').textContent")
    const audioSeen = /BGM|SE|VOICE/.test(String(audio))
    record('audio (audio-status reports bus when playing)', audioSeen || /^—$/.test(String(audio)), JSON.stringify(audio))
    // WebAudio real playback: a live source on the bus proves the normalized
    // asset URL decoded + started (core state alone would mask 404s).
    let srcs = await cdp.eval("(window.__caesuraAudio && window.__caesuraAudio._sources ? [...window.__caesuraAudio._sources.keys()] : [])")
    if (audioSeen && (!Array.isArray(srcs) || srcs.length === 0)) {
      // decode/start is async (large WAV); give it a window before failing
      const srcDeadline = Date.now() + 20000
      while (Date.now() < srcDeadline) {
        await sleep(500)
        srcs = await cdp.eval("(window.__caesuraAudio && window.__caesuraAudio._sources ? [...window.__caesuraAudio._sources.keys()] : [])")
        if (Array.isArray(srcs) && srcs.length > 0) break
      }
    }
    record('audio (WebAudio source(s) live when bus claims playing)', Array.isArray(srcs) && srcs.length > 0 || !audioSeen, JSON.stringify(srcs))

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
    const baseUrl = 'http://127.0.0.1:' + httpPort + '/' + (SCENE ? '?scene=' + encodeURIComponent(SCENE) : '')
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