import { useState, useRef, useEffect } from "react";

export default function AIPanel() {
  const [messages, setMessages] = useState([
    { role: "assistant", content: "Hello! I am Caesura AI assistant. Use @generate to create KAG code, or @fix to debug your script." }
  ]);
  const [input, setInput] = useState("");
  const [provider, setProvider] = useState("openai");
  const [loading, setLoading] = useState(false);
  const bodyRef = useRef(null);

  useEffect(() => {
    if (bodyRef.current) bodyRef.current.scrollTop = bodyRef.current.scrollHeight;
  }, [messages]);

  const send = async () => {
    const text = input.trim();
    if (!text) return;
    setInput("");
    setMessages(m => [...m, { role: "user", content: text }]);
    setLoading(true);

    // Simulate AI response
    setTimeout(() => {
      if (text.startsWith("@generate")) {
        setMessages(m => [...m, { role: "assistant", content: "```lua\n-- Generated KAG Script\nKAG.layer_show(\"bg\", \"classroom.png\")\nKAG.text_show(\"Hello, world!\")\nKAG.system_wait(2.0)\nKAG.layer_hide(\"bg\")\n```" }]);
      } else if (text.startsWith("@fix")) {
        setMessages(m => [...m, { role: "assistant", content: "I analyzed your script. Found: no syntax errors. The issue might be layer ordering \u2014 ensure background layers are loaded before character layers." }]);
      } else {
        setMessages(m => [...m, { role: "assistant", content: "I understand. You can use @generate to create KAG code from natural language, or @fix to debug existing scripts." }]);
      }
      setLoading(false);
    }, 800);
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
      <div style={{ display: "flex", gap: 4, padding: "4px 8px", borderBottom: "1px solid var(--vscode-sideBar-border)" }}>
        <select value={provider} onChange={(e) => setProvider(e.target.value)}
          style={{ padding: "2px 4px", border: "1px solid var(--vscode-input-border)", borderRadius: "var(--radius-sm)", background: "var(--vscode-input-background)", color: "var(--vscode-input-foreground)", fontSize: 11 }}>
          <option value="openai">OpenAI</option>
          <option value="codex">Codex</option>
          <option value="custom">Custom</option>
        </select>
        <button className="btn btn-sm" onClick={() => setInput("@generate ")}>@generate</button>
        <button className="btn btn-sm" onClick={() => setInput("@fix ")}>@fix</button>
      </div>
      <div ref={bodyRef} style={{ flex: 1, overflow: "auto", padding: 8, display: "flex", flexDirection: "column", gap: 8 }}>
        {messages.map((msg, i) => (
          <div key={i} style={{
            alignSelf: msg.role === "user" ? "flex-end" : "flex-start",
            maxWidth: "85%",
            padding: "6px 10px",
            borderRadius: "var(--radius-md)",
            background: msg.role === "user" ? "var(--accent-dim)" : "var(--vscode-editor-background-deep)",
            color: "var(--vscode-foreground)",
            fontSize: 11,
            whiteSpace: "pre-wrap",
            border: msg.role === "assistant" ? "1px solid var(--vscode-sideBar-border)" : "none",
          }}>
            {msg.content}
          </div>
        ))}
        {loading && (
          <div style={{ alignSelf: "flex-start", padding: "6px 10px", color: "var(--vscode-descriptionForeground)", fontSize: 11 }}>
            {'\u25CF\u25CB\u25CB'} thinking...
          </div>
        )}
      </div>
      <div style={{ display: "flex", gap: 4, padding: 8, borderTop: "1px solid var(--vscode-sideBar-border)" }}>
        <input value={input} onChange={(e) => setInput(e.target.value)}
          onKeyDown={(e) => { if (e.key === "Enter") send(); }}
          placeholder="Ask AI or use @generate / @fix..."
          style={{ flex: 1, padding: "4px 8px", border: "1px solid var(--vscode-input-border)", borderRadius: "var(--radius-sm)", background: "var(--vscode-input-background)", color: "var(--vscode-input-foreground)", fontSize: 11 }} />
        <button className="btn btn-accent btn-sm" onClick={send} disabled={loading}>{'\u2191'}</button>
      </div>
    </div>
  );
}