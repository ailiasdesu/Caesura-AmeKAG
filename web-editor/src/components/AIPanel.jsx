import { useEditor } from "../context/EditorContext";
import { useState, useRef, useEffect } from "react";

const PROVIDERS = [
  { id: "openai", label: "OpenAI" },
  { id: "codex", label: "Codex" },
  { id: "custom", label: "Custom" },
];

const KAG_SYSTEM_PROMPT = `You are an expert KAG (Kirikiri Adventure Game) scripting assistant for the Caesura (AmeKAG) visual novel engine. You generate Lua code that controls visual novel scenes.

## Engine Capabilities (KAG API)
- KAG.clear_screen() — clear all layers
- KAG.show_image(layer, path, x, y) — show image on layer (bg/fg/char)
- KAG.show_text(speaker, text) — display dialogue text
- KAG.wait_click() — wait for player click/input
- KAG.clear_text() — clear text box
- KAG.play_bgm(path, volume) — play background music
- KAG.stop_bgm(fade) — stop BGM with fade
- KAG.play_se(path) — play sound effect
- KAG.play_voice(path) — play voice line
- KAG.transition(type, duration) — screen transition (fade_in/fade_out/dissolve)
- KAG.live2d_load(id, modelPath) — load Live2D model
- KAG.live2d_set_animation(id, anim, loop) — play Live2D animation
- KAG.live2d_set_expression(id, expr) — set Live2D expression
- KAG.live2d_remove(id) — remove Live2D model
- KAG.play_video(path, opts) — play video with loop/volume options
- KAG.wait_video_end() — wait for video completion
- KAG.save(slot) / KAG.load(slot) — save/load game state
- mini_game API — 3D minigame objects (spawn_cube, spawn_sphere, set_camera, etc.)

## Code Style
- All functions use KAG.* prefix
- Scene functions: function scene_name() ... end
- Always wait for player input after dialogue
- Use clear_screen() before scene transitions

Respond with clean, runnable Lua code. Use @generate tag for new scenes.`;

export default function AIPanel() {
  const { state, dispatch } = useEditor();
  const [input, setInput] = useState("");
  const [showSettings, setShowSettings] = useState(false);
  const [error, setError] = useState(null);
  const msgListRef = useRef(null);

  useEffect(() => {
    if (msgListRef.current) {
      msgListRef.current.scrollTop = msgListRef.current.scrollHeight;
    }
  }, [state.aiMessages, state.aiGenerating]);

  const sendMessage = async () => {
    const text = input.trim();
    if (!text) return;
    setInput("");
    setError(null);

    dispatch({ type: "ADD_AI_MESSAGE", payload: { role: "user", content: text } });
    dispatch({ type: "SET_AI_GENERATING", payload: true });

    try {
      const messages = [
        { role: "system", content: KAG_SYSTEM_PROMPT },
        ...state.aiMessages.slice(-6).map((m) => ({ role: m.role, content: m.content })),
        { role: "user", content: text },
      ];

      // Check if running in Electron (window.caesura available) or standalone browser
      if (window.caesura && window.caesura.aiChat) {
        // Real Electron IPC bridge
        const result = await window.caesura.aiChat({
          messages,
          provider: state.aiProvider,
          settings: state.aiSettings,
        });

        if (result.error) {
          throw new Error(result.error);
        }

        dispatch({
          type: "ADD_AI_MESSAGE",
          payload: { role: "assistant", content: result.content || "(empty response)" },
        });
      } else {
        // Fallback: direct fetch for standalone browser dev mode
        const response = await callDirectApi(messages, state.aiProvider, state.aiSettings);
        dispatch({
          type: "ADD_AI_MESSAGE",
          payload: { role: "assistant", content: response },
        });
      }
    } catch (e) {
      setError(e.message);
      dispatch({
        type: "ADD_AI_MESSAGE",
        payload: {
          role: "assistant",
          content: "Error: " + (e.message || "Unknown error") + "\n\nCheck your API key and network connection in Settings.",
        },
      });
    } finally {
      dispatch({ type: "SET_AI_GENERATING", payload: false });
    }
  };

  async function callDirectApi(messages, provider, settings) {
    if (provider === "openai") {
      const key = settings.openaiKey || "";
      if (!key) throw new Error("OpenAI API key not configured.");
      const resp = await fetch("https://api.openai.com/v1/chat/completions", {
        method: "POST",
        headers: { "Content-Type": "application/json", Authorization: "Bearer " + key },
        body: JSON.stringify({ model: settings.openaiModel || "gpt-4o", messages, temperature: 0.7, max_tokens: 2048 }),
      });
      if (!resp.ok) {
        const err = await resp.json().catch(() => ({}));
        throw new Error(err.error?.message || "HTTP " + resp.status);
      }
      const data = await resp.json();
      return data.choices?.[0]?.message?.content || "";
    }
    if (provider === "custom") {
      const url = settings.customUrl || "";
      if (!url) throw new Error("Custom endpoint URL not configured.");
      const resp = await fetch(url, {
        method: "POST",
        headers: { "Content-Type": "application/json", Authorization: "Bearer " + (settings.customKey || "") },
        body: JSON.stringify({ model: settings.customModel || "default", messages, temperature: 0.7, max_tokens: 2048 }),
      });
      if (!resp.ok) throw new Error("HTTP " + resp.status);
      const data = await resp.json();
      return data.choices?.[0]?.message?.content || "";
    }
    throw new Error("Direct API not available for provider: " + provider + ". Use Electron for full support.");
  }

  return (
    <div className="ai-panel">
      <div className="ai-provider-bar">
        <select value={state.aiProvider}
          onChange={e => {
            dispatch({ type: "SET_AI_PROVIDER", payload: e.target.value });
            localStorage.setItem("caesura-ai-provider", e.target.value);
          }}
        >
          {PROVIDERS.map(p => <option key={p.id} value={p.id}>{p.label}</option>)}
        </select>
        <span className="spacer" />
        {error && <span style={{ color: "var(--error)", fontSize: 10, marginRight: 8 }}>{error.slice(0, 40)}</span>}
        <button className="btn btn-icon btn-sm" onClick={() => setShowSettings(!showSettings)} title="API Settings">&#9881;</button>
      </div>

      {showSettings && (
        <div style={{ padding: "8px 10px", borderBottom: "1px solid var(--border)", background: "var(--bg-primary)" }}>
          {state.aiProvider === "openai" && (
            <>
              <div className="prop-row" style={{ marginBottom: 4 }}>
                <label style={{ width: 50 }}>Key</label>
                <input type="password" value={state.aiSettings.openaiKey}
                  onChange={e => dispatch({ type: "SET_AI_SETTINGS", payload: { openaiKey: e.target.value } })}
                  placeholder="sk-..." />
              </div>
              <div className="prop-row">
                <label style={{ width: 50 }}>Model</label>
                <input value={state.aiSettings.openaiModel}
                  onChange={e => dispatch({ type: "SET_AI_SETTINGS", payload: { openaiModel: e.target.value } })} />
              </div>
            </>
          )}
          {state.aiProvider === "custom" && (
            <>
              <div className="prop-row" style={{ marginBottom: 4 }}>
                <label style={{ width: 50 }}>URL</label>
                <input value={state.aiSettings.customUrl}
                  onChange={e => dispatch({ type: "SET_AI_SETTINGS", payload: { customUrl: e.target.value } })}
                  placeholder="https://..." />
              </div>
              <div className="prop-row" style={{ marginBottom: 4 }}>
                <label style={{ width: 50 }}>Key</label>
                <input type="password" value={state.aiSettings.customKey}
                  onChange={e => dispatch({ type: "SET_AI_SETTINGS", payload: { customKey: e.target.value } })} />
              </div>
              <div className="prop-row">
                <label style={{ width: 50 }}>Model</label>
                <input value={state.aiSettings.customModel}
                  onChange={e => dispatch({ type: "SET_AI_SETTINGS", payload: { customModel: e.target.value } })} />
              </div>
            </>
          )}
          {state.aiProvider === "codex" && (
            <div style={{ fontSize: 11, color: "var(--fg-muted)", padding: 4 }}>
              Codex provider uses local agent bridge. Open project in Codex IDE for AI-assisted scripting.
            </div>
          )}
        </div>
      )}

      <div className="ai-messages" ref={msgListRef}>
        {state.aiMessages.length === 0 && (
          <div style={{ textAlign: "center", color: "var(--fg-muted)", padding: 20, fontSize: 11 }}>
            <div style={{ fontSize: 28, marginBottom: 8 }}>&#129302;</div>
            AI assistant - generates KAG code from natural language<br />
            Try @generate / @fix / @explain / @scene
          </div>
        )}
        {state.aiMessages.map((msg, i) => (
          <div key={i} className={"ai-msg " + msg.role}>
            {msg.content.split("\n").map((line, j) => (
              <span key={j}>{line}{j < msg.content.split("\n").length - 1 ? <br /> : null}</span>
            ))}
          </div>
        ))}
        {state.aiGenerating && (
          <div className="ai-msg assistant" style={{ display: "flex", gap: 6, alignItems: "center" }}>
            <div className="skeleton" style={{ width: 80, height: 12 }} />
            <div className="skeleton" style={{ width: 120, height: 12 }} />
          </div>
        )}
      </div>

      <div className="ai-quick-cmds">
        {["@generate", "@fix", "@explain", "@scene"].map(cmd => (
          <button key={cmd} className="ai-quick-cmd"
            onClick={() => { setInput(cmd + " "); }}
          >{cmd}</button>
        ))}
      </div>

      <div className="ai-input-row">
        <textarea
          value={input}
          onChange={e => setInput(e.target.value)}
          onKeyDown={e => {
            if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); sendMessage(); }
          }}
          placeholder="Describe what you want in natural language..."
          disabled={state.aiGenerating}
        />
        <button className="btn btn-accent" onClick={sendMessage} disabled={state.aiGenerating || !input.trim()}>
          &rarr;
        </button>
      </div>
    </div>
  );
}
