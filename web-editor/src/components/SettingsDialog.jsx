import { useEditor } from "../context/EditorContext";
import { useState } from "react";

const SETTINGS_TABS = [
  { id: "ai", label: "AI 配置" },
  { id: "editor", label: "编辑器" },
  { id: "engine", label: "引擎" },
  { id: "package", label: "打包" },
  { id: "about", label: "关于" },
];

export default function SettingsDialog({ onClose }) {
  const { state, dispatch } = useEditor();
  const [tab, setTab] = useState(state.settingsTab || "ai");

  // Local state for editable settings (draft until Apply/OK)
  const [draft, setDraft] = useState({
    aiProvider: state.aiProvider,
    aiSettings: { ...state.aiSettings },
    resolution: { ...state.resolution },
    live2dEnabled: state.live2dEnabled,
  });

  const updateDraft = (section, field, val) => {
    if (section === "aiSettings") {
      setDraft(prev => ({ ...prev, aiSettings: { ...prev.aiSettings, [field]: val } }));
    } else {
      setDraft(prev => ({ ...prev, [field]: val }));
    }
  };

  const handleApply = () => {
    dispatch({ type: "SET_AI_PROVIDER", payload: draft.aiProvider });
    dispatch({ type: "SET_AI_SETTINGS", payload: draft.aiSettings });
    dispatch({ type: "SET_RESOLUTION", payload: draft.resolution });
    dispatch({ type: "SET_LIVE2D_ENABLED", payload: draft.live2dEnabled });
    try {
      localStorage.setItem("caesura-ai-settings", JSON.stringify(draft.aiSettings));
      localStorage.setItem("caesura-ai-provider", draft.aiProvider);
    } catch {}
    dispatch({ type: "ADD_LOG", payload: { level: "info", source: "settings", message: "Settings applied" } });
  };

  const handleOK = () => {
    handleApply();
    onClose();
  };

  const handleCancel = () => {
    onClose();
  };

  return (
    <div style={{
      position: "fixed", inset: 0, zIndex: 200,
      background: "var(--bg-root)", display: "flex", flexDirection: "column",
    }}>
      {/* Settings header */}
      <div style={{
        display: "flex", alignItems: "center", justifyContent: "space-between",
        padding: "8px 16px", background: "var(--bg-primary)", borderBottom: "1px solid var(--border)",
      }}>
        <div style={{ display: "flex", alignItems: "center", gap: 12 }}>
          <span style={{ fontSize: 16, fontWeight: 700, color: "var(--fg)" }}>⚙ 设置</span>
          <div className="settings-tabs" style={{ marginBottom: 0 }}>
            {SETTINGS_TABS.map(t => (
              <button key={t.id}
                className={`settings-tab${tab === t.id ? " active" : ""}`}
                onClick={() => { dispatch({ type: 'SET_SETTINGS_TAB', payload: t.id }); setTab(t.id); }}
              >{t.label}</button>
            ))}
          </div>
        </div>
        <button className="btn btn-sm" onClick={handleCancel} style={{ fontSize: 16 }}>✕</button>
      </div>

      {/* Settings body */}
      <div style={{ flex: 1, overflowY: "auto", padding: "24px 32px" }}>
        {tab === "ai" && (
          <div style={{ maxWidth: 560 }}>
            <div className="settings-group">
              <h3>AI Provider</h3>
              <div className="settings-field">
                <label>Provider</label>
                <select value={draft.aiProvider} onChange={e => updateDraft(null, "aiProvider", e.target.value)}>
                  <option value="openai">OpenAI</option>
                  <option value="codex">Codex</option>
                  <option value="custom">Custom</option>
                </select>
              </div>
            </div>
            <div className="settings-group">
              <h3>API Key</h3>
              <div className="settings-field">
                <label>API Key</label>
                <input type="password" value={draft.aiSettings.openaiKey}
                  onChange={e => updateDraft("aiSettings", "openaiKey", e.target.value)} placeholder="sk-..." />
              </div>
            </div>
            <div className="settings-group">
              <h3>Model</h3>
              <div className="settings-field">
                <label>Model</label>
                <input value={draft.aiSettings.openaiModel}
                  onChange={e => updateDraft("aiSettings", "openaiModel", e.target.value)} placeholder="gpt-4o" />
              </div>
            </div>
            <div className="settings-group">
              <h3>Custom URL</h3>
              <div className="settings-field">
                <label>URL</label>
                <input value={draft.aiSettings.customUrl}
                  onChange={e => updateDraft("aiSettings", "customUrl", e.target.value)} placeholder="https://api.example.com/v1" />
              </div>
            </div>
          </div>
        )}

        {tab === "editor" && (
          <div style={{ maxWidth: 560 }}>
            <div className="settings-group">
              <h3>编辑器设置</h3>
              <div className="settings-field">
                <label>字体大小</label>
                <input type="number" defaultValue={13} min={10} max={24} />
              </div>
              <div className="settings-field">
                <label>自动保存 (秒)</label>
                <input type="number" defaultValue={30} min={5} max={300} />
              </div>
              <div className="settings-field">
                <label>舞台分辨率</label>
                <select defaultValue="1280×720">
                  <option>1280×720</option>
                  <option>1920×1080</option>
                </select>
              </div>
            </div>
          </div>
        )}

        {tab === "engine" && (
          <div style={{ maxWidth: 560 }}>
            <div className="settings-group">
              <h3>引擎设置</h3>
              <div className="settings-field">
                <label>分辨率</label>
                <select value={`${draft.resolution.w}×${draft.resolution.h}`}
                  onChange={e => {
                    const [w, h] = e.target.value.split("×").map(Number);
                    updateDraft(null, "resolution", { w, h });
                  }}>
                  <option>1280×720</option>
                  <option>1920×1080</option>
                </select>
              </div>
              <div className="settings-field">
                <label>全屏</label>
                <div className="toggle-switch" />
              </div>
              <div className="settings-field">
                <label>Live2D</label>
                <div className={`toggle-switch${draft.live2dEnabled ? " on" : ""}`}
                  onClick={() => updateDraft(null, "live2dEnabled", !draft.live2dEnabled)} />
              </div>
              <div className="settings-field">
                <label>FFmpeg</label>
                <div className="toggle-switch on" />
              </div>
            </div>
            <div className="settings-group">
              <h3>音量</h3>
              <div className="settings-field"><label>BGM</label><input type="range" className="prop-slider" defaultValue={1} min={0} max={1} step={0.05} /></div>
              <div className="settings-field"><label>语音</label><input type="range" className="prop-slider" defaultValue={1} min={0} max={1} step={0.05} /></div>
              <div className="settings-field"><label>音效</label><input type="range" className="prop-slider" defaultValue={1} min={0} max={1} step={0.05} /></div>
            </div>
          </div>
        )}

        {tab === "package" && (
          <div style={{ maxWidth: 560 }}>
            <div className="settings-group">
              <h3>CARC 打包</h3>
              <div className="settings-field"><label>输出路径</label><input placeholder="./dist/" /></div>
              <div className="settings-field"><label>签名密钥</label><input type="password" placeholder="ed25519 private key" /></div>
              <button className="btn btn-accent btn-sm mt-2">打包 CARC</button>
            </div>
          </div>
        )}

        {tab === "about" && (
          <div className="settings-group">
            <h3>Caesura (AmeKAG)</h3>
            <p style={{ fontSize: 13, color: "var(--fg-dim)", lineHeight: 2 }}>
              次世代 Visual Novel 引擎<br />
              版本 Beta (~93%)<br />
              MIT License<br /><br />
              <a href="https://github.com/ailiasdesu/Caesura-AmeKAG" style={{ color: "var(--accent-bright)" }}>
                github.com/ailiasdesu/Caesura-AmeKAG
              </a>
            </p>
          </div>
        )}
      </div>

      {/* Settings footer */}
      <div style={{
        display: "flex", alignItems: "center", justifyContent: "flex-end", gap: 8,
        padding: "10px 24px", background: "var(--bg-primary)", borderTop: "1px solid var(--border)",
      }}>
        <button className="btn btn-sm" onClick={handleApply}>应用</button>
        <button className="btn btn-sm" onClick={handleCancel}>取消</button>
        <button className="btn btn-accent" onClick={handleOK}>确定</button>
      </div>
    </div>
  );
}
