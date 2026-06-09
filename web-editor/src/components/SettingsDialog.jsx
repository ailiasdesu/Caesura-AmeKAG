import { useEditor } from "../context/EditorContext";

export default function SettingsDialog() {
  const { state, dispatch } = useEditor();

  const updateSetting = (key, value) => {
    dispatch({ type: "SET_AI_SETTINGS", payload: { [key]: value } });
  };

  const save = () => {
    try {
      localStorage.setItem("caesura-ai-settings", JSON.stringify(state.aiSettings));
      localStorage.setItem("caesura-ai-provider", state.aiProvider);
      dispatch({ type: "ADD_AI_MESSAGE", payload: { role: "system", content: "⚙ Settings saved." } });
    } catch (e) {
      dispatch({ type: "ADD_AI_MESSAGE", payload: { role: "system", content: "Failed to save settings." } });
    }
  };

  const close = () => {
    dispatch({ type: "SET_ACTIVE_PANEL", payload: "editor" });
  };

  return (
    <div className="settings-overlay" onClick={(e) => { if (e.target === e.currentTarget) close(); }}>
      <div className="settings-dialog">
        <h3>AI Settings</h3>
        <div className="settings-row">
          <label>Provider</label>
          <select value={state.aiProvider} onChange={(e) => dispatch({ type: "SET_AI_PROVIDER", payload: e.target.value })}>
            <option value="openai">OpenAI API</option>
            <option value="codex">Codex (Local)</option>
            <option value="custom">Custom Endpoint</option>
          </select>
        </div>

        {state.aiProvider === "openai" && (
          <>
            <div className="settings-row">
              <label>API Key</label>
              <input type="password" placeholder="sk-..." value={state.aiSettings.openaiKey}
                onChange={(e) => updateSetting("openaiKey", e.target.value)} />
            </div>
            <div className="settings-row">
              <label>Model</label>
              <select value={state.aiSettings.openaiModel} onChange={(e) => updateSetting("openaiModel", e.target.value)}>
                <option value="gpt-4o">GPT-4o</option>
                <option value="gpt-4o-mini">GPT-4o Mini</option>
                <option value="gpt-4-turbo">GPT-4 Turbo</option>
              </select>
            </div>
          </>
        )}

        {state.aiProvider === "codex" && (
          <div className="settings-row">
            <label>Codex Endpoint</label>
            <input placeholder="http://localhost:PORT" value={state.aiSettings.codexEndpoint}
              onChange={(e) => updateSetting("codexEndpoint", e.target.value)} />
          </div>
        )}

        {state.aiProvider === "custom" && (
          <>
            <div className="settings-row">
              <label>API URL</label>
              <input placeholder="https://your-api.com/v1/chat/completions" value={state.aiSettings.customUrl}
                onChange={(e) => updateSetting("customUrl", e.target.value)} />
            </div>
            <div className="settings-row">
              <label>API Key</label>
              <input type="password" placeholder="Optional" value={state.aiSettings.customKey}
                onChange={(e) => updateSetting("customKey", e.target.value)} />
            </div>
          </>
        )}

        <div className="settings-row">
          <label>Temperature (0-2)</label>
          <input type="number" step="0.1" min="0" max="2" value={state.aiSettings.temperature || 0.7}
            onChange={(e) => updateSetting("temperature", parseFloat(e.target.value) || 0.7)} />
        </div>

        <div className="settings-actions">
          <button className="btn btn-gray" onClick={close}>Cancel</button>
          <button className="btn btn-green" onClick={save}>Save</button>
        </div>
      </div>
    </div>
  );
}