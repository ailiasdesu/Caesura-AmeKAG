// Caesura IDE — Electron main process.
// Launches the engine in editor mode (hidden GPU window + HTTP RPC on
// :9876), opens the IDE window, proxies /api to the engine and recycles
// the engine process on quit. VS Code-style desktop IDE (Electron is the
// same shell VS Code uses).
// CommonJS (.cjs): the Electron main process must be CJS regardless of
// the renderer's "type": "module" (Vite requirement).

const { app, BrowserWindow, shell, ipcMain } = require('electron')
const { spawn } = require('node:child_process')
const path = require('node:path')
const fs = require('node:fs')
const { createServer, request } = require('node:http')

const ENGINE_PORT = 9876
const RENDERER_PORT = 5920 // dev-only static server port

let engineProc = null
let rendererServer = null
let mainWindow = null

// ---------------------------------------------------------------------------
// Engine lifecycle: spawn the game engine in editor mode and keep it alive
// for the whole IDE session; kill it on quit.
// ---------------------------------------------------------------------------

function resolveEngineExe() {
  // 1) env override (packaged app: set CAESURA_ENGINE_EXE to the game exe)
  if (process.env.CAESURA_ENGINE_EXE) return process.env.CAESURA_ENGINE_EXE
  // 2) dev: sibling build/Debug/CaesuraAmeKAG.exe
  const candidates = [
    path.join(__dirname, '..', '..', 'build', 'Debug', 'CaesuraAmeKAG.exe'),
    path.join(app.getAppPath(), '..', 'CaesuraAmeKAG.exe'),
    path.join(process.resourcesPath || '', 'CaesuraAmeKAG.exe'),
  ]
  for (const c of candidates) {
    try {
      if (fs.existsSync(c)) return c
    } catch {
      /* keep looking */
    }
  }
  return null
}

function startEngine() {
  if (engineProc) return
  const exe = resolveEngineExe()
  if (!exe) {
    console.error('[ide] engine executable not found — run the game binary first')
    return
  }
  console.log('[ide] spawning engine:', exe)
  engineProc = spawn(exe, ['--editor', '--backend', 'dx11'], {
    cwd: path.dirname(exe),
    windowsHide: true,
    stdio: ['ignore', 'pipe', 'pipe'],
  })
  engineProc.stdout && engineProc.stdout.on('data', (d) => process.stdout.write(`[engine] ${d}`))
  engineProc.stderr && engineProc.stderr.on('data', (d) => process.stderr.write(`[engine] ${d}`))
  engineProc.on('exit', (code) => {
    console.log('[ide] engine exited with code', code)
    engineProc = null
  })
}

function stopEngine() {
  if (engineProc) {
    engineProc.kill()
    engineProc = null
  }
}

// ---------------------------------------------------------------------------
// Dev renderer server: serve editor/dist when not running under vite dev.
// ---------------------------------------------------------------------------

const MIME = {
  '.html': 'text/html',
  '.js': 'text/javascript',
  '.css': 'text/css',
  '.json': 'application/json',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.map': 'application/json',
}

function serveStatic(port) {
  const root = path.join(__dirname, '..', 'dist')
  rendererServer = createServer((req, res) => {
    const url = (req.url || '/').split('?')[0]
    const rel = url === '/' ? 'index.html' : url.replace(/^\/+/, '')
    const file = path.join(root, rel)
    try {
      const data = fs.readFileSync(file)
      const ext = path.extname(file).toLowerCase()
      res.writeHead(200, { 'Content-Type': MIME[ext] || 'application/octet-stream' })
      res.end(data)
    } catch {
      // SPA fallback
      try {
        const idx = fs.readFileSync(path.join(root, 'index.html'))
        res.writeHead(200, { 'Content-Type': 'text/html' })
        res.end(idx)
      } catch {
        res.writeHead(404)
        res.end('not found')
      }
    }
  })
  rendererServer.listen(port, '127.0.0.1', () => {
    console.log(`[ide] renderer server on http://127.0.0.1:${port}`)
  })
}

// ---------------------------------------------------------------------------
// /api proxy to the engine's HTTP RPC server (keeps the renderer
// same-origin; no CORS, no token exposure in the page).
// ---------------------------------------------------------------------------

function proxyApi(req, res) {
  const upstream = request(
    {
      host: '127.0.0.1',
      port: ENGINE_PORT,
      path: req.url,
      method: req.method,
      headers: Object.assign({}, req.headers, { host: `127.0.0.1:${ENGINE_PORT}` }),
    },
    (upRes) => {
      res.writeHead(upRes.statusCode || 502, upRes.headers)
      upRes.pipe(res)
    },
  )
  upstream.on('error', () => {
    res.writeHead(502, { 'Content-Type': 'application/json' })
    res.end(JSON.stringify({ status: 'error', message: 'engine unreachable' }))
  })
  req.pipe(upstream)
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 960,
    minHeight: 600,
    title: 'Caesura IDE',
    backgroundColor: '#1e1e2e',
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  })

  // external links (if any) open in the system browser, never in-window
  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url)
    return { action: 'deny' }
  })

  // dev: vite dev server; prod: bundled dist via local static server
  const devUrl = process.env.VITE_DEV_SERVER_URL
  if (devUrl) {
    mainWindow.loadURL(devUrl)
  } else {
    serveStatic(RENDERER_PORT)
    mainWindow.loadURL(`http://127.0.0.1:${RENDERER_PORT}`)
  }
  mainWindow.on('closed', () => {
    mainWindow = null
  })
}

// ---------------------------------------------------------------------------
// IPC: the renderer can ask for engine control
// ---------------------------------------------------------------------------

ipcMain.handle('ide:engine-status', () => ({
  running: engineProc !== null,
  port: ENGINE_PORT,
}))

ipcMain.handle('ide:open-external', (_e, url) => {
  if (typeof url === 'string') shell.openExternal(url)
  return true
})

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

app.whenReady().then(() => {
  startEngine()
  createWindow()
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow()
  })
})

app.on('window-all-closed', () => {
  stopEngine()
  if (rendererServer) rendererServer.close()
  app.quit()
})

app.on('before-quit', () => {
  stopEngine()
})
