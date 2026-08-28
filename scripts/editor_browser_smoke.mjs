#!/usr/bin/env node
// -----------------------------------------------------------------------------
// Caesura (AmeKAG) — editor_browser_smoke.mjs
//
// Real-browser end-to-end smoke for the HTTP editor's TOKEN UX (Sprint 4 t5).
// curl can prove the HTTP layer (401 on /api/*, 200 on the static shell); it
// cannot prove that a stranger who clicks the URL the engine prints actually
// gets a working editor. This harness does, by driving a real Chromium-family
// browser over CDP against a real engine process:
//
//   * engine:    launch CaesuraAmeKAG.exe --editor, parse the printed
//                "Open the editor: http://127.0.0.1:<port>/?token=<hex>" line
//   * shell:     navigating WITHOUT a token still serves the editor shell
//                (static shell is unauthenticated) and the UI says a token is
//                needed instead of looking silently broken
//   * token URL: navigating the printed URL persists the token in localStorage
//                and at least one /api/* call succeeds IN THE BROWSER
//   * reload:    a bare reload (no ?token= in the URL) still works — the whole
//                point of the localStorage code, invisible to curl
//   * revoke:    clearing localStorage + reload degrades to "needs token"
//   * errors:    any page JS error / console error is a finding
//
// Usage (from repo root, git bash):
//   node scripts/editor_browser_smoke.mjs --engine build/Debug
//   node scripts/editor_browser_smoke.mjs --engine /tmp/rel/CaesuraAmeKAG-1.0.1-Windows-AMD64 --browser edge
//
// Output: [PASS]/[FAIL] lines + summary; screenshots under build/editor-smoke/.
// Exit code: 0 = every asserted check passed, 1 = any failed.
// -----------------------------------------------------------------------------

import net from 'node:net'
import { existsSync, mkdirSync, writeFileSync, readFileSync } from 'node:fs'
import { join, resolve } from 'node:path'
import { spawn } from 'node:child_process'

const ARGV = process.argv.slice(2)
const arg = (name, dflt) => {
  const i = ARGV.indexOf(name)
  return i >= 0 && ARGV[i + 1] !== undefined ? ARGV[i + 1] : dflt
}
const has = (name) => ARGV.includes(name)

const ENGINE_DIR = resolve(arg('--engine', join(process.cwd(), 'build', 'Debug')))
const EXE = arg('--exe', 'CaesuraAmeKAG.exe')
const BROWSER = arg('--browser', 'chrome')
const CDP_PORT_ARG = Number(arg('--cdp-port', 0))
const BOOT_TIMEOUT_MS = Number(arg('--boot-timeout', 120000))
const OUT_DIR = join(process.cwd(), 'build', 'editor-smoke')

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
  srv.listen(0, '127.0.0.1', () => { const p = srv.address().port; srv.close(() => res(p)) })
})
const pass = new Set()
const fail = new Set()
const record = (name, ok, detail = '') => {
  ;(ok ? pass : fail).add(name)
  console.log((ok ? '[PASS] ' : '[FAIL] ') + name + (detail ? '  — ' + detail : ''))
}

function findBrowser() {
  for (const p of CHROME_PATHS[BROWSER] ?? CHROME_PATHS.chrome) if (existsSync(p)) return p
  return null
}

async function waitForCdp(port, timeout = 25000) {
  const deadline = Date.now() + timeout
  while (Date.now() < deadline) {
    try {
      const r = await fetch('http://127.0.0.1:' + port + '/json/list')
      if (r.ok) return await r.json()
    } catch { /* not up yet */ }
    await sleep(250)
  }
  throw new Error('CDP endpoint did not come up on port ' + port)
}

class Cdp {
  constructor(ws) { this.ws = ws; this.id = 0; this.pending = new Map(); this.events = [] }
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
      } else if (msg.method) {
        cdp.events.push(msg)
      }
    }
    return cdp
  }
  send(method, params = {}) {
    const id = ++this.id
    return new Promise((res, rej) => {
      const timer = setTimeout(() => {
        this.pending.delete(id)
        rej(new Error('CDP send timed out: ' + method))
      }, 20000)
      this.pending.set(id, {
        res: (m) => { clearTimeout(timer); res(m) },
        rej: (e) => { clearTimeout(timer); rej(e) },
      })
      this.ws.send(JSON.stringify({ id, method, params }))
    })
  }
  async eval(expression) {
    const r = await this.send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true })
    if (r.exceptionDetails) {
      throw new Error('page eval failed: ' + String(r.exceptionDetails.exception?.description ?? JSON.stringify(r.exceptionDetails)).slice(0, 300))
    }
    return r.result?.value
  }
  async waitEval(expression, label, timeout = 30000) {
    const deadline = Date.now() + timeout
    let last = null
    while (Date.now() < deadline) {
      try { last = await this.eval(expression); if (last) return last } catch { /* transient */ }
      await sleep(250)
    }
    throw new Error('waitEval timed out: ' + label + ' (last=' + JSON.stringify(last) + ')')
  }
  async navigate(url) {
    await this.send('Page.navigate', { url })
    // the shell is a single inline-script file: DOM ready is enough
    await this.waitEval("document.readyState === 'complete' && !!document.getElementById('status')", 'shell DOM ready for ' + url, 30000)
  }
  // console errors + page exceptions collected since a marker index
  drainErrors(fromIndex) {
    const out = []
    for (let i = fromIndex; i < this.events.length; i++) {
      const e = this.events[i]
      if (e.method === 'Runtime.exceptionThrown') {
        out.push({ kind: 'exception', text: String(e.params?.exceptionDetails?.exception?.description ?? e.params?.exceptionDetails?.text ?? '').slice(0, 300) })
      } else if (e.method === 'Runtime.consoleAPICalled' && (e.params?.type === 'error' || e.params?.type === 'assert')) {
        out.push({ kind: 'console.' + e.params.type, text: (e.params.args ?? []).map((a) => String(a.value ?? a.description ?? a.type)).join(' ').slice(0, 300) })
      }
    }
    return out
  }
  async screenshot(file) {
    const r = await this.send('Page.captureScreenshot', { format: 'png' })
    mkdirSync(OUT_DIR, { recursive: true })
    writeFileSync(file, Buffer.from(r.data, 'base64'))
    return file
  }
}

// --------------------------------------------------------------- engine boot
function startEngine() {
  const exePath = join(ENGINE_DIR, EXE)
  if (!existsSync(exePath)) throw new Error('engine binary not found: ' + exePath)
  const proc = spawn(exePath, ['--editor'], { cwd: ENGINE_DIR, stdio: ['ignore', 'pipe', 'pipe'] })
  const log = { out: '', err: '' }
  let exitInfo = null
  proc.stdout.on('data', (d) => { log.out += String(d) })
  proc.stderr.on('data', (d) => { log.err += String(d) })
  proc.on('exit', (code, signal) => { exitInfo = { code, signal } })
  return { proc, log, exePath, exitInfo: () => exitInfo }
}

// An engine that dies mid-run must fail the run with its stderr tail instead
// of silently turning every later assertion into a mystery.
function assertEngineAlive(engine, phase) {
  const info = engine.exitInfo()
  if (info) {
    throw new Error('engine EXITED during ' + phase + ' (code=' + info.code + ' signal=' + info.signal + ')\n' +
      '--- stdout tail ---\n' + engine.log.out.slice(-1200) + '\n--- stderr tail ---\n' + engine.log.err.slice(-1200))
  }
}

// Verify the OWN engine accepts the printed token over a family-exact socket
// BEFORE the browser is involved. Anything else is an instant, informative
// abort (stale twin on ::1, wrong binary, port hijack).
async function verifyOwnEngine(port, token) {
  const api = 'http://127.0.0.1:' + port + '/api/status'
  for (let i = 0; i < 20; i++) {
    try {
      const r = await fetch(api, { headers: { Authorization: 'Bearer ' + token } })
      if (r.status === 200) {
        const j = await r.json().catch(() => ({}))
        record('own-engine: 127.0.0.1:' + port + ' accepts the printed token', j.status === 'ok', JSON.stringify(j).slice(0, 120))
        return true
      }
      record('own-engine: 127.0.0.1:' + port + ' accepts the printed token', false, 'status=' + r.status)
      return false
    } catch { await sleep(500) }
  }
  record('own-engine: 127.0.0.1:' + port + ' accepts the printed token', false, 'no response')
  return false
}

// ---------------------------------------------------------------------------
// 9876 port hygiene: list listening owners on BOTH address families (the
// engine binds 127.0.0.1:9876; a leftover editor can still hold [::1]:9876,
// which "localhost" would happily reach and answer with a DIFFERENT token).
// Reap exactly stale CaesuraAmeKAG holders; refuse anything else.
// ---------------------------------------------------------------------------
function listenersOn9876() {
  try {
    const out = execFileSync('powershell.exe',
      ['-NoProfile', '-NonInteractive', '-Command',
        "Get-NetTCPConnection -LocalPort 9876 -State Listen -ErrorAction SilentlyContinue | ForEach-Object { $p = Get-Process -Id $_.OwningProcess -ErrorAction SilentlyContinue; if ($p) { Write-Output ($_.OwningProcess.ToString() + '|' + $_.LocalAddress + '|' + $p.ProcessName) } }"],
      { encoding: 'utf8', timeout: 15000 })
    return out.trim().split(/\r?\n/).filter(Boolean).map((l) => {
      const [pid, addr, name] = l.split('|')
      return { pid: Number(pid), addr: String(addr).trim(), name: String(name).trim() }
    })
  } catch { return [] }
}

async function ensureEditorPortFree() {
  const owners = listenersOn9876()
  if (owners.length === 0) return true
  console.warn('[editor-smoke] 127.0.0.1/::1:9876 occupied by: ' + owners.map((o) => o.name + ':' + o.pid + '@' + o.addr).join(', '))
  const stale = owners.filter((o) => /^CaesuraAmeKAG/i.test(o.name))
  const foreign = owners.filter((o) => !/^CaesuraAmeKAG/i.test(o.name))
  if (foreign.length) {
    console.error('[editor-smoke] FATAL: port 9876 is held by a NON-engine process (' +
      foreign.map((o) => o.name + ':' + o.pid).join(', ') + '); free it and rerun')
    return false
  }
  for (const o of stale) {
    console.warn('[editor-smoke] reaping stale editor PID ' + o.pid)
    try { execFileSync('taskkill', ['/F', '/T', '/PID', String(o.pid)], { stdio: 'ignore', timeout: 15000 }) } catch { /* noop */ }
  }
  await sleep(1500)
  const still = listenersOn9876()
  if (still.length) {
    console.error('[editor-smoke] FATAL: 9876 still occupied after reaping: ' + still.map((o) => o.name + ':' + o.pid).join(', '))
    return false
  }
  return true
}

// Chrome/Edge children often outlive their launcher; kill exactly the
// processes tied to THIS run's unique --user-data-dir (cmdline match on
// chrome/msedge only — never a blanket node.exe/taskkill sweep; DSH host is
// node.exe and must not be touched).
function cleanupBrowsersByProfile(profile) {
  try {
    if (!profile) return
    const esc = profile.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
    const q = "Get-CimInstance Win32_Process -Filter \"Name='chrome.exe' or Name='msedge.exe'\" | " +
      "Where-Object { $_.CommandLine -match '" + esc + "' } | " +
      "ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }"
    spawn('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command', q], { stdio: 'ignore' })
  } catch { /* best effort */ }
}

async function waitForEditorUrl(engine, timeout) {
  const { log, proc } = engine
  const deadline = Date.now() + timeout
  const re = /Open the editor:\s*(http:\/\/\S+)/
  while (Date.now() < deadline) {
    if (proc.exitCode !== null) throw new Error('engine exited early (code ' + proc.exitCode + ')\n' + log.err.slice(-1500))
    const m = re.exec(log.out) ?? re.exec(log.err)
    if (m) return m[1].trim()
    await sleep(250)
  }
  throw new Error('engine never printed the editor URL within ' + timeout + 'ms\nstdout tail:\n' + log.out.slice(-800) + '\nstderr tail:\n' + log.err.slice(-800))
}

// ------------------------------------------------------------------- main
async function main() {
  const browserPath = findBrowser()
  if (!browserPath) { console.error('[editor-smoke] FATAL: browser not found: ' + BROWSER); return 1 }

  if (!(await ensureEditorPortFree())) return 1

  const engine = startEngine()
  console.log('[editor-smoke] engine:', engine.exePath, '(cwd ' + ENGINE_DIR + ')')
  let browserProc = null
  let cdp = null
  let profile = null
  try {
    const printedUrl = await waitForEditorUrl(engine, BOOT_TIMEOUT_MS)
    console.log('[editor-smoke] printed URL:', printedUrl)
    const u = new URL(printedUrl)
    const token = u.searchParams.get('token') ?? ''
    const port = Number(u.port)
    record('engine prints a clickable editor URL with ?token=', /^[0-9a-f]{32,}$/.test(token), 'origin=' + u.origin + ' tokenLen=' + token.length)

    // cross-check against the token file the engine writes next to the launch dir
    const tokenFile = join(ENGINE_DIR, '.caesura-editor-token')
    const fileToken = existsSync(tokenFile) ? readFileSync(tokenFile, 'utf8').trim() : ''
    record('printed token matches .caesura-editor-token', !!token && fileToken === token, 'file=' + (fileToken ? fileToken.slice(0, 8) + '…' : 'missing'))

    // Family-exact origin for ALL browser work: the engine binds 127.0.0.1;
    // resolving "localhost" could land on a ::1 twin with a different token.
    const testUrl = 'http://127.0.0.1:' + port
    if (!(await verifyOwnEngine(port, token))) {
      throw new Error('own engine failed token verification — stale twin on ::1 or a hijacked port?')
    }
    assertEngineAlive(engine, 'own-engine verification')

    const cdpPort = CDP_PORT_ARG || await freePort()
    profile = join(OUT_DIR, 'profile-' + BROWSER + '-' + Date.now())
    mkdirSync(OUT_DIR, { recursive: true })
    browserProc = spawn(browserPath, [
      '--headless=new', '--no-first-run', '--no-default-browser-check',
      '--disable-features=Translate', '--disable-extensions',
      '--remote-debugging-port=' + cdpPort,
      '--user-data-dir=' + profile,
      '--window-size=1280,900',
      'about:blank',
    ], { stdio: 'ignore' })
    browserProc.on('error', (e) => console.error('[editor-smoke] browser spawn error', e))

    let targets = await waitForCdp(cdpPort)
    let page = targets.find((t) => t.type === 'page')
    const pageDeadline = Date.now() + 30000
    while (!page && Date.now() < pageDeadline) {
      await sleep(250)
      try { targets = await (await fetch('http://127.0.0.1:' + cdpPort + '/json/list')).json() } catch { continue }
      page = targets.find((t) => t.type === 'page')
    }
    if (!page) throw new Error('no page target; targets=' + JSON.stringify(targets.map((t) => t.type + ':' + t.url)))
    cdp = await Cdp.connect(page.webSocketDebuggerUrl)
    await cdp.send('Runtime.enable')
    await cdp.send('Page.enable')

    // in-page fetch helper: returns the REAL status the browser sees
    const fetchStatus = async (path, withToken) => cdp.eval(
      "(async () => { try {" +
      "  const h = {};" +
      (withToken ? "  h['Authorization'] = 'Bearer ' + (localStorage.getItem('caesura.editorToken') || '');" : '') +
      "  const r = await fetch(" + JSON.stringify(path) + ", { headers: h });" +
      "  return r.status;" +
      "} catch (e) { return 'throw:' + String(e && e.message); } })()")

    assertEngineAlive(engine, 'phase 1 start')
// -------------------------------------------------- 1. bare shell, no token
    let mark = cdp.events.length
    await cdp.navigate(testUrl + '/')
    const shellDom = await cdp.eval("(() => ({" +
      " sidebar: !!document.getElementById('sidebar')," +
      " tabs: document.querySelectorAll('#tabs button').length," +
      " tokenInput: !!document.getElementById('tokenInput')," +
      " title: document.title," +
      " bodyLen: document.body.innerHTML.length }))()")
    record('shell: unauthenticated GET / renders the editor (not a 401 page)',
      !!shellDom && shellDom.sidebar === true && Number(shellDom.tabs) >= 4 && Number(shellDom.bodyLen) > 500,
      JSON.stringify(shellDom))
    const lsEmpty = await cdp.eval("String(localStorage.getItem('caesura.editorToken') || '')")
    record('shell: fresh profile has no stored token', String(lsEmpty) === '', JSON.stringify(lsEmpty))
    const apiNoToken = await fetchStatus('/api/status', false)
    record('shell: /api/status without token is 401 IN THE BROWSER', Number(apiNoToken) === 401, 'status=' + apiNoToken)
    // the UI must SAY it needs a token rather than look silently broken
    const badgeNoToken = await cdp.waitEval("(() => { const t = ((document.getElementById('status')||{}).textContent || '').trim(); return (t && t !== '?') ? t : null })()", 'status badge text (no token)', 15000).catch(() => '')
    record('shell: UI states a token is needed (badge shows 需要令牌)', String(badgeNoToken).includes('需要令牌'), JSON.stringify(String(badgeNoToken)))
    const shot1 = await cdp.screenshot(join(OUT_DIR, 'editor-no-token.png'))
    const errs1 = cdp.drainErrors(mark)
    record('shell: no page JS errors without a token', errs1.length === 0, JSON.stringify(errs1).slice(0, 400))

    // 1b. badge stability: the no-token badge must STAY 需要令牌, not flicker into ERR
    {
      const samples = []
      for (let i = 0; i < 60; i++) {
        const t = await cdp.eval("((document.getElementById('status')||{}).textContent || '').trim()").catch(() => '')
        samples.push(String(t))
        await sleep(120)
      }
      const errCount = samples.filter((s) => s === 'ERR').length
      record('shell: 需要令牌 badge is STABLE (no ERR flicker over ~7s)',
        errCount === 0 && samples.every((s) => s.includes('需要令牌') || s === '?'),
        'ERR x' + errCount + ' of ' + samples.length + '; first 20: ' + JSON.stringify(samples.slice(0, 20)))
    }

    assertEngineAlive(engine, 'phase 2 start')
// -------------------------------------------------- 2. printed URL w/ token
    mark = cdp.events.length
    await cdp.navigate(testUrl + '?token=' + token)
    const stored = await cdp.eval("String(localStorage.getItem('caesura.editorToken') || '')")
    record('token URL: ?token= persisted into localStorage', stored === token, 'stored=' + String(stored).slice(0, 8) + '… len=' + String(stored).length)
    const badgeOk = await cdp.waitEval("(() => { const t = ((document.getElementById('status')||{}).textContent || '').trim(); return t === 'ok' ? t : null })()", 'status badge ok', 20000).catch(() => '')
    record('token URL: /api/status succeeds in the browser (badge ok)', String(badgeOk) === 'ok', JSON.stringify(String(badgeOk)))
    const portText = await cdp.eval("((document.getElementById('port')||{}).textContent || '').trim()")
    record('token URL: UI shows engine port from the API payload', String(portText) === String(u.port), 'port=' + portText)
    const luaText = await cdp.eval("((document.getElementById('lua')||{}).textContent || '').trim()")
    // invariant: the UI text must MATCH the /api/status payload ('lua' field), not just be non-empty
    const apiLua = await cdp.eval("(async () => { try { const r = await fetch('/api/status', { headers: { Authorization: 'Bearer ' + (localStorage.getItem('caesura.editorToken') || '') } }); return (await r.json()).lua === true; } catch (e) { return String(e && e.message) } })()")
    const expectedLua = apiLua === true ? '可用' : '不可用'
    record('token URL: UI shows Lua availability matching /api/status payload', String(luaText) === String(expectedLua), 'ui=' + luaText + ' api.lua=' + JSON.stringify(apiLua) + ' expected=' + expectedLua)
    const apiWithToken = await fetchStatus('/api/status', true)
    record('token URL: /api/status with stored bearer is 200', Number(apiWithToken) === 200, 'status=' + apiWithToken)
    const listCounts = await cdp.eval("(async () => { await new Promise(r => setTimeout(r, 1200)); return { assets: document.querySelectorAll('#assets li').length, models: document.querySelectorAll('#models li').length, logs: document.querySelectorAll('#log div').length } })()")
    const assetStatus = await fetchStatus('/api/assets?type=script', true)
    record('token URL: /api/assets answers 200 in the browser', Number(assetStatus) === 200, 'status=' + assetStatus + ' dom=' + JSON.stringify(listCounts))
    const shot2 = await cdp.screenshot(join(OUT_DIR, 'editor-with-token.png'))
    const errs2 = cdp.drainErrors(mark)
    record('token URL: no page JS errors', errs2.length === 0, JSON.stringify(errs2).slice(0, 400))

    assertEngineAlive(engine, 'phase 3 start')
// -------------------------------------------------- 3. bare reload, no query
    mark = cdp.events.length
    await cdp.navigate(testUrl + '/')
    const searchAfter = await cdp.eval('String(location.search)')
    const badgeReload = await cdp.waitEval("(() => { const t = ((document.getElementById('status')||{}).textContent || '').trim(); return t === 'ok' ? t : null })()", 'status badge ok after bare reload', 20000).catch(() => '')
    record('reload: bare URL (no ?token=) still authorized via localStorage',
      String(badgeReload) === 'ok' && String(searchAfter) === '', 'search=' + JSON.stringify(searchAfter) + ' badge=' + JSON.stringify(String(badgeReload)))
    const reloadApi = await fetchStatus('/api/status', true)
    record('reload: /api/status still 200 after bare reload', Number(reloadApi) === 200, 'status=' + reloadApi)
    const errs3 = cdp.drainErrors(mark)
    record('reload: no page JS errors', errs3.length === 0, JSON.stringify(errs3).slice(0, 400))

    assertEngineAlive(engine, 'phase 4 start')
// -------------------------------------------------- 4. revoke -> needs token
    mark = cdp.events.length
    await cdp.eval("(() => { localStorage.removeItem('caesura.editorToken'); return true })()")
    await cdp.navigate(testUrl + '/')
    const clearedLs = await cdp.eval("String(localStorage.getItem('caesura.editorToken') || '')")
    const badgeCleared = await cdp.waitEval("(() => { const t = ((document.getElementById('status')||{}).textContent || '').trim(); return (t && t !== '?') ? t : null })()", 'status badge text after clear', 15000).catch(() => '')
    record('revoke: cleared localStorage really is empty', String(clearedLs) === '', JSON.stringify(clearedLs))
    record('revoke: UI degrades to 需要令牌 (not a silent blank)', String(badgeCleared).includes('需要令牌'), JSON.stringify(String(badgeCleared)))
    const shot3 = await cdp.screenshot(join(OUT_DIR, 'editor-token-cleared.png'))
    const errs4 = cdp.drainErrors(mark)
    record('revoke: no page JS errors', errs4.length === 0, JSON.stringify(errs4).slice(0, 400))

    assertEngineAlive(engine, 'phase 5 start')
// -------------------------------------------------- 5. paste-token path
    mark = cdp.events.length
    await cdp.eval("(() => { const i = document.getElementById('tokenInput'); i.value = " + JSON.stringify(token) + "; return true })()")
    await cdp.eval("(() => { saveToken(); return true })()")
    const badgePasted = await cdp.waitEval("(() => { const t = ((document.getElementById('status')||{}).textContent || '').trim(); return t === 'ok' ? t : null })()", 'status badge ok after paste', 20000).catch(() => '')
    record('paste: sidebar token paste + 保存 recovers the session', String(badgePasted) === 'ok', JSON.stringify(String(badgePasted)))
    const errs5 = cdp.drainErrors(mark)
    record('paste: no page JS errors', errs5.length === 0, JSON.stringify(errs5).slice(0, 400))

    console.log('[editor-smoke] screenshots: ' + [shot1, shot2, shot3].join(', '))
  } finally {
    try { if (cdp) await cdp.send('Page.close') } catch { /* page already gone */ }
    if (browserProc) { try { browserProc.kill() } catch { /* noop */ } }
    // only the engine process THIS harness spawned is killed (never a blanket kill)
    try { engine.proc.kill() } catch { /* noop */ }
    await sleep(500)
    if (engine.proc.exitCode === null) { try { spawn('taskkill', ['/F', '/T', '/PID', String(engine.proc.pid)], { stdio: 'ignore' }) } catch { /* noop */ } }
    cleanupBrowsersByProfile(profile)
  }

  console.log('\n[editor-smoke] summary: ' + pass.size + ' passed, ' + fail.size + ' failed')
  for (const f of fail) console.log('  FAILED: ' + f)
  return fail.size === 0 ? 0 : 1
}

main().then((code) => process.exit(code)).catch((e) => {
  console.error('[editor-smoke] FATAL:', e.message)
  process.exit(1)
})
