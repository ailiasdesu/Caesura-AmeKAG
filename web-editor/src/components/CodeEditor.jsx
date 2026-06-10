import { useEffect, useRef } from "react";
import { useEditor } from "../context/EditorContext";
import { basicSetup } from "codemirror";
import { EditorView, keymap } from "@codemirror/view";
import { lua } from "@codemirror/legacy-modes/mode/lua";
import { StreamLanguage } from "@codemirror/language";
import { oneDark } from "@codemirror/theme-one-dark";
import { autocompletion } from "@codemirror/autocomplete";

const KAG_COMMANDS = [
  "layer_show", "layer_hide", "layer_move", "text_show", "text_clear", "text_speed",
  "audio_bgm", "audio_se", "audio_voice", "audio_stop", "transition_fade", "transition_dissolve",
  "system_wait", "system_jump", "system_choice", "video_play", "video_stop", "vfx_shake",
];

export default function CodeEditor() {
  const { state, dispatch } = useEditor();
  const editorRef = useRef(null);
  const viewRef = useRef(null);
  const file = state.openFiles?.[state.activeFileIdx];
  const content = file?.content || "-- Caesura KAG Script\n";

  useEffect(() => {
    if (!editorRef.current || viewRef.current) return;

    const kagCompletion = autocompletion({
      override: [async (ctx) => {
        const word = ctx.matchBefore(/KAG\.(\w*)/);
        if (!word) return null;
        return {
          from: word.from + 4,
          options: KAG_COMMANDS.map(cmd => ({ label: cmd, type: "function" })),
        };
      }],
    });

    try {
      viewRef.current = new EditorView({
        doc: content,
        extensions: [
          basicSetup,
          StreamLanguage.define(lua),
          oneDark,
          kagCompletion,
          keymap.of([{ key: "Ctrl-s", run: () => true }]),
          EditorView.updateListener.of((update) => {
            if (update.docChanged) {
              dispatch({ type: "UPDATE_FILE", payload: update.state.doc.toString() });
            }
          }),
        ],
        parent: editorRef.current,
      });
    } catch (err) {
      console.error("CodeMirror init failed:", err);
    }

    return () => { viewRef.current?.destroy(); viewRef.current = null; };
  }, []);

  // Update content when file changes
  useEffect(() => {
    if (viewRef.current && file) {
      const current = viewRef.current.state.doc.toString();
      if (current !== file.content) {
        viewRef.current.dispatch({
          changes: { from: 0, to: current.length, insert: file.content || "" },
        });
      }
    }
  }, [file?.content]);

  return (
    <div className="code-editor-wrap">
      <div className="code-file-tabs">
        {state.openFiles?.map((f, i) => (
          <div key={i} className={`code-file-tab${i === state.activeFileIdx ? " active" : ""}`}
            onClick={() => dispatch({ type: "SET_ACTIVE_FILE", payload: i })}>
            {f.name}
            {state.unsaved && i === state.activeFileIdx && '\u25CF'}
            <span className="tab-close" onClick={(e) => { e.stopPropagation(); dispatch({ type: "CLOSE_FILE", payload: i }); }}>{'\u00D7'}</span>
          </div>
        ))}
      </div>
      <div ref={editorRef} style={{ flex: 1, overflow: "auto" }} />
      <div className="code-statusbar">
        <span>Ln 1, Col 1</span>
        <span>Spaces: 2</span>
        <span>Lua</span>
        <span>UTF-8</span>
      </div>
    </div>
  );
}
