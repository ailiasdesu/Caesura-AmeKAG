import { useState, useEffect, useRef } from "react";
import { useEditor } from "../context/EditorContext";

export default function LogPanel() {
  const { state } = useEditor();
  const [filter, setFilter] = useState("all");
  const [search, setSearch] = useState("");
  const bodyRef = useRef(null);

  const logs = (state.logs || []).filter(l => {
    if (filter !== "all" && l.level !== filter) return false;
    if (search && !(l.message || "").toLowerCase().includes(search.toLowerCase())) return false;
    return true;
  });

  useEffect(() => {
    if (bodyRef.current) bodyRef.current.scrollTop = bodyRef.current.scrollHeight;
  }, [logs.length]);

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
      <div style={{ display: "flex", gap: 4, padding: "4px 8px", borderBottom: "1px solid var(--vscode-sideBar-border)" }}>
        {["all", "info", "warn", "error"].map(l => (
          <button key={l} className={`btn btn-sm${filter === l ? " btn-accent" : ""}`}
            onClick={() => setFilter(l)}>{l.toUpperCase()}</button>
        ))}
        <input placeholder="\u641C\u7D22..." value={search} onChange={(e) => setSearch(e.target.value)}
          style={{ marginLeft: "auto", padding: "2px 6px", border: "1px solid var(--vscode-input-border)", borderRadius: "var(--radius-sm)", background: "var(--vscode-input-background)", color: "var(--vscode-input-foreground)", fontSize: 11, width: 120 }} />
      </div>
      <div ref={bodyRef} style={{ flex: 1, overflow: "auto", padding: "4px 8px", font: "var(--text-xs) var(--font-mono)" }}>
        {logs.map((l, i) => (
          <div key={i} style={{
            padding: "1px 0",
            color: l.level === "error" ? "var(--vscode-errorForeground)" : l.level === "warn" ? "var(--vscode-editorWarning-foreground)" : "var(--vscode-descriptionForeground)"
          }}>
            <span style={{ color: "var(--fg-muted)", marginRight: 8 }}>{new Date().toLocaleTimeString()}</span>
            {l.message}
          </div>
        ))}
        {logs.length === 0 && <div style={{ color: "var(--vscode-descriptionForeground)", padding: 8 }}>{'\u65E0\u65E5\u5FD7'}</div>}
      </div>
    </div>
  );
}