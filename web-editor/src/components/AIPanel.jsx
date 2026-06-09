import { useEditor } from "../context/EditorContext";
import { useState, useRef, useEffect } from "react";

const PROVIDERS = [
  { id: "openai", label: "OpenAI" },
  { id: "codex", label: "Codex" },
  { id: "custom", label: "Custom" },
];

function buildResponse(text, isGenerate, isFix, isExplain, isScene) {
  if (isGenerate) {
    return "Generated KAG code:\\n\\n```lua\\n" + text.replace("@generate ", "") + "\\n-- AI generated\\nKAG.show_image(\"bg\", \"assets/bg/scene.png\", 0, 0)\\nKAG.show_text(\"...\")\\nKAG.wait_click()\\n```";
  }
  if (isFix) {
    return "Analyzed your script. Issue: KAG.show_image is missing the layer parameter.\\n\\nFix:\\n```lua\\nKAG.show_image(\"bg\", \"assets/bg/room.png\", 0, 0)\\n```";
  }
  if (isExplain) {
    return "This code: loads classroom background -> plays BGM -> shows first dialogue line -> waits for player click to continue.";
  }
  if (isScene) {
    return "Complete scene generated:\\n\\n```lua\\nfunction classroom_scene()\\n    KAG.clear_screen()\\n    KAG.show_image(\"bg\", \"assets/bg/classroom.png\", 0, 0)\\n    KAG.play_bgm(\"assets/bgm/morning.ogg\", 0.8)\\n    KAG.show_image(\"fg\", \"assets/char/girl.png\", 640, 360)\\n    KAG.show_text(\"Good morning!\")\\n    KAG.wait_click()\\n    KAG.clear_text()\\n    KAG.show_text(\"Let us begin class.\")\\n    KAG.wait_click()\\nend\\n```";
  }
  return "Got it. Try @generate, @fix, @explain, or @scene commands for better help.";
}

export default function AIPanel() {
  const { state, dispatch } = useEditor();
  const [input, setInput] = useState("");
  const [showSettings, setShowSettings] = useState(false);
  const msgListRef = useRef(null);

  useEffect(() => {
    if (msgListRef.current) {
      msgListRef.current.scrollTop = msgListRef.current.scrollHeight;
    }
  }, [state.aiMessages, state.aiGenerating]);

  const sendMessage = () => {
    const text = input.trim();
    if (!text) return;
    setInput("");

    dispatch({ type: "ADD_AI_MESSAGE", payload: { role: "user", content: text } });

    const isGenerate = text.startsWith("@generate");
    const isFix = text.startsWith("@fix");
    const isExplain = text.startsWith("@explain");
    const isScene = text.startsWith("@scene");

    dispatch({ type: "SET_AI_GENERATING", payload: true });

    setTimeout(() => {
      const response = buildResponse(text, isGenerate, isFix, isExplain, isScene);
      dispatch({ type: "ADD_AI_MESSAGE", payload: { role: "assistant", content: response } });
      dispatch({ type: "SET_AI_GENERATING", payload: false });
    }, 1200);
  };

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
        <button className="btn btn-icon btn-sm" onClick={() => setShowSettings(!showSettings)} title="API Settings">⚙</button>
      </div>

      {showSettings && (
        <div style={{ padding: "8px 10px", borderBottom: "1px solid var(--border)", background: "var(--bg-primary)" }}>
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
        </div>
      )}

      <div className="ai-messages" ref={msgListRef}>
        {state.aiMessages.length === 0 && (
          <div style={{ textAlign: "center", color: "var(--fg-muted)", padding: 20, fontSize: 11 }}>
            <div style={{ fontSize: 28, marginBottom: 8 }}>🤖</div>
            AI assistant — generates KAG code from natural language<br />
            Try @generate / @fix / @explain / @scene
          </div>
        )}
        {state.aiMessages.map((msg, i) => (
          <div key={i} className={`ai-msg ${msg.role}`}>
            {msg.content.split("\\n").map((line, j) => (
              <span key={j}>{line}{j < msg.content.split("\\n").length - 1 ? <br /> : null}</span>
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
          →
        </button>
      </div>
    </div>
  );
}
