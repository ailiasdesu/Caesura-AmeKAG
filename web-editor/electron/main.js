const { app, BrowserWindow, dialog } = require('electron')
const { spawn } = require('child_process')
const path = require('path')

let engineProcess = null
let mainWindow = null

function startEngine() {
  const enginePath = path.join(__dirname, '..', '..', 'build', 'Debug', 'CaesuraAmeKAG.exe')
  
  try {
    engineProcess = spawn(enginePath, ['--headless'], {
      cwd: path.join(__dirname, '..', '..', 'build', 'Debug'),
      stdio: ['ignore', 'pipe', 'pipe']
    })

    engineProcess.stdout.on('data', (data) => {
      console.log(`[Engine] ${data}`)
    })

    engineProcess.stderr.on('data', (data) => {
      console.error(`[Engine Error] ${data}`)
    })

    engineProcess.on('close', (code) => {
      console.log(`Engine exited with code ${code}`)
      engineProcess = null
    })

    console.log('Engine started:', enginePath)
  } catch (err) {
    console.error('Failed to start engine:', err.message)
    dialog.showErrorBox('Engine Error', `Cannot start Caesura engine:\n${err.message}\n\nPath: ${enginePath}`)
  }
}

function stopEngine() {
  if (engineProcess) {
    engineProcess.kill()
    engineProcess = null
  }
}

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

  // Dev mode: load from Vite dev server
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
  // Wait a moment for engine HTTP server to start
  // Wait for engine HTTP server to start (headless mode serves on :9876)
  setTimeout(createWindow, 2000)
})

app.on('window-all-closed', () => {
  stopEngine()
  app.quit()
})

app.on('before-quit', () => {
  stopEngine()
})