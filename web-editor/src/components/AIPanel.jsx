import { useState, useRef, useEffect } from "react";
import { useEditor } from "../context/EditorContext";

const KAG_API_SUMMARY = `KAG API Reference:
- KAG.show_image(layer, path, x, y, scale?, opacity?, blend?) — display image on bg/fg/msg
- KAG.show_text(text) — show text in message box
- KAG.clear_screen(layer?) — clear layer or all
- KAG.wait_click() — pause until user click
- KAG.play_bgm(path, fadeTime?) — play background music
- KAG.stop_bgm() — stop BGM
- KAG.play_voice(path) — play voice clip
- KAG.play_se(path) — play sound effect
- KAG.quake(intensity, duration?) — screen shake
- KAG.render("submit_transition", type, time) — scene transition (fade/crossfade)
- KAG.render("submit_vfx", ...) — visual effects
- KAG.set_font(id) — 0=Small, 1=Large, 2=TTF
- KAG.log(level, msg) — engine log
- mini_game.spawn_cube(x,y,z) — 3D cube
- mini_game.spawn_sphere(x,y,z,radius?)
- mini_game.spawn_plane(x,y,z,w,h?)
- mini_game.set_camera(eyeX,eyeY,eyeZ, atX,atY,atZ)
- Sandbox: no os.execute, no io.open, require only preloaded modules`;

export default function AIPanel() {
  const { state, dispatch } = useEditor();
  const [input, setInput] = useState("");
  const [loading, setLoading] = useState(false);
  const messagesEnd = useRef(null);

  useEffect(() => { messagesEnd.current?.scrollIntoView({ behavior: "smooth" }); }, [state.aiMessages]);

  const buildSystemPrompt = () => {
    const scriptPreview = state.script.substring(0, 4000);
    const totalChars = scriptPreview.length + KAG_API_SUMMARY.length + 500;
    if (totalChars > TOKEN_WARNING_THRESHOLD) {
      dispatch({ type: "ADD_AI_MESSAGE", payload: { role: "system", content: `⚠️ Context: ~${Math.round(totalChars/4)} tokens (budget warning). Consider shorter scripts or fewer conversation turns.` } });
    }

    return `You are an AI assistant for the Caesura (AmeKAG) visual novel engine. Help the user write KAG Lua scripts.

CURRENT SCRIPT:
\`\`\`lua
${scriptPreview}
\`\`\`

${KAG_API_SUMMARY}

RULES:
1. Only output valid KAG Lua code in code blocks.
2. All API access goes through KAG.* or mini_game.* globals.
3. Use \`\`\`lua code blocks for code.
4. Keep responses concise — focus on code, not explanations.
5. When user says @generate or describes a scene, output the full scene_start() function.
6. When user says @fix, analyze the error and output corrected code.`;
  };

  const sendMessage = async () => {
    if (!input.trim() || loading) return;
    const userMsg = { role: "user", content: input };
    dispatch({ type: "ADD_AI_MESSAGE", payload: userMsg });
    setInput("");
    setLoading(true);

    const isGenerate = input.includes("@generate");
    const isFix = input.includes("@fix");
    let fullPrompt = input;
    if (isGenerate) fullPrompt = `Generate a KAG scene script for: ${input.replace("@generate", "").trim()}. Use scene_start() function.`;
    if (isFix) fullPrompt = `Fix this KAG script error:\n\`\`\`lua\n${state.script.substring(0, 3000)}\n\`\`\`\nError context: ${input.replace("@fix", "").trim()}`;

    try {
      const response = await window.caesura.aiChat({
        messages: [
          { role: "system", content: buildSystemPrompt() },
          ...state.aiMessages.slice(-6).map((m) => ({ role: m.role, content: m.content })),
          { role: "user", content: fullPrompt },
        ],
        provider: state.aiProvider,
        settings: state.aiSettings,
      });

      dispatch({ type: "ADD_AI_MESSAGE", payload: { role: "assistant", content: response || "No response from AI." } });

      // Extract code blocks and offer insertion
      const codeMatch = response?.match(/```lua\n([\s\S]*?)```/);
      if (codeMatch && (isGenerate || isFix)) {
        dispatch({ type: "ADD_AI_MESSAGE", payload: { role: "system", content: "💡 Code block detected. Click below to insert into editor." } });
      }
    } catch (err) {
      dispatch({ type: "ADD_AI_MESSAGE", payload: { role: "system", content: `Error: ${err.message}` } });
    }
    setLoading(false);
  };

  const insertLastCode = () => {
    const lastMsg = [...state.aiMessages].reverse().find((m) => m.role === "assistant");
    if (!lastMsg) return;
    const codeMatch = lastMsg.content?.match(/```lua\n([\s\S]*?)```/);
    if (codeMatch) {
      const code = codeMatch[1];
      if (!code.includes("scene_start") && !code.includes("function") && !code.includes("KAG.") && !code.includes("mini_game.")) {
        dispatch({ type: "ADD_AI_MESSAGE", payload: { role: "system", content: "⚠️ Generated code may be invalid (no KAG/mini_game calls found). Please review before running." } });
      }
      dispatch({ type: "SET_SCRIPT", payload: code });
      dispatch({ type: "ADD_AI_MESSAGE", payload: { role: "system", content: "✅ Code inserted into editor." } });
    }
  };

  const handleKeyDown = (e) => {
    if (e.key === "Enter" && !e.shiftKey) {
      e.preventDefault();
      sendMessage();
    }
  };

  return (
    <div className="ai-panel" style={{ height: "100%" }}>
      <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "6px 8px", borderBottom: "1px solid #1a1a24" }}>
        <span style={{ fontSize: 11, fontWeight: 700, color: "#888" }}>AI Assistant</span>
        <select className="ai-provider-select" value={state.aiProvider}
          onChange={(e) => dispatch({ type: "SET_AI_PROVIDER", payload: e.target.value })}>
          <option value="openai">OpenAI</option>
          <option value="codex">Codex</option>
          <option value="custom">Custom</option>
        </select>
        <div className="spacer" />
        <button className="btn btn-sm btn-gray" onClick={() => dispatch({ type: "SET_ACTIVE_PANEL", payload: "settings" })}>⚙</button>
      </div>

      <div className="ai-messages">
        {state.aiMessages.length === 0 && (
          <div className="ai-msg system">
            👋 I'm your KAG scripting assistant.<br />
            Try <b>@generate a classroom scene</b> or <b>@fix</b> for errors.
          </div>
        )}
        {state.aiMessages.map((msg, i) => (
          <div key={i} className={`ai-msg ${msg.role}`}>
            {msg.content.split("\n").map((line, j) => {
              if (line.startsWith("```")) return <br key={j} />;
              if (line.startsWith("  ") || line.startsWith("\t")) return <code key={j}>{line}<br /></code>;
              return <span key={j}>{line}<br /></span>;
            })}
          </div>
        ))}
        {loading && <div className="ai-msg assistant">Thinking...</div>}
        <div ref={messagesEnd} />
      </div>

      <div className="ai-input-row">
        <input className="ai-input" placeholder={loading ? "Waiting..." : "Ask AI... (@generate / @fix)"}
          value={input} onChange={(e) => setInput(e.target.value)} onKeyDown={handleKeyDown} disabled={loading} />
        <button className="btn btn-blue btn-sm" onClick={sendMessage} disabled={loading || !input.trim()}>Send</button>
        <button className="btn btn-green btn-sm" onClick={insertLastCode} title="Insert last generated code into editor">Insert</button>
      </div>
    </div>
  );
}