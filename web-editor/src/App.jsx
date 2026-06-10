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
import SettingsPage from "./components/SettingsPage";
import SaveManager from "./components/SaveManager";
import CommandPalette from "./components/CommandPalette";
import "./styles/editor.css";

// =========================================================================
// SVG Icons (inline — lightweight)
// =========================================================================
const Icons = {
  Explorer: () => (
    <svg width="22" height="22" viewBox="0 0 22 22" fill="none">
      <rect x="2" y="2" width="8" height="8" rx="1.5" stroke="currentColor" strokeWidth="1.5"/>
      <rect x="12" y="2" width="8" height="8" rx="1.5" stroke="currentColor" strokeWidth="1.5"/>
      <rect x="2" y="12" width="8" height="8" rx="1.5" stroke="currentColor" strokeWidth="1.5"/>
      <rect x="12" y="12" width="8" height="8" rx="1.5" stroke="currentColor" strokeWidth="1.5"/>
    </svg>
  ),
  Search: () => (
    <svg width="20" height="20" viewBox="0 0 20 20" fill="none">
      <circle cx="8.5" cy="8.5" r="5.5" stroke="currentColor" strokeWidth="1.5"/>
      <line x1="12.5" y1="12.5" x2="17" y2="17" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round"/>
    </svg>
  ),
  Scenes: () => (
    <svg width="20" height="20" viewBox="0 0 20 20" fill="none">
      <rect x="2" y="3" width="16" height="4" rx="1" stroke="currentColor" strokeWidth="1.5"/>
      <rect x="2" y="9" width="16" height="4" rx="1" stroke="currentColor" strokeWidth="1.5"/>
      <rect x="2" y="15" width="16" height="4" rx="1" stroke="currentColor" strokeWidth="1.5"/>
    </svg>
  ),
  AI: () => (
    <svg width="20" height="20" viewBox="0 0 20 20" fill="none">
      <circle cx="10" cy="10" r="7" stroke="currentColor" strokeWidth="1.5"/>
      <path d="M7 10h6M10 7v6" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round"/>
      <circle cx="10" cy="10" r="2" fill="currentColor"/>
    </svg>
  ),
  Live2D: () => (
    <svg width="20" height="20" viewBox="0 0 20 20" fill="none">
      <circle cx="10" cy="7" r="3" stroke="currentColor" strokeWidth="1.5"/>
      <path d="M4 18c0-4 2.5-7 6-7s6 3 6 7" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round"/>
    </svg>
  ),
  MiniGame: () => (
    <svg width="20" height="20" viewBox="0 0 20 20" fill="none">
      <rect x="3" y="3" width="14" height="14" rx="2" stroke="currentColor" strokeWidth="1.5"/>
      <circle cx="10" cy="10" r="4" stroke="currentColor" strokeWidth="1.5"/>
    </svg>
  ),
  Package: () => (
    <svg width="20" height="20" viewBox="0 0 20 20" fill="none">
      <rect x="3" y="6" width="14" height="11" rx="1.5" stroke="currentColor" strokeWidth="1.5"/>
      <path d="M7 6V3.5a1 1 0 011-1h4a1 1 0 011 1V6" stroke="currentColor" strokeWidth="1.5"/>
      <line x1="10" y1="9" x2="10" y2="14" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round"/>
    </svg>
  ),
  Settings: () => (
    <svg width="20" height="20" viewBox="0 0 20 20" fill="none">
      <circle cx="10" cy="10" r="3" stroke="currentColor" strokeWidth="1.5"/>
      <path d="M10 2v2m0 12v2M2 10h2m12 0h2M4.93 4.93l1.41 1.41m7.32 7.32l1.41 1.41M4.93 15.07l1.41-1.41m7.32-7.32l1.41-1.41" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round"/>
    </svg>
  ),
  Close: () => (
    <svg width="12" height="12" viewBox="0 0 12 12" fill="none">
      <line x1="2" y1="2" x2="10" y2="10" stroke="currentColor" strokeWidth="1.3" strokeLinecap="round"/>
      <line x1="10" y1="2" x2="2" y2="10" stroke="currentColor" strokeWidth="1.3" strokeLinecap="round"/>
    </svg>
  ),
  Minimize: () => (
    <svg width="12" height="12" viewBox="0 0 12 12" fill="none">
      <rect x="1.5" y="5.5" width="9" height="1" rx="0.5" fill="currentColor"/>
    </svg>
  ),
  Maximize: () => (
    <svg width="12" height="12" viewBox="0 0 12 12" fill="none">
      <rect x="2" y="2" width="8" height="8" rx="1" stroke="currentColor" strokeWidth="1.2"/>
    </svg>
  ),
  ChevronRight: () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none">
      <path d="M5 2l5 5-5 5" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round"/>
    </svg>
  ),
  Play: () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none">
      <polygon points="3,1 13,7 3,13" stroke="currentColor" strokeWidth="1.2" strokeLinejoin="round"/>
    </svg>
  ),
  Stop: () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none">
      <rect x="2" y="2" width="10" height="10" rx="1" stroke="currentColor" strokeWidth="1.5"/>
    </svg>
  ),
  Grid: () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none">
      <rect x="1" y="1" width="5" height="5" rx="0.5" stroke="currentColor" strokeWidth="1.2"/>
      <rect x="8" y="1" width="5" height="5" rx="0.5" stroke="currentColor" strokeWidth="1.2"/>
      <rect x="1" y="8" width="5" height="5" rx="0.5" stroke="currentColor" strokeWidth="1.2"/>
      <rect x="8" y="8" width="5" height="5" rx="0.5" stroke="currentColor" strokeWidth="1.2"/>
    </svg>
  ),
  Safe: () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none">
      <rect x="2" y="2" width="10" height="10" rx="1.5" stroke="currentColor" strokeWidth="1.2" strokeDasharray="2 1.5"/>
    </svg>
  ),
  Add: () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none">
      <line x1="7" y1="2" x2="7" y2="12" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round"/>
      <line x1="2" y1="7" x2="12" y2="7" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round"/>
    </svg>
  ),
};

// =========================================================================
// Activity Bar
// =========================================================================
const ACTIVITIES = [
  { id: "explorer", Icon: Icons.Explorer, label: "\u8D44\u6E90\u7BA1\u7406\u5668" },
  { id: "search",   Icon: Icons.Search,   label: "\u641C\u7D22" },
  { id: "scenes",   Icon: Icons.Scenes,   label: "\u573A\u666F\u5217\u8868" },
  { id: "ai",       Icon: Icons.AI,       label: "AI \u52A9\u624B" },
  { id: "live2d",   Icon: Icons.Live2D,   label: "Live2D" },
  { id: "minigame", Icon: Icons.MiniGame, label: "3D \u5C0F\u6E38\u620F" },
];

function ActivityBar({ active, sidebarVisible, onActivityClick, onPackageClick }) {
  return (
    <div className="activity-bar">
      {ACTIVITIES.map(a => (
        <div key={a.id}
          className={`activity-btn${active === a.id && sidebarVisible ? " active" : ""}`}
          onClick={() => onActivityClick(a.id)}
          title={a.label}
        >
          <a.Icon />
          <span className="activity-tooltip">{a.label}</span>
        </div>
      ))}
      <div className="activity-spacer" />
      <div className="activity-btn" onClick={onPackageClick} title="\u4E00\u952E\u6253\u5305">
        <Icons.Package />
        <span className="activity-tooltip">\u4E00\u952E\u6253\u5305</span>
      </div>
      <div className="activity-btn" onClick={() => onActivityClick("settings")} title="\u8BBE\u7F6E">
        <Icons.Settings />
        <span className="activity-tooltip">\u8BBE\u7F6E (Ctrl+,)</span>
      </div>
    </div>
  );
}

// =========================================================================
// Sidebar
// =========================================================================
const SIDEBAR_TITLES = {
  explorer: "\u8D44\u6E90\u7BA1\u7406\u5668",
  search: "\u641C\u7D22",
  scenes: "\u573A\u666F\u5217\u8868",
  ai: "AI \u52A9\u624B",
  live2d: "Live2D",
  minigame: "3D \u5C0F\u6E38\u620F",
};

function Sidebar({ activity, visible, onCollapse }) {
  if (!visible) return null;
  const title = SIDEBAR_TITLES[activity] || "";

  return (
    <div className="sidebar">
      <div className="sidebar-header">
        <span className="sidebar-title">{title}</span>
        <div className="sidebar-actions">
          <button className="sidebar-btn" onClick={onCollapse} title="\u6298\u53E0">
            <Icons.Close />
          </button>
        </div>
      </div>
      <div className="sidebar-body">
        {activity === "explorer" && (
          <>
            <SceneList />
            <AssetPanel />
          </>
        )}
        {activity === "search" && (
          <div style={{ padding: 12 }}>
            <input className="asset-search" placeholder="\u641C\u7D22\u6587\u4EF6..." style={{ width: "100%", margin: 0 }} />
          </div>
        )}
        {activity === "scenes" && <SceneList />}
        {activity === "ai" && <AIPanel />}
        {activity === "live2d" && <Live2DPanel />}
        {activity === "minigame" && <MiniGamePanel />}
      </div>
    </div>
  );
}

// =========================================================================
// Editor Area
// =========================================================================
function EditorArea({ state, dispatch, runScript, stopScript }) {
  const { activePanel } = state;

  return (
    <div className="editor-area">
      {/* Tabs */}
      <div className="editor-tabs">
        {activePanel === "code" && state.openFiles?.map((f, i) => (
          <div key={i}
            className={`editor-tab code-tab${i === state.activeFileIdx ? " active" : ""}`}
            onClick={() => dispatch({ type: "SET_ACTIVE_FILE", payload: i })}
          >
            {f.name}
            {state.unsaved && i === state.activeFileIdx && <span style={{ color: "var(--vscode-focusBorder)", fontSize: 14 }}>{'\u25CF'}</span>}
            <span className="tab-close" onClick={(e) => { e.stopPropagation(); dispatch({ type: "CLOSE_FILE", payload: i }); }}>
              <Icons.Close />
            </span>
          </div>
        ))}
        <div className={`editor-tab stage-tab${activePanel === "scene" ? " active" : ""}`}
          onClick={() => dispatch({ type: "SET_ACTIVE_PANEL", payload: "scene" })}>
          {'\uD83C\uDFAC'} \u821E\u53F0
        </div>
        <div className={`editor-tab code-tab${activePanel === "code" ? " active" : ""}`}
          onClick={() => dispatch({ type: "SET_ACTIVE_PANEL", payload: "code" })}>
          {'<>'} \u4EE3\u7801
        </div>
        <div className="editor-actions" style={{ display: "flex", alignItems: "center", marginLeft: "auto", paddingRight: 8, gap: 4 }}>
          <button className="btn btn-sm" onClick={runScript} disabled={state.previewing} title="\u8FD0\u884C (F5)">
            <Icons.Play />
          </button>
          <button className="btn btn-sm" onClick={stopScript} title="\u505C\u6B62 (Shift+F5)">
            <Icons.Stop />
          </button>
        </div>
      </div>

      {/* Content (mutually exclusive) */}
      <div className="editor-body">
        {activePanel === "scene" && <StageView />}
        {activePanel === "code" && <CodeEditor />}
      </div>
    </div>
  );
}

// =========================================================================
// Status Bar
// =========================================================================
function StatusBar({ state, onPanelToggle }) {
  return (
    <div className="statusbar">
      <div className="statusbar-left">
        <div className="statusbar-item" onClick={onPanelToggle}>
          <span className={`statusbar-dot${state.status === "connected" ? "" : " offline"}`} />
          <span>{state.status === "connected" ? "\u5F15\u64CE\u5DF2\u8FDE\u63A5" : "\u5F15\u64CE\u672A\u8FDE\u63A5"}</span>
        </div>
        <div className="statusbar-item">
          <span>{Math.round((state.stageZoom || 0.55) * 100)}%</span>
        </div>
        <div className="statusbar-item">
          <span>{state.resolution ? state.resolution.w + "\u00D7" + state.resolution.h : "1920\u00D71080"}</span>
        </div>
      </div>
      <div className="statusbar-right">
        <div className="statusbar-item">
          <Icons.Play />
          <span>{(state.openFiles?.length || 0)} \u6587\u4EF6</span>
        </div>
      </div>
    </div>
  );
}

// =========================================================================
// Package Panel (modal)
// =========================================================================
const PACKAGE_PLATFORMS = [
  { id: "win", label: "Windows", ext: ".exe", gpu: "D3D11", color: "#40a0e0" },
  { id: "mac", label: "macOS", ext: ".app", gpu: "Metal", color: "#e2e2f0" },
  { id: "linux", label: "Linux", ext: ".AppImage", gpu: "OpenGL", color: "#f0a040" },
  { id: "web", label: "Web", ext: ".html", gpu: "WebGL", color: "#00d68f" },
];

function PackagePanel({ show, onClose, onBuild }) {
  const [building, setBuilding] = useState(null);
  const [progress, setProgress] = useState(0);

  const handleBuild = (platform) => {
    setBuilding(platform);
    setProgress(0);
    const iv = setInterval(() => {
      setProgress(p => {
        if (p >= 100) { clearInterval(iv); setBuilding(null); onBuild(platform); return 100; }
        return p + Math.random() * 25 + 5;
      });
    }, 200);
  };

  if (!show) return null;
  return (
    <div className="package-panel open" onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className="modal" style={{ maxWidth: 500 }}>
        <h3 style={{ marginBottom: 12, color: "var(--vscode-foreground)", fontSize: 14 }}>{'\u4E00\u952E\u6253\u5305'}</h3>
        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 8 }}>
          {PACKAGE_PLATFORMS.map(p => (
            <div key={p.id} className="package-card" onClick={() => handleBuild(p.id)}
              style={{ borderColor: building === p.id ? "var(--vscode-focusBorder)" : undefined }}
            >
              <div style={{ fontSize: 18, fontWeight: 700, color: p.color }}>{p.label}</div>
              <div style={{ fontSize: 11, color: "var(--vscode-descriptionForeground)" }}>{p.ext} \u00B7 {p.gpu}</div>
              {building === p.id && (
                <div style={{ marginTop: 8, height: 3, background: "var(--vscode-sideBar-border)", borderRadius: 2, overflow: "hidden" }}>
                  <div style={{ width: Math.min(progress, 100) + "%", height: "100%", background: p.color, transition: "width 0.2s" }} />
                </div>
              )}
            </div>
          ))}
        </div>
        <div style={{ marginTop: 12, textAlign: "right" }}>
          <button className="btn" onClick={onClose}>{'\u53D6\u6D88'}</button>
        </div>
      </div>
    </div>
  );
}

// =========================================================================
// Toast system
// =========================================================================
function ToastContainer({ toasts }) {
  return (
    <div className="toast-container">
      {toasts.map(t => <div key={t.id} className="toast">{t.text}</div>)}
    </div>
  );
}

// =========================================================================
// Main App
// =========================================================================
function AppInner() {
  const { state, dispatch } = useEditor();
  const [sidebarActivity, setSidebarActivity] = useState("explorer");
  const [sidebarVisible, setSidebarVisible] = useState(true);
  const [sidebarWidth, setSidebarWidth] = useState(260);
  const [panelOpen, setPanelOpen] = useState(false);
  const [panelTab, setPanelTab] = useState("debug");
  const [panelHeight, setPanelHeight] = useState(200);
  const [showSettings, setShowSettings] = useState(false);
  const [showCommandPalette, setShowCommandPalette] = useState(false);
  const [showSaveManager, setShowSaveManager] = useState(false);
  const [showPackagePanel, setShowPackagePanel] = useState(false);
  const [toasts, setToasts] = useState([]);
  const resizeRef = useRef({ side: false, panel: false });

  const addToast = useCallback((text) => {
    const id = Date.now();
    setToasts(t => [...t, { id, text }]);
    setTimeout(() => setToasts(t => t.filter(x => x.id !== id)), 2000);
  }, []);

  // Activity click
  const handleActivityClick = useCallback((id) => {
    if (id === "settings") { setShowSettings(true); return; }
    if (sidebarActivity === id && sidebarVisible) { setSidebarVisible(false); return; }
    setSidebarActivity(id);
    setSidebarVisible(true);
  }, [sidebarActivity, sidebarVisible]);

  // Run / Stop
  const runScript = useCallback(() => {
    if (state.previewing) return;
    dispatch({ type: "SET_PREVIEW", payload: true });
    addToast("\u8FD0\u884C KAG \u811A\u672C...");
  }, [state.previewing, dispatch, addToast]);

  const stopScript = useCallback(() => {
    dispatch({ type: "SET_PREVIEW", payload: false });
    addToast("\u505C\u6B62\u8FD0\u884C");
  }, [dispatch, addToast]);

  // Package build
  const handlePackageBuild = useCallback((platform) => {
    addToast("\u2713 \u5DF2\u6253\u5305 " + platform + " \u2192 ./dist/");
    setShowPackagePanel(false);
  }, [addToast]);

  // Keyboard shortcuts
  useEffect(() => {
    const handler = (e) => {
      const tag = (e.target || {}).tagName;
      if (tag === "INPUT" || tag === "TEXTAREA" || e.target?.isContentEditable) return;
      if (e.ctrlKey && e.shiftKey && e.key === "P") { e.preventDefault(); setShowCommandPalette(true); }
      if (e.ctrlKey && e.key === "p") { e.preventDefault(); runScript(); }
      if (e.ctrlKey && e.key === "s") { e.preventDefault(); setShowSaveManager(true); }
      if (e.ctrlKey && e.key === ",") { e.preventDefault(); setShowSettings(true); }
      if (e.key === "g") dispatch({ type: "TOGGLE_GRID" });
      if (e.key === "s" && !e.ctrlKey) dispatch({ type: "TOGGLE_SAFE" });
      if (e.key === "Escape") { setShowCommandPalette(false); setShowSettings(false); setShowSaveManager(false); }
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, [runScript, dispatch]);

  // Resize
  useEffect(() => {
    const mm = (e) => {
      if (resizeRef.current.side) setSidebarWidth(Math.max(180, Math.min(420, e.clientX - 48)));
      if (resizeRef.current.panel) setPanelHeight(Math.max(28, Math.min(400, window.innerHeight - e.clientY - 22)));
    };
    const mu = () => { resizeRef.current = { side: false, panel: false }; };
    window.addEventListener("mousemove", mm);
    window.addEventListener("mouseup", mu);
    return () => { window.removeEventListener("mousemove", mm); window.removeEventListener("mouseup", mu); };
  }, []);

  // Settings fullscreen
  if (showSettings) {
    return (
      <div className="caesura-workbench">
        <SettingsPage onClose={() => setShowSettings(false)} />
      </div>
    );
  }

  const PANEL_TABS = [
    { id: "problems", label: "\u95EE\u9898" },
    { id: "output", label: "\u8F93\u51FA" },
    { id: "debug", label: "\u8C03\u8BD5\u63A7\u5236\u53F0" },
    { id: "ai", label: "AI \u5BF9\u8BDD" },
  ];

  return (
    <div className="caesura-workbench">
      {/* Title Bar */}
      <div className="titlebar">
        <div className="titlebar-menus">
          {["\u6587\u4EF6","\u7F16\u8F91","\u9009\u62E9","\u67E5\u770B","\u8F6C\u5230","\u8FD0\u884C","\u7EC8\u7AEF","\u5E2E\u52A9"].map(m => (
            <div key={m} className="titlebar-menu">{m}</div>
          ))}
        </div>
        <div className="titlebar-center">
          {(state.openFiles?.[state.activeFileIdx]?.name || "untitled.cae") + " \u2014 C\u00E6sura Editor"}
        </div>
        <div className="titlebar-actions">
          <button className="win-btn" onClick={() => addToast("\u6700\u5C0F\u5316")}><Icons.Minimize /></button>
          <button className="win-btn" onClick={() => addToast("\u6700\u5927\u5316")}><Icons.Maximize /></button>
          <button className="win-btn close"><Icons.Close /></button>
        </div>
      </div>

      {/* Workbench */}
      <div className="workbench">
        <ActivityBar active={sidebarActivity} sidebarVisible={sidebarVisible}
          onActivityClick={handleActivityClick} onPackageClick={() => setShowPackagePanel(true)} />

        {sidebarVisible && (
          <div className="resize-handle-v"
            onMouseDown={() => { resizeRef.current.side = true; }} />
        )}

        <Sidebar activity={sidebarActivity} visible={sidebarVisible}
          onCollapse={() => setSidebarVisible(false)} />

        {/* Center: Editor + Panel */}
        <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0, minHeight: 0 }}>
          <EditorArea state={state} dispatch={dispatch} runScript={runScript} stopScript={stopScript} />

          {panelOpen && (
            <div className="resize-handle-h"
              onMouseDown={() => { resizeRef.current.panel = true; }} />
          )}

          <div className={`bottom-panel${panelOpen ? " open" : ""}`}>
            <div className="panel-tabs">
              {PANEL_TABS.map(t => (
                <div key={t.id} className={`panel-tab${panelTab === t.id ? " active" : ""}`}
                  onClick={() => { if (!panelOpen) setPanelOpen(true); setPanelTab(t.id); }}>
                  {t.label}
                </div>
              ))}
              <div className="panel-actions">
                <button className="sidebar-btn" onClick={() => setPanelOpen(false)}>
                  <Icons.Close />
                </button>
              </div>
            </div>
            <div className={`panel-body${panelOpen ? " active" : ""}`}>
              {panelTab === "problems" && <div style={{ padding: 12, fontSize: 12, color: "var(--vscode-descriptionForeground)" }}>{'\u2713 \u65E0\u95EE\u9898\u68C0\u6D4B\u5230'}</div>}
              {panelTab === "output" && <div style={{ padding: 12, fontSize: 12, color: "var(--vscode-descriptionForeground)", fontFamily: "var(--font-mono)", whiteSpace: "pre-wrap" }}>{'\u5F15\u64CE\u8F93\u51FA\u5C06\u663E\u793A\u5728\u8FD9\u91CC...'}</div>}
              {panelTab === "debug" && <LogPanel />}
              {panelTab === "ai" && <AIPanel />}
            </div>
          </div>
        </div>

        {/* Right Auxiliary Bar */}
        <div className="aux-bar">
          <PropertyPanel />
        </div>
      </div>

      {/* Status Bar */}
      <StatusBar state={state} onPanelToggle={() => setPanelOpen(!panelOpen)} />

      {/* Overlays */}
      <SaveManager show={showSaveManager} onClose={() => setShowSaveManager(false)} />
      {showCommandPalette && <CommandPalette onClose={() => setShowCommandPalette(false)} dispatch={dispatch} state={state} />}
      <PackagePanel show={showPackagePanel} onClose={() => setShowPackagePanel(false)} onBuild={handlePackageBuild} />
      <ToastContainer toasts={toasts} />
      {state.packageResult && (
        <div className="toast-container">
          <div className="toast" onClick={() => dispatch({ type: "SET_PACKAGE_RESULT", payload: null })}>
            {state.packageResult.error
              ? <span style={{ color: "var(--vscode-errorForeground)" }}>{'\u2715 ' + state.packageResult.error}</span>
              : <span>{'\u2713 \u6253\u5305\u5B8C\u6210 \u2192 ' + (state.packageResult.path || "release/")}</span>}
          </div>
        </div>
      )}
    </div>
  );
}

export default function App() {
  return <EditorProvider><AppInner /></EditorProvider>;
}