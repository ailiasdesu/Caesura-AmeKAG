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
  { id: "ai", label: "AI", icon: "🤖" },
  { id: "live2d", label: "Live2D", icon: "🎭", cls: "live2d" },
  { id: "minigame", label: "3D小游戏", icon: "🎮", cls: "minigame" },
];

const BOTTOM_TABS = [
  { id: "code", label: "代码编辑器" },
  { id: "ai", label: "AI 面板" },
  { id: "logs", label: "日志" },
  { id: "settings", label: "设置" },
];

function AppInner() {
  const { state, dispatch } = useEditor();
  const resizeInitRef = useRef(false);

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

  /* Load saved settings */
  useEffect(() => {
    try {
      const settings = JSON.parse(localStorage.getItem("caesura-ai-settings"));
      if (settings) dispatch({ type: "SET_AI_SETTINGS", payload: settings });
      const provider = localStorage.getItem("caesura-ai-provider");
      if (provider) dispatch({ type: "SET_AI_PROVIDER", payload: provider });
    } catch {}
  }, []);

  /* === Run / Stop === */
  const runScript = async () => {
    try {
      dispatch({ type: "SET_PREVIEW", payload: true });
      dispatch({ type: "ADD_LOG", payload: { level: "info", source: "editor", message: "Running scene script..." } });
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

  /* === Keyboard shortcuts === */
  useEffect(() => {
    const handler = (e) => {
      const t = e.target;
      if (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.isContentEditable) return;
      if (e.key === "g") dispatch({ type: "TOGGLE_GRID" });
      if (e.key === "s") dispatch({ type: "TOGGLE_SAFE" });
      if (e.key === "Escape") dispatch({ type: "SET_SHOW_SAVE_MANAGER", payload: false });
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, []);

  const statusClass = state.status === "connected" ? "connected" : "disconnected";
  const panel = state.activePanel;

  return (
    <div className="app-layout">
      {/* ================================================================
          Toolbar
          ================================================================ */}
      <div className="toolbar">
        <div className="brand-block">
          <div className="brand-logo" />
          <span className="brand-name">Caesura</span>
          <span className={`status-dot ${statusClass}`}
            title={state.status === "connected" ? "引擎已连接" : "引擎断开"}
            onClick={() => dispatch({ type: "SET_STATUS", payload: state.status === "connected" ? "disconnected" : "connected" })} />
        </div>

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

        <div className="toolbar-actions">
          <button className="btn btn-sm" onClick={() => dispatch({ type: "SET_SHOW_SAVE_MANAGER", payload: true })}
            title="存档管理">💾</button>
          <button className="btn btn-success btn-sm" onClick={runScript} disabled={state.previewing}
            title="运行场景 (Ctrl+R)">▶ Run</button>
          <button className="btn btn-danger btn-sm" onClick={stopScript} title="停止">■</button>
        </div>
      </div>

      {/* ================================================================
          Body: Left | Center | Right
          ================================================================ */}
      <div className="editor-body">
        {/* Left Panel */}
        <div id="left-panel" className="panel">
          <div style={{ flex: "60%", display: "flex", flexDirection: "column", overflow: "hidden" }}>
            <SceneList />
          </div>
          <div style={{ flex: "40%", display: "flex", flexDirection: "column", overflow: "hidden", borderTop: "1px solid var(--border)" }}>
            <AssetPanel />
          </div>
        </div>
        <div id="resize-v1" className="resize-v" />

        {/* Center */}
        <div className="editor-center">
          {/* Live2D / MiniGame / Scene canvas */}
          {panel === "live2d" ? (
            <Live2DPanel />
          ) : panel === "minigame" ? (
            <MiniGamePanel />
          ) : (
            <StageView />
          )}

          <div id="resize-h1" className="resize-h" />

          {/* Bottom Panel */}
          <div id="bottom-panel" className="panel">
            <div className="tab-bar">
              {BOTTOM_TABS.map(tab => (
                <button key={tab.id}
                  className={`tab-btn${state.bottomTab === tab.id ? " active" : ""}`}
                  onClick={() => dispatch({ type: "SET_BOTTOM_TAB", payload: tab.id })}
                >
                  {tab.label}
                </button>
              ))}
            </div>

            <div style={{ flex: 1, overflow: "hidden" }}>
              {state.bottomTab === "code" && <CodeEditor />}
              {state.bottomTab === "ai" && <AIPanel />}
              {state.bottomTab === "logs" && <LogPanel />}
              {state.bottomTab === "settings" && <SettingsDialog />}
            </div>
          </div>
        </div>

        {/* Right Panel */}
        <div id="resize-v2" className="resize-v" />
        <div id="props-panel" className="panel">
          <PropertyPanel />
        </div>
      </div>

      {/* ================================================================
          Modals
          ================================================================ */}
      <SaveManager />
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
