import { useEffect, useCallback } from "react";
import { EditorProvider, useEditor } from "./context/EditorContext";
import StageView from "./components/StageView";
import AssetPanel from "./components/AssetPanel";
import Timeline from "./components/Timeline";
import PropertyPanel from "./components/PropertyPanel";
import AIPanel from "./components/AIPanel";
import SettingsDialog from "./components/SettingsDialog";

function AppInner() {
  const { state, dispatch } = useEditor();

  // Poll engine status
  useEffect(() => {
    const ping = async () => {
      try {
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

  // Listen for logs
  useEffect(() => {
    if (!window.caesura.onLog) return;
    window.caesura.onLog((msg) => dispatch({ type: "ADD_LOG", payload: msg }));
  }, []);

  // Load assets
  const loadAssets = useCallback(async () => {
    try {
      const r = await window.caesura.assets();
      dispatch({ type: "SET_ASSETS", payload: r.assets || [] });
    } catch {}
  }, []);
  useEffect(() => { if (state.status === "connected") loadAssets(); }, [state.status, loadAssets]);

  // Load saved AI settings
  useEffect(() => {
    try {
      const settings = JSON.parse(localStorage.getItem("caesura-ai-settings"));
      if (settings) dispatch({ type: "SET_AI_SETTINGS", payload: settings });
      const provider = localStorage.getItem("caesura-ai-provider");
      if (provider) dispatch({ type: "SET_AI_PROVIDER", payload: provider });
    } catch {}
  }, []);

  const runScript = async () => {
    try {
      dispatch({ type: "SET_PREVIEW", payload: true });
      dispatch({ type: "ADD_LOG", payload: { level: "info", message: "Running scene script..." } });
      await window.caesura.run(state.script);
    } catch (e) {
      dispatch({ type: "ADD_LOG", payload: { level: "error", message: e.message } });
    }
    dispatch({ type: "SET_PREVIEW", payload: false });
  };

  const stopScript = async () => {
    try { await window.caesura.stop(); } catch {}
    dispatch({ type: "SET_PREVIEW", payload: false });
  };

  const statusColor = state.status === "connected" ? "#4f8" : state.status === "error" ? "#f84" : "#f44";
  const panel = state.activePanel;

  return (
    <div className="app-layout">
      {/* Left: Asset Browser / Settings */}
      {panel === "settings" ? (
        <SettingsDialog />
      ) : (
        <div className={`panel ${panel === "assets" ? "" : "collapsed"}`} style={{ width: panel === "assets" ? "240px" : "42px" }}>
          <div className="panel-tab" onClick={() => dispatch({ type: "SET_ACTIVE_PANEL", payload: panel === "assets" ? "editor" : "assets" })}>
            {panel === "assets" ? "◂ Assets" : "▸"}
          </div>
          {panel === "assets" && <AssetPanel />}
        </div>
      )}

      {/* Center: Stage / Code Editor */}
      <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
        {/* Toolbar */}
        <div className="toolbar">
          <span style={{ fontSize: 12, fontWeight: 700, color: "#a0a0c0" }}>Caesura (AmeKAG)</span>
          <span style={{ fontSize: 10, padding: "2px 8px", borderRadius: 4, background: statusColor + "22", color: statusColor }}>
            <span className={`status-dot ${state.status}`} />
            {state.status}
          </span>
          <span style={{ fontSize: 10, color: "#444", marginLeft: 4 }}>
            {state.resolution.w}×{state.resolution.h}
          </span>
          <div className="spacer" />
          <button className={`btn ${panel === "stage" ? "btn-blue" : "btn-gray"} btn-sm`}
            onClick={() => dispatch({ type: "SET_ACTIVE_PANEL", payload: panel === "stage" ? "editor" : "stage" })}>
            {panel === "stage" ? "Code" : "Stage"}
          </button>
          <button className={`btn ${panel === "ai" ? "btn-blue" : "btn-gray"} btn-sm`}
            onClick={() => dispatch({ type: "SET_ACTIVE_PANEL", payload: panel === "ai" ? "editor" : "ai" })}>
            AI
          </button>
          <button className="btn btn-green btn-sm" onClick={runScript} disabled={state.previewRunning}>
            ▶ Run
          </button>
          <button className="btn btn-red btn-sm" onClick={stopScript}>■ Stop</button>
        </div>

        {/* Code Editor or Stage */}
        {panel === "stage" ? (
          <StageView />
        ) : (
          <textarea className="code-editor" value={state.script}
            onChange={(e) => dispatch({ type: "SET_SCRIPT", payload: e.target.value })}
            spellCheck={false} />
        )}

        {/* Timeline */}
        <Timeline />
      </div>

      {/* Right: Properties / AI / Logs */}
      {panel !== "settings" && (
        <div className={`panel ${["props", "ai", "logs"].includes(panel) ? "" : "collapsed"}`}
          style={{ width: ["props", "ai", "logs"].includes(panel) ? "280px" : "42px" }}>
          <div style={{ display: "flex", borderBottom: "1px solid #1a1a24" }}>
            <div className="panel-tab" style={{ flex: 1 }}
              onClick={() => dispatch({ type: "SET_ACTIVE_PANEL", payload: panel === "props" ? "editor" : "props" })}>
              {panel === "props" ? "Props ▸" : "▸"}
            </div>
            <div className="panel-tab" style={{ flex: 1 }}
              onClick={() => dispatch({ type: "SET_ACTIVE_PANEL", payload: panel === "ai" ? "editor" : "ai" })}>
              {panel === "ai" ? "AI ▸" : "▸"}
            </div>
            <div className="panel-tab" style={{ flex: 1 }}
              onClick={() => dispatch({ type: "SET_ACTIVE_PANEL", payload: panel === "logs" ? "editor" : "logs" })}>
              {panel === "logs" ? "Logs ▸" : "▸"}
            </div>
          </div>

          {panel === "props" && <PropertyPanel />}
          {panel === "ai" && <AIPanel />}
          {panel === "logs" && (
            <div className="panel-content" style={{ padding: 8, fontFamily: "monospace", fontSize: 11 }}>
              {state.logs.length === 0 && (
                <div style={{ color: "#555", textAlign: "center", padding: 16 }}>No logs yet.<br />Run a script to see output.</div>
              )}
              {state.logs.map((l, i) => (
                <div key={i} style={{ padding: "1px 0", color: l.level === "error" ? "#f66" : l.level === "warn" ? "#fa0" : "#888" }}>
                  <span style={{ color: "#444" }}>{l.time}</span> {l.message}
                </div>
              ))}
            </div>
          )}
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