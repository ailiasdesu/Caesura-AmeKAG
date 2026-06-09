import { useEditor } from "../context/EditorContext";

const SETTINGS_TABS = [
  { id: "ai", label: "AI 配置" },
  { id: "editor", label: "编辑器" },
  { id: "engine", label: "引擎" },
  { id: "package", label: "打包" },
  { id: "about", label: "关于" },
];

export default function SettingsDialog() {
  const { state, dispatch } = useEditor();

  const updateAi = (field, val) => {
    dispatch({ type: "SET_AI_SETTINGS", payload: { [field]: val } });
    try {
      const current = JSON.parse(localStorage.getItem("caesura-ai-settings") || "{}");
      localStorage.setItem("caesura-ai-settings", JSON.stringify({ ...current, [field]: val }));
    } catch {}
  };

  const tab = state.settingsTab || "ai";

  return (
    <div className="settings-panel">
      <div className="settings-tabs">
        {SETTINGS_TABS.map(t => (
          <button key={t.id}
            className={`settings-tab${tab === t.id ? " active" : ""}`}
            onClick={() => dispatch({ type: "SET_SETTINGS_TAB", payload: t.id })}
          >{t.label}</button>
        ))}
      </div>

      {tab === "ai" && (
        <div>
          <div className="settings-group">
            <h3>AI Provider</h3>
            <div className="settings-field">
              <label>Provider</label>
              <select value={state.aiProvider}
                onChange={e => {
                  dispatch({ type: "SET_AI_PROVIDER", payload: e.target.value });
                  localStorage.setItem("caesura-ai-provider", e.target.value);
                }}>
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
              <input type="password" value={state.aiSettings.openaiKey}
                onChange={e => updateAi("openaiKey", e.target.value)} placeholder="sk-..." />
            </div>
          </div>
          <div className="settings-group">
            <h3>Model</h3>
            <div className="settings-field">
              <label>Model</label>
              <input value={state.aiSettings.openaiModel}
                onChange={e => updateAi("openaiModel", e.target.value)} placeholder="gpt-4o" />
            </div>
          </div>
          <div className="settings-group">
            <h3>Custom URL</h3>
            <div className="settings-field">
              <label>URL</label>
              <input value={state.aiSettings.customUrl}
                onChange={e => updateAi("customUrl", e.target.value)} placeholder="https://api.example.com/v1" />
            </div>
          </div>
        </div>
      )}

      {tab === "editor" && (
        <div>
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
        <div>
          <div className="settings-group">
            <h3>引擎设置</h3>
            <div className="settings-field">
              <label>分辨率</label>
              <select value={`${state.resolution.w}×${state.resolution.h}`}
                onChange={e => {
                  const [w, h] = e.target.value.split("×").map(Number);
                  dispatch({ type: "SET_RESOLUTION", payload: { w, h } });
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
              <div className={`toggle-switch${state.live2dEnabled ? " on" : ""}`}
                onClick={() => dispatch({ type: "SET_LIVE2D_ENABLED", payload: !state.live2dEnabled })} />
            </div>
            <div className="settings-field">
              <label>FFmpeg</label>
              <div className="toggle-switch on" />
            </div>
          </div>
          <div className="settings-group">
            <h3>音量</h3>
            <div className="settings-field">
              <label>BGM</label>
              <input type="range" className="prop-slider" defaultValue={1} min={0} max={1} step={0.05} />
            </div>
            <div className="settings-field">
              <label>语音</label>
              <input type="range" className="prop-slider" defaultValue={1} min={0} max={1} step={0.05} />
            </div>
            <div className="settings-field">
              <label>音效</label>
              <input type="range" className="prop-slider" defaultValue={1} min={0} max={1} step={0.05} />
            </div>
          </div>
        </div>
      )}

      {tab === "package" && (
        <div>
          <div className="settings-group">
            <h3>CARC 打包</h3>
            <div className="settings-field">
              <label>输出路径</label>
              <input placeholder="./dist/" />
            </div>
            <div className="settings-field">
              <label>签名密钥</label>
              <input type="password" placeholder="ed25519 private key" />
            </div>
            <button className="btn btn-accent btn-sm mt-2">打包 CARC</button>
          </div>
        </div>
      )}

      {tab === "about" && (
        <div>
          <div className="settings-group">
            <h3>Caesura (AmeKAG)</h3>
            <p style={{ fontSize: 12, color: "var(--fg-dim)", lineHeight: 1.8 }}>
              次世代 Visual Novel 引擎<br />
              版本 Beta (~93%)<br />
              MIT License<br /><br />
              <a href="https://github.com/ailiasdesu/Caesura-AmeKAG" style={{ color: "var(--accent-bright)" }}>
                github.com/ailiasdesu/Caesura-AmeKAG
              </a>
            </p>
          </div>
        </div>
      )}
    </div>
  );
}
