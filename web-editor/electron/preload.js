const { contextBridge, ipcRenderer } = require('electron')

contextBridge.exposeInMainWorld('caesura', {
  // Call engine RPC
  call: (method, params = {}) => ipcRenderer.invoke('rpc-call', method, params),

  // Listen for engine events
  onLog: (callback) => {
    ipcRenderer.on('engine-log', (event, msg) => callback(msg))
  },
  onStatus: (callback) => {
    ipcRenderer.on('engine-status', (event, msg) => callback(msg))
  },

  // Convenience methods
  ping: () => ipcRenderer.invoke('rpc-call', 'ping'),
  run: (script) => ipcRenderer.invoke('rpc-call', 'run', { script }),
  eval: (code) => ipcRenderer.invoke('rpc-call', 'eval', { code }),
  stop: () => ipcRenderer.invoke('rpc-call', 'stop'),
  assets: (type = '') => ipcRenderer.invoke('rpc-call', 'assets', { type }),
  getState: () => ipcRenderer.invoke('rpc-call', 'getState')
})