import { useEditor } from "../context/EditorContext";
import { useRef, useEffect, useState } from "react";

const KAG_COMMANDS = [
  "clear_screen", "show_image", "show_text", "clear_text", "wait_click",
  "play_bgm", "stop_bgm", "fade_bgm", "play_voice", "play_se", "stop_se",
  "fade_in", "fade_out", "dissolve", "wipe_left", "wipe_right",
  "set_layer", "move_layer", "scale_layer", "rotate_layer", "set_opacity",
  "jump", "branch", "choice", "flag_set", "flag_check",
  "show_video", "pause_video", "stop_video", "seek_video",
  "delay", "shake", "flash", "snow", "rain",
];

export default function CodeEditor() {
  const { state, dispatch } = useEditor();
  const textareaRef = useRef(null);
  const [showAutocomplete, setShowAutocomplete] = useState(false);
  const [acMatches, setAcMatches] = useState([]);
  const [acIdx, setAcIdx] = useState(0);

  const activeFile = state.openFiles[state.activeFileIdx] || { name: "untitled.lua", content: "" };
  const lines = activeFile.content.split("\n");

  const handleKeyDown = (e) => {
    // Save: Ctrl+S
    if (e.ctrlKey && e.key === "s") {
      e.preventDefault();
      dispatch({ type: "MARK_SAVED" });
    }
    // Run: Ctrl+R
    if (e.ctrlKey && e.key === "r") {
      e.preventDefault();
      // handled by parent
    }

    // Autocomplete navigation
    if (showAutocomplete) {
      if (e.key === "ArrowDown") { e.preventDefault(); setAcIdx(i => Math.min(i + 1, acMatches.length - 1)); return; }
      if (e.key === "ArrowUp") { e.preventDefault(); setAcIdx(i => Math.max(i - 1, 0)); return; }
      if (e.key === "Enter" || e.key === "Tab") {
        e.preventDefault();
        insertAutocomplete(acMatches[acIdx]);
        return;
      }
      if (e.key === "Escape") { setShowAutocomplete(false); return; }
    }
  };

  const handleInput = (e) => {
    const val = e.target.value;
    dispatch({ type: "SET_SCRIPT", payload: val });

    // Autocomplete trigger: "KAG."
    const cursorPos = e.target.selectionStart;
    const textBefore = val.slice(0, cursorPos);
    const match = textBefore.match(/KAG\.(\w*)$/);
    if (match) {
      const partial = match[1].toLowerCase();
      const matches = KAG_COMMANDS.filter(c => c.startsWith(partial));
      if (matches.length > 0) {
        setAcMatches(matches);
        setAcIdx(0);
        setShowAutocomplete(true);
        return;
      }
    }
    setShowAutocomplete(false);
  };

  const insertAutocomplete = (cmd) => {
    const ta = textareaRef.current;
    if (!ta) return;
    const pos = ta.selectionStart;
    const before = state.script.slice(0, pos);
    const after = state.script.slice(pos);
    const newBefore = before.replace(/KAG\.\w*$/, `KAG.${cmd}`);
    const newScript = newBefore + after;
    dispatch({ type: "SET_SCRIPT", payload: newScript });
    setShowAutocomplete(false);
    setTimeout(() => {
      ta.focus();
      ta.selectionStart = ta.selectionEnd = newBefore.length;
    }, 0);
  };

  const closeFile = (idx) => {
    dispatch({ type: "CLOSE_FILE", payload: idx });
  };

  return (
    <div className="code-editor-wrap">
      {/* File tabs */}
      <div className="code-file-tabs">
        {state.openFiles.map((f, i) => (
          <div key={i}
            className={`code-file-tab${i === state.activeFileIdx ? " active" : ""}`}
            onClick={() => dispatch({ type: "SET_ACTIVE_FILE", payload: i })}
          >
            {f.name}
            <span className="close-tab" onClick={(e) => { e.stopPropagation(); closeFile(i); }}>×</span>
          </div>
        ))}
      </div>

      {/* Editor body */}
      <div className="code-editor-body" style={{ position: "relative" }}>
        <div className="code-gutter">
          {lines.map((_, i) => (
            <div key={i} style={{ paddingRight: 6 }}>{i + 1}</div>
          ))}
        </div>

        <textarea
          ref={textareaRef}
          className="code-textarea"
          value={activeFile.content}
          onChange={handleInput}
          onKeyDown={handleKeyDown}
          spellCheck={false}
          placeholder="-- Write KAG script here...&#10;function scene_start()&#10;    KAG.clear_screen()&#10;end"
        />

        {/* Autocomplete popup */}
        {showAutocomplete && (
          <div style={{
            position: "absolute", top: 24, left: 48, zIndex: 30,
            background: "var(--bg-primary)", border: "1px solid var(--border)",
            borderRadius: "var(--radius-md)", maxHeight: 160, overflowY: "auto",
            minWidth: 180, boxShadow: "var(--shadow-md)",
          }}>
            {acMatches.map((cmd, i) => (
              <div key={cmd}
                style={{
                  padding: "3px 10px", fontSize: 12, fontFamily: "var(--font-mono)",
                  cursor: "pointer", color: i === acIdx ? "var(--fg)" : "var(--fg-dim)",
                  background: i === acIdx ? "var(--accent-dim)" : "transparent",
                }}
                onClick={() => insertAutocomplete(cmd)}
                onMouseEnter={() => setAcIdx(i)}
              >
                KAG.{cmd}
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Status bar */}
      <div className="code-statusbar">
        <span className={`saved-dot ${state.unsaved ? "unsaved" : "saved"}`} />
        <span>{state.unsaved ? "未保存" : "已保存"}</span>
        <span className="spacer" />
        <span>行 {lines.length}</span>
        <span>Lua 5.4</span>
        <span>Ctrl+S 保存</span>
      </div>
    </div>
  );
}
