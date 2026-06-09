const { app, BrowserWindow, dialog, ipcMain } = require("electron");
const { spawn } = require("child_process");
const path = require("path");

let engineProcess = null;
let mainWindow = null;
let rpcId = 0;
let rpcPending = new Map();

// =========================================================================
// Engine process management
// =========================================================================

function getEnginePath() {
  const isDev = !app.isPackaged;
  if (isDev) {
    return path.join(__dirname, "..", "..", "build_nol2d", "Debug", "CaesuraAmeKAG.exe");
  }
  // Packaged: engine is in extraResources
  return path.join(process.resourcesPath, "engine", "CaesuraAmeKAG.exe");
}

function getEngineCwd() {
  const isDev = !app.isPackaged;
  if (isDev) {
    // Include assets/, scripts/, demo/ folders accessible
    return path.join(__dirname, "..", "..");
  }
  return path.join(process.resourcesPath, "engine");
}

function startEngine() {
  const enginePath = getEnginePath();
  const cwd = getEngineCwd();

  try {
    engineProcess = spawn(enginePath, ["--editor"], {
      cwd,
      stdio: ["pipe", "pipe", "pipe"],
    });

    let buffer = "";
    engineProcess.stdout.on("data", (data) => {
      buffer += data.toString();
      const lines = buffer.split("\n");
      buffer = lines.pop();
      for (const line of lines) {
        if (!line.trim()) continue;
        try {
          const msg = JSON.parse(line);
          handleRpcResponse(msg);
        } catch (e) {
          console.log("[Engine stdout]", line);
        }
      }
    });

    engineProcess.stderr.on("data", (data) => {
      const text = data.toString().trim();
      if (text) console.log("[Engine]", text);
    });

    engineProcess.on("close", (code) => {
      console.log("Engine exited with code", code);
      engineProcess = null;
      if (mainWindow) {
        mainWindow.webContents.send("engine-status", { running: false, code });
      }
    });

    engineProcess.on("error", (err) => {
      console.error("Engine spawn error:", err.message);
      if (err.message.includes("ENOENT")) {
        dialog.showErrorBox("Engine Not Found", `Cannot find engine at:\n${enginePath}\n\nBuild the engine first: cmake --build build_nol2d --config Debug`);
      } else {
        dialog.showErrorBox("Engine Error", "Cannot start engine:\n" + err.message);
      }
    });

    console.log("Engine started:", enginePath);
  } catch (err) {
    console.error("Failed to start engine:", err.message);
  }
}

function stopEngine() {
  if (engineProcess) {
    try { engineProcess.stdin.end(); } catch {}
    engineProcess.kill();
    engineProcess = null;
  }
}

// =========================================================================
// JSON-RPC handlers
// =========================================================================

function handleRpcResponse(msg) {
  // Push events (logs)
  if (msg.event === "log") {
    if (mainWindow) mainWindow.webContents.send("engine-log", msg);
    return;
  }

  if (msg.id !== undefined && rpcPending.has(msg.id)) {
    const { resolve } = rpcPending.get(msg.id);
    rpcPending.delete(msg.id);
    resolve(msg);
  }
}

function sendRpc(method, params = {}) {
  return new Promise((resolve, reject) => {
    if (!engineProcess || !engineProcess.stdin.writable) {
      reject(new Error("Engine not running"));
      return;
    }
    const id = ++rpcId;
    const request = JSON.stringify({ id, method, ...params }) + "\n";
    rpcPending.set(id, { resolve, reject });
    const timeout = setTimeout(() => { rpcPending.delete(id); reject(new Error("RPC timeout")); }, 30000);
    const origResolve = resolve;
    rpcPending.set(id, { resolve: (result) => { clearTimeout(timeout); origResolve(result); }, reject });
    engineProcess.stdin.write(request);
  });
}

// =========================================================================
// AI Chat bridge (E6/E7)
// =========================================================================

async function handleAiChat(messages, provider, settings) {
  if (provider === "openai") {
    const key = settings.openaiKey || "";
    if (!key) throw new Error("OpenAI API key not configured.");
    const model = settings.openaiModel || "gpt-4o";
    const temperature = settings.temperature ?? 0.7;

    const resp = await fetch("https://api.openai.com/v1/chat/completions", {
      method: "POST",
      headers: { "Content-Type": "application/json", "Authorization": `Bearer ${key}` },
      body: JSON.stringify({ model, messages, temperature, max_tokens: 2048 }),
    });

    if (!resp.ok) {
      const err = await resp.json().catch(() => ({}));
      throw new Error(err.error?.message || `HTTP ${resp.status}`);
    }

    const data = await resp.json();
    return { content: data.choices?.[0]?.message?.content || "" };
  }

  if (provider === "codex") {
    // Codex: try to use local agent bridge
    try {
      const { execSync } = require("child_process");
      const prompt = messages.map((m) => `${m.role}: ${m.content}`).join("\n\n");
      // Use PowerShell to call Codex via CLI
      const result = execSync(
        `powershell -NoProfile -Command "& '${process.env.USERPROFILE}\\.codex\\venv\\Scripts\\python.exe' -c \\"import sys; print('Codex local provider: use AI panel in Codex IDE for direct assistance.')\\" "`,
        { encoding: "utf-8", timeout: 5000 }
      );
      return { content: result.trim() || "Local Codex bridge is available. Open this project in Codex IDE for AI-assisted KAG scripting." };
    } catch {
      return { content: "Codex local provider requires the project to be open in Codex IDE. Use OpenAI or Custom provider for standalone AI assistance." };
    }
  }

  if (provider === "custom") {
    const url = settings.customUrl || "";
    if (!url) throw new Error("Custom endpoint URL not configured.");
    const key = settings.customKey || "";
    const model = settings.customModel || "default";
    const temperature = settings.temperature ?? 0.7;
    const headers = { "Content-Type": "application/json" };
    if (key) headers["Authorization"] = `Bearer ${key}`;

    const resp = await fetch(url, {
      method: "POST",
      headers,
      body: JSON.stringify({ model, messages, temperature, max_tokens: 2048 }),
    });

    if (!resp.ok) {
      const err = await resp.json().catch(() => ({}));
      throw new Error(err.error?.message || `HTTP ${resp.status}`);
    }

    const data = await resp.json();
    return { content: data.choices?.[0]?.message?.content || "" };
  }

  throw new Error(`Unknown AI provider: ${provider}`);
}

// =========================================================================
// IPC handlers
// =========================================================================

ipcMain.handle("rpc-call", async (event, method, params) => {
  try { return await sendRpc(method, params); }
  catch (e) { return { error: e.message }; }
});

ipcMain.handle("ai-chat", async (event, { messages, provider, settings }) => {
  try {
    const result = await handleAiChat(messages, provider, settings);
    return result;
  } catch (e) {
    return { error: e.message };
  }
});

ipcMain.handle("codex-chat", async (event, { messages, settings }) => {
  return { content: "Codex bridge: use the Codex IDE for AI assistance with KAG scripting." };
});


// =========================================================================
// One-click package
// =========================================================================

ipcMain.handle("package-build", async (event, platform) => {
  const { execSync } = require("child_process");
  const builderPath = path.join(__dirname, "..");

  try {
    // Step 1: Build web app
    if (mainWindow) mainWindow.webContents.send("package-log", { level: "info", message: "Building web app..." });
    execSync("npx vite build", { cwd: builderPath, stdio: "pipe", encoding: "utf-8", timeout: 60000 });

    // Step 2: Package for platform
    if (mainWindow) mainWindow.webContents.send("package-log", { level: "info", message: `Packaging for ${platform}...` });

    const result = execSync(`npx electron-builder --${platform}`, {
      cwd: builderPath,
      stdio: "pipe",
      encoding: "utf-8",
      timeout: 300000,
      env: { ...process.env, CI: "true", USE_HARD_LINKS: "false" },
    });

    const outputDir = path.join(builderPath, "release");
    if (mainWindow) mainWindow.webContents.send("package-log", { level: "info", message: `Package complete: ${outputDir}` });
    return { success: true, path: outputDir, platform };
  } catch (e) {
    const errMsg = e.stderr || e.stdout || e.message;
    if (mainWindow) mainWindow.webContents.send("package-log", { level: "error", message: `Package failed: ${errMsg.slice(0, 200)}` });
    return { success: false, error: errMsg.slice(0, 500), platform };
  }
});

// =========================================================================
// Window management
// =========================================================================

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1600,
    height: 950,
    minWidth: 1100,
    minHeight: 650,
    title: "Caesura (AmeKAG) — Visual Novel Engine",
    backgroundColor: "#0f0f14",
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, "preload.js"),
    },
  });

  const isDev = !app.isPackaged;

  if (isDev) {
    mainWindow.loadURL("http://localhost:5173");
    mainWindow.webContents.openDevTools({ mode: "detach" });
  } else {
    mainWindow.loadFile(path.join(__dirname, "..", "dist", "index.html"));
  }

  mainWindow.on("closed", () => { mainWindow = null; });
}

app.whenReady().then(() => {
  startEngine();
  setTimeout(createWindow, 2000);
});

app.on("window-all-closed", () => {
  stopEngine();
  app.quit();
});

app.on("before-quit", () => {
  stopEngine();
});