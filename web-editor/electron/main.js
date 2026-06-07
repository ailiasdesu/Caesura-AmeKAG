const { app, BrowserWindow, dialog } = require('electron')
const { spawn } = require('child_process')
const path = require('path')

let engineProcess = null
let mainWindow = null
let rpcId = 0
let rpcPending = new Map()

function startEngine() {
  const enginePath = path.join(__dirname, '..', '..', 'build', 'Debug', 'CaesuraAmeKAG.exe')

  try {
    engineProcess = spawn(enginePath, ['--headless'], {
      cwd: path.join(__dirname, '..', '..', 'build', 'Debug'),
      stdio: ['pipe', 'pipe', 'pipe'] // stdin, stdout, stderr
    })

    // Collect stdout (JSON-RPC responses)
    let buffer = ''
    engineProcess.stdout.on('data', (data) => {
      buffer += data.toString()
      const lines = buffer.split('\n')
      buffer = lines.pop() // keep incomplete line

      for (const line of lines) {
        if (!line.trim()) continue
        try {
          const msg = JSON.parse(line)
          handleRpcResponse(msg)
        } catch (e) {
          console.log('[Engine stdout]', line)
        }
      }
    })

    engineProcess.stderr.on('data', (data) => {
      console.log('[Engine]', data.toString().trim())
    })

    engineProcess.on('close', (code) => {
      console.log('Engine exited with code', code)
      engineProcess = null
      if (mainWindow) {
        mainWindow.webContents.send('engine-status', { running: false, code })
      }
    })

    engineProcess.on('error', (err) => {
      console.error('Engine spawn error:', err.message)
      dialog.showErrorBox('Engine Error', 'Cannot start engine:\n' + err.message)
    })

    console.log('Engine started via stdin/stdout RPC:', enginePath)
  } catch (err) {
    console.error('Failed to start engine:', err.message)
    dialog.showErrorBox('Engine Error', 'Cannot start engine:\n' + err.message)
  }
}

function handleRpcResponse(msg) {
  if (msg.event === 'log') {
    if (mainWindow) {
      mainWindow.webContents.send('engine-log', msg)
    }
    return
  }

  if (msg.id !== undefined && rpcPending.has(msg.id)) {
    const { resolve } = rpcPending.get(msg.id)
    rpcPending.delete(msg.id)
    resolve(msg)
  }
}

function sendRpc(method, params = {}) {
  return new Promise((resolve, reject) => {
    if (!engineProcess || !engineProcess.stdin.writable) {
      reject(new Error('Engine not running'))
      return
    }

    const id = ++rpcId
    const request = JSON.stringify({ id, method, ...params }) + '\n'

    rpcPending.set(id, { resolve, reject })

    // Timeout after 30s
    const timeout = setTimeout(() => {
      rpcPending.delete(id)
      reject(new Error('RPC timeout'))
    }, 30000)

    const origResolve = resolve
    rpcPending.set(id, {
      resolve: (result) => {
        clearTimeout(timeout)
        origResolve(result)
      },
      reject
    })

    engineProcess.stdin.write(request)
  })
}

function stopEngine() {
  if (engineProcess) {
    try { engineProcess.stdin.end() } catch (e) {}
    engineProcess.kill()
    engineProcess = null
  }
}

// Expose RPC to renderer via IPC
const { ipcMain } = require('electron')

ipcMain.handle('rpc-call', async (event, method, params) => {
  try {
    return await sendRpc(method, params)
  } catch (e) {
    return { error: e.message }
  }
})

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    minWidth: 900,
    minHeight: 600,
    title: 'Caesura (AmeKAG)',
    backgroundColor: '#0f0f14',
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.js')
    }
  })

  const isDev = process.env.NODE_ENV !== 'production' || process.argv.includes('--dev')

  if (isDev) {
    mainWindow.loadURL('http://localhost:5173')
    mainWindow.webContents.openDevTools({ mode: 'detach' })
  } else {
    mainWindow.loadFile(path.join(__dirname, '..', 'dist', 'index.html'))
  }

  mainWindow.on('closed', () => {
    mainWindow = null
  })
}

app.whenReady().then(() => {
  startEngine()
  // Wait for engine RPC to be ready, then open window
  setTimeout(createWindow, 2000)
})

app.on('window-all-closed', () => {
  stopEngine()
  app.quit()
})

app.on('before-quit', () => {
  stopEngine()
})