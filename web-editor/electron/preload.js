const { contextBridge } = require('electron')

contextBridge.exposeInMainWorld('caesura', {
  platform: process.platform,
  isElectron: true
})