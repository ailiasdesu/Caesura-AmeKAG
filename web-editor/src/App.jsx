import { useEffect, useCallback, useRef, useState } from "react";
import { EditorProvider, useEditor } from "./context/EditorContext";
import SceneList from "./components/SceneList";
import StageView from "./components/StageView";
import AssetPanel from "./components/AssetPanel";
import PropertyPanel from "./components/PropertyPanel";
import AIPanel from "./components/AIPanel";
import CodeEditor from "./components/CodeEditor";
import LogPanel from "./components/LogPanel";
import Live2DPanel from "./components/Live2DPanel";
import MiniGamePanel from "./components/MiniGamePanel";
import SettingsDialog from "./components/SettingsDialog";
import SaveManager from "./components/SaveManager";
import "./styles/editor.css";

const TOOLBAR_TABS = [
  { id: "scene", label: "场景", icon: "📋" },
  { id: "code", label: "代码", icon: "<>" },
  { id: "live2d", label: "Live2D", icon: "🎭", cls: "live2d" },
  { id: "minigame", label: "3D小游戏", icon: "🎮", cls: "minigame" },
];

const BOTTOM_TABS = [
  { id: "code", label: "代码编辑器" },
  { id: "ai", label: "AI 面板" },
  { id: "logs", label: "日志" },

];

const PLATFORMS = [
  { id: "win", label: "🪟 Windows", script: "package:win" },
  { id: "mac", label: "🍎 macOS", script: "package:mac" },
  { id: "linux", label: "🐧 Linux", script: "package:linux" },
];

const FILE_MENU_ITEMS = [
  { id: "new", label: "新建文件", shortcut: "Ctrl+N", icon: "📄" },
  { id: "open", label: "打开文件...", shortcut: "Ctrl+O", icon: "📂" },
  { id: "save", label: "保存", shortcut: "Ctrl+S", icon: "💾" },
  { id: "saveAs", label: "另存为...", shortcut: "Ctrl+Shift+S", icon: "📥" },
  { id: "divider", label: "", icon: "" },
  { id: "saveProject", label: "保存项目", shortcut: "", icon: "📁" },
  { id: "openProject", label: "打开项目...", shortcut: "", icon: "📂" },
  { id: "divider2", label: "", icon: "" },
  { id: "exportCARC", label: "导出 CARC 包...", shortcut: "", icon: "📦" },
  { id: "divider3", label: "", icon: "" },
  { id: "exit", label: "退出", shortcut: "Alt+F4", icon: "🚪" },
];

function AppInner() {
  const { state, dispatch } = useEditor();
  const resizeInitRef = useRef(false);
  const [showFileMenu, setShowFileMenu] = useState(false);
  const [showSettings, setShowSettings] = useState(false);
  const [showPlatformMenu, setShowPlatformMenu] = useState(false);

  /* === Engine RPC === */
  useEffect(() => {
    const ping = async () => {
      try {
        if (!window.caesura) return;
        const r = await window.caesura.ping();
        dispatch({ type: "SET_STATUS", payload: r.result === "ok" ? "connected" : "error" });
      } catch {
        dispatch({ type: "SET_STATUS", payload: "disconnected" });
      }
    };
    ping();
    const iv = setInterval(ping, 3000);
    return () => clearInterval(iv);
  }, []);

  useEffect(() => {
    if (!window.caesura?.onLog) return;
    window.caesura.onLog((msg) => dispatch({ type: "ADD_LOG", payload: msg }));
  }, []);

  const loadAssets = useCallback(async () => {
    try {
      if (!window.caesura) return;
      const r = await window.caesura.assets();
      dispatch({ type: "SET_ASSETS", payload: r.assets || [] });
    } catch {}
  }, []);
  useEffect(() => { if (state.status === "connected") loadAssets(); }, [state.status, loadAssets]);

  useEffect(() => {
    try {
      const s = JSON.parse(localStorage.getItem("caesura-ai-settings"));
      if (s) dispatch({ type: "SET_AI_SETTINGS", payload: s });
      const p = localStorage.getItem("caesura-ai-provider");
      if (p) dispatch({ type: "SET_AI_PROVIDER", payload: p });
    } catch {}
  }, []);

  const runScript = async () => {
    try {
      dispatch({ type: "SET_PREVIEW", payload: true });
      dispatch({ type: "ADD_LOG", payload: { level: "info", source: "editor", message: "Running..." } });
      if (window.caesura) await window.caesura.run(state.script);
    } catch (e) {
      dispatch({ type: "ADD_LOG", payload: { level: "error", source: "editor", message: e.message } });
    }
    dispatch({ type: "SET_PREVIEW", payload: false });
  };

  const stopScript = async () => {
    try { if (window.caesura) await window.caesura.stop(); } catch {}
    dispatch({ type: "SET_PREVIEW", payload: false });
  };

  const handleFileAction = (id) => {
    setShowFileMenu(false);
    switch (id) {
      case "new":
        dispatch({ type: "OPEN_FILE", payload: { name: `untitled-${Date.now()}.lua`, content: "" } });
        dispatch({ type: "SET_ACTIVE_PANEL", payload: "code" });
        break;
      case "save":
        dispatch({ type: "MARK_SAVED" });
        dispatch({ type: "ADD_LOG", payload: { level: "info", source: "editor", message: "File saved" } });
        break;
      case "saveAs":
        dispatch({ type: "ADD_LOG", payload: { level: "info", source: "editor", message: "Save As dialog (native)" } });
        break;
      case "settings":
        dispatch({ type: "SET_BOTTOM_TAB", payload: "settings" });
        break;
    }
  };

  const handlePackage = async () => {
    dispatch({ type: "SET_PACKAGING", payload: true });
    dispatch({ type: "ADD_LOG", payload: { level: "info", source: "build", message: `Packaging for ${state.packagePlatform.toUpperCase()}...` } });
    try {
      if (window.caesura?.package) {
        const r = await window.caesura.package(state.packagePlatform);
        dispatch({ type: "SET_PACKAGE_RESULT", payload: r });
        dispatch({ type: "ADD_LOG", payload: { level: "info", source: "build", message: `Done: ${r.path || "release/"}` } });
      } else {
        dispatch({ type: "ADD_LOG", payload: { level: "info", source: "build", message: `Run: cd web-editor && npm run package:${state.packagePlatform}` } });
      }
    } catch (e) {
      dispatch({ type: "SET_PACKAGE_RESULT", payload: { error: e.message } });
      dispatch({ type: "ADD_LOG", payload: { level: "error", source: "build", message: `Failed: ${e.message}` } });
    }
    dispatch({ type: "SET_PACKAGING", payload: false });
  };

  /* === Resize handles === */
  useEffect(() => {
    if (resizeInitRef.current) return;
    resizeInitRef.current = true;
    const initResizeV = (hid, sel, minW, maxW) => {
      const h = document.getElementById(hid), p = document.querySelector(sel);
      if (!h || !p) return;
      let sx, sw;
      h.addEventListener("mousedown", (e) => {
        e.preventDefault(); sx = e.clientX; sw = p.getBoundingClientRect().width;
        h.classList.add("active"); document.body.style.cursor = "col-resize"; document.body.style.userSelect = "none";
        const m = (ev) => { const dx = ev.clientX - sx; let nw = sw + dx; nw = Math.max(minW, Math.min(maxW, nw)); p.style.width = nw + "px"; p.style.minWidth = nw + "px"; };
        const u = () => { h.classList.remove("active"); document.body.style.cursor = ""; document.body.style.userSelect = ""; document.removeEventListener("mousemove", m); document.removeEventListener("mouseup", u); };
        document.addEventListener("mousemove", m); document.addEventListener("mouseup", u);
      });
    };
    const initResizeH = (hid, sel, minH, maxH) => {
      const h = document.getElementById(hid), p = document.querySelector(sel);
      if (!h || !p) return;
      let sy, sh;
      h.addEventListener("mousedown", (e) => {
        e.preventDefault(); sy = e.clientY; sh = p.getBoundingClientRect().height;
        h.classList.add("active"); document.body.style.cursor = "row-resize"; document.body.style.userSelect = "none";
        const m = (ev) => { const dy = ev.clientY - sy; let nh = sh + dy; nh = Math.max(minH, Math.min(maxH, nh)); p.style.height = nh + "px"; p.style.minHeight = nh + "px"; };
        const u = () => { h.classList.remove("active"); document.body.style.cursor = ""; document.body.style.userSelect = ""; document.removeEventListener("mousemove", m); document.removeEventListener("mouseup", u); };
        document.addEventListener("mousemove", m); document.addEventListener("mouseup", u);
      });
    };
    initResizeV("resize-v1", "#left-panel", 180, 380);
    initResizeV("resize-v2", "#props-panel", 180, 400);
    initResizeH("resize-h1", "#bottom-panel", 80, 400);
  }, []);

  useEffect(() => {
    const handler = (e) => {
      const t = e.target;
      if (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.isContentEditable) {
        if (e.ctrlKey && e.key === "s") { e.preventDefault(); dispatch({ type: "MARK_SAVED" }); }
        return;
      }
      if (e.key === "g") dispatch({ type: "TOGGLE_GRID" });
      if (e.key === "s") dispatch({ type: "TOGGLE_SAFE" });
      if (e.key === "Escape") { dispatch({ type: "SET_SHOW_SAVE_MANAGER", payload: false }); setShowFileMenu(false); setShowPlatformMenu(false); }
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, []);

  const statusClass = state.status === "connected" ? "connected" : "disconnected";
  const panel = state.activePanel;
  const currentPlatform = PLATFORMS.find(p => p.id === state.packagePlatform) || PLATFORMS[0];

  return (
    <div className="app-layout" onClick={() => { setShowFileMenu(false); setShowPlatformMenu(false); }}>
      {/* ================================================================
          Toolbar — VS Code style
          ================================================================ */}
      <div className="toolbar">
        {/* File menu */}
        <div style={{ position: "relative" }}>
          <button className="btn btn-sm toolbar-file-btn"
            onClick={(e) => { e.stopPropagation(); setShowFileMenu(!showFileMenu); }}
            style={{ fontWeight: 600, letterSpacing: "0.02em", gap: 4 }}
          >
            <span style={{ fontSize: 14, lineHeight: 1 }}>☰</span>
            <span style={{ fontSize: 12 }}>文件</span>
          </button>
          {showFileMenu && (
            <div className="file-menu-dropdown" onClick={e => e.stopPropagation()}>
              {FILE_MENU_ITEMS.map(item => {
                if (item.id.startsWith("divider")) {
                  return <div key={item.id} className="file-menu-divider" />;
                }
                return (
                  <div key={item.id} className="file-menu-item"
                    onClick={() => handleFileAction(item.id)}
                  >
                    <span style={{ width: 18, textAlign: "center" }}>{item.icon}</span>
                    <span style={{ flex: 1 }}>{item.label}</span>
                    <span className="file-menu-shortcut">{item.shortcut}</span>
                  </div>
                );
              })}
            </div>
          )}
        </div>

        {/* Separator */}
        <div className="toolbar-sep" />

        {/* Activity tabs */}
        <div className="toolbar-tabs">
          {TOOLBAR_TABS.map(tab => (
            <div key={tab.id}
              className={`toolbar-tab${panel === tab.id ? " active" : ""}${tab.cls ? ` ${tab.cls}-tab` : ""}`}
              onClick={() => dispatch({ type: "SET_ACTIVE_PANEL", payload: tab.id })}
            >
              <span>{tab.icon}</span> {tab.label}
            </div>
          ))}
        </div>

        {/* Right actions */}
        <div className="toolbar-actions">
          {/* Settings */}
          <button className="btn btn-icon btn-sm toolbar-icon-btn" onClick={() => setShowSettings(true)}
            title="设置 (Ctrl+,)"
          >
            ⚙
          </button>

          {/* Platform selector */}
          <div style={{ position: "relative" }}>
            <button className="btn btn-sm"
              onClick={(e) => { e.stopPropagation(); setShowPlatformMenu(!showPlatformMenu); }}
            >
              {currentPlatform.label} ▼
            </button>
            {showPlatformMenu && (
              <div className="file-menu-dropdown" onClick={e => e.stopPropagation()} style={{ minWidth: 150 }}>
                {PLATFORMS.map(p => (
                  <div key={p.id} className="file-menu-item"
                    onClick={() => { dispatch({ type: "SET_PACKAGE_PLATFORM", payload: p.id }); setShowPlatformMenu(false); }}
                  >
                    <span>{state.packagePlatform === p.id ? "✓ " : "  "}{p.label}</span>
                  </div>
                ))}
              </div>
            )}
          </div>

          {/* Package button */}
          <button className={`btn btn-accent btn-sm${state.packaging ? " disabled" : ""}`}
            onClick={handlePackage} disabled={state.packaging} title="一键构建 + 打包"
          >
            {state.packaging ? "⏳ 打包中..." : "📦 一键打包"}
          </button>

          {/* Save */}
          <button className="btn btn-icon btn-sm toolbar-icon-btn" onClick={() => setShowSettings(true)}
          >
            💾
          </button>

          {/* Run / Stop */}
          <button className="btn btn-success btn-sm" onClick={runScript} disabled={state.previewing}
            title="运行场景 (Ctrl+R)">▶ Run</button>
          <button className="btn btn-danger btn-sm" onClick={stopScript} title="停止">■</button>
        </div>
      </div>

      {/* ================================================================
          Body: Left | Center | Right
          ================================================================ */}
      <div className="editor-body">
        <div id="left-panel" className="panel">
          <div style={{ flex: "60%", display: "flex", flexDirection: "column", overflow: "hidden" }}>
            <SceneList />
          </div>
          <div style={{ flex: "40%", display: "flex", flexDirection: "column", overflow: "hidden", borderTop: "1px solid var(--border)" }}>
            <AssetPanel />
          </div>
        </div>
        <div id="resize-v1" className="resize-v" />

        <div className="editor-center">
          {panel === "code" ? (
            <CodeEditor />
          ) : panel === "live2d" ? (
            <Live2DPanel />
          ) : panel === "minigame" ? (
            <MiniGamePanel />
          ) : (
            <StageView />
          )}

          <div id="resize-h1" className="resize-h" />

          <div id="bottom-panel" className="panel">
            <div className="tab-bar">
              {BOTTOM_TABS.map(tab => (
                <button key={tab.id}
                  className={`tab-btn${state.bottomTab === tab.id ? " active" : ""}`}
                  onClick={() => dispatch({ type: "SET_BOTTOM_TAB", payload: tab.id })}
                >{tab.label}</button>
              ))}
            </div>
            <div style={{ flex: 1, overflow: "hidden" }}>
              {state.bottomTab === "code" && <CodeEditor />}
              {state.bottomTab === "ai" && <AIPanel />}
              {state.bottomTab === "logs" && <LogPanel />}
              {}
            </div>
          </div>
        </div>

        <div id="resize-v2" className="resize-v" />
        <div id="props-panel" className="panel">
          <PropertyPanel />
        </div>
      </div>

      <SaveManager />
      {showSettings && <SettingsDialog onClose={() => setShowSettings(false)} />}

      {state.packageResult && (
        <div className="toast" onClick={() => dispatch({ type: "SET_PACKAGE_RESULT", payload: null })}>
          {state.packageResult.error
            ? <span style={{ color: "var(--red)" }}>❌ {state.packageResult.error}</span>
            : <span>✅ 打包完成 → {state.packageResult.path || "release/"}</span>}
        </div>
      )}
    </div>
  );
}

export default function App() {
  return (
    <EditorProvider>
      <AppInner />
    </EditorProvider>
  );
}
