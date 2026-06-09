const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("caesura", {
  // Engine RPC
  ping: () => ipcRenderer.invoke("rpc-call", "ping"),
  run: (script) => ipcRenderer.invoke("rpc-call", "run", { script }),
  stop: () => ipcRenderer.invoke("rpc-call", "stop"),
  assets: (type) => ipcRenderer.invoke("rpc-call", "assets", { type: type || "" }),
  eval: (code) => ipcRenderer.invoke("rpc-call", "eval", { code }),
  getState: () => ipcRenderer.invoke("rpc-call", "getState", {}),

  // Log events
  onLog: (callback) => {
    ipcRenderer.on("engine-log", (event, msg) => callback(msg));
  },

  // Engine status
  onStatus: (callback) => {
    ipcRenderer.on("engine-status", (event, status) => callback(status));
  },

  // AI Chat (E6/E7)
  aiChat: ({ messages, provider, settings }) =>
    ipcRenderer.invoke("ai-chat", { messages, provider, settings }),

  codexChat: ({ messages, settings }) =>
    ipcRenderer.invoke("codex-chat", { messages, settings }),
});