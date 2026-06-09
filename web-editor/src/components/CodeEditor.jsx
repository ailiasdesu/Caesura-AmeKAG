import { useEditor } from "../context/EditorContext";
import { useEffect, useRef, useCallback } from "react";
import { EditorState } from "@codemirror/state";
import { EditorView, keymap, lineNumbers, highlightActiveLine, highlightActiveLineGutter, drawSelection, rectangularSelection, crosshairCursor, highlightSpecialChars } from "@codemirror/view";
import { defaultKeymap, history, historyKeymap, indentWithTab } from "@codemirror/commands";
import { syntaxHighlighting, defaultHighlightStyle, bracketMatching, indentOnInput, foldGutter, foldKeymap } from "@codemirror/language";
import { autocompletion, completionKeymap, closeBrackets, closeBracketsKeymap } from "@codemirror/autocomplete";
import { lintKeymap } from "@codemirror/lint";
import { searchKeymap, highlightSelectionMatches } from "@codemirror/search";
import { oneDark } from "@codemirror/theme-one-dark";
import { lua } from "@codemirror/legacy-modes/mode/lua";
import { StreamLanguage } from "@codemirror/language";

const KAG_COMPLETIONS = [
  { label: "KAG.clear_screen", type: "function", detail: "清除屏幕" },
  { label: "KAG.show_image", type: "function", detail: "显示图像(layer, path, x, y)" },
  { label: "KAG.show_text", type: "function", detail: "显示文本(text)" },
  { label: "KAG.clear_text", type: "function", detail: "清除文本" },
  { label: "KAG.wait_click", type: "function", detail: "等待点击" },
  { label: "KAG.play_bgm", type: "function", detail: "播放BGM(path, volume)" },
  { label: "KAG.stop_bgm", type: "function", detail: "停止BGM" },
  { label: "KAG.fade_bgm", type: "function", detail: "淡出BGM(duration)" },
  { label: "KAG.play_voice", type: "function", detail: "播放语音(path)" },
  { label: "KAG.play_se", type: "function", detail: "播放音效(path)" },
  { label: "KAG.stop_se", type: "function", detail: "停止音效" },
  { label: "KAG.fade_in", type: "function", detail: "淡入(duration)" },
  { label: "KAG.fade_out", type: "function", detail: "淡出(duration)" },
  { label: "KAG.dissolve", type: "function", detail: "溶解过渡(duration)" },
  { label: "KAG.wipe_left", type: "function", detail: "左擦过渡" },
  { label: "KAG.wipe_right", type: "function", detail: "右擦过渡" },
  { label: "KAG.set_layer", type: "function", detail: "设置图层(layer, x, y, scale, opacity)" },
  { label: "KAG.move_layer", type: "function", detail: "移动图层(layer, x, y, duration)" },
  { label: "KAG.scale_layer", type: "function", detail: "缩放图层(layer, scale, duration)" },
  { label: "KAG.rotate_layer", type: "function", detail: "旋转图层(layer, angle, duration)" },
  { label: "KAG.set_opacity", type: "function", detail: "设置透明度(layer, opacity, duration)" },
  { label: "KAG.jump", type: "function", detail: "跳转(label)" },
  { label: "KAG.branch", type: "function", detail: "分支(condition, label_true, label_false)" },
  { label: "KAG.choice", type: "function", detail: "选择分支(options)" },
  { label: "KAG.flag_set", type: "function", detail: "设置标记(key, value)" },
  { label: "KAG.flag_check", type: "function", detail: "检查标记(key)" },
  { label: "KAG.show_video", type: "function", detail: "显示视频(path)" },
  { label: "KAG.pause_video", type: "function", detail: "暂停视频" },
  { label: "KAG.stop_video", type: "function", detail: "停止视频" },
  { label: "KAG.seek_video", type: "function", detail: "跳转视频(time)" },
  { label: "KAG.delay", type: "function", detail: "延迟(seconds)" },
  { label: "KAG.shake", type: "function", detail: "震动(intensity, duration)" },
  { label: "KAG.flash", type: "function", detail: "闪烁(color, duration)" },
  { label: "KAG.show_live2d", type: "function", detail: "显示Live2D(path, x, y)" },
  { label: "KAG.set_expression", type: "function", detail: "设置表情(id)" },
  { label: "KAG.set_motion", type: "function", detail: "设置动作(id)" },
  { label: "KAG.spawn_cube", type: "function", detail: "生成立方体(name, x, y, z)" },
  { label: "KAG.spawn_sphere", type: "function", detail: "生成球体(name, x, y, z)" },
  { label: "KAG.spawn_plane", type: "function", detail: "生成平面(name, x, y, z)" },
  { label: "KAG.set_camera", type: "function", detail: "设置相机(x, y, z, target_x, target_y, target_z)" },
  { label: "KAG.set_gravity", type: "function", detail: "设置重力(x, y, z)" },
];

function kagAutocomplete(context) {
  const word = context.matchBefore(/KAG\.\w*/);
  if (!word) return null;
  return {
    from: word.from + 4,
    options: KAG_COMPLETIONS.map(c => ({
      label: c.label.slice(4),
      type: c.type,
      detail: c.detail,
    })),
  };
}

const luaLanguage = StreamLanguage.define(lua);

export default function CodeEditor() {
  const { state, dispatch } = useEditor();
  const editorRef = useRef(null);
  const viewRef = useRef(null);

  const activeFile = state.openFiles[state.activeFileIdx] || { name: "untitled.lua", content: "" };

  // Initialize CodeMirror
  const initEditor = useCallback(() => {
    const container = editorRef.current;
    if (!container || viewRef.current) return;

    const updateListener = EditorView.updateListener.of((update) => {
      if (update.docChanged) {
        dispatch({ type: "SET_SCRIPT", payload: update.state.doc.toString() });
      }
    });

    const cmState = EditorState.create({
      doc: activeFile.content,
      extensions: [
        lineNumbers(),
        highlightActiveLine(),
        highlightActiveLineGutter(),
        highlightSpecialChars(),
        drawSelection(),
        rectangularSelection(),
        crosshairCursor(),
        highlightSelectionMatches(),
        bracketMatching(),
        closeBrackets(),
        indentOnInput(),
        foldGutter(),

        luaLanguage,
        syntaxHighlighting(defaultHighlightStyle, { fallback: true }),
        oneDark,

        autocompletion({
          override: [kagAutocomplete],
          activateOnTyping: true,
        }),

        keymap.of([
          ...defaultKeymap,
          ...historyKeymap,
          ...foldKeymap,
          ...completionKeymap,
          ...closeBracketsKeymap,
          ...lintKeymap,
          ...searchKeymap,
          indentWithTab,
        ]),

        history(),
        updateListener,
        EditorView.lineWrapping,

        EditorView.theme({
          "&": { height: "100%", fontSize: "13px", fontFamily: "var(--font-mono)" },
          ".cm-gutters": { backgroundColor: "var(--bg-primary)", color: "var(--fg-muted)", borderRight: "1px solid var(--border)" },
          ".cm-activeLineGutter": { backgroundColor: "var(--accent-ghost)" },
          ".cm-activeLine": { backgroundColor: "var(--accent-ghost)" },
          ".cm-foldGutter .cm-gutterElement": { color: "var(--fg-dim)" },
        }),
      ],
    });

    const view = new EditorView({
      state: cmState,
      parent: container,
    });

    viewRef.current = view;
  }, []);

  useEffect(() => {
    initEditor();
    return () => {
      if (viewRef.current) {
        viewRef.current.destroy();
        viewRef.current = null;
      }
    };
  }, [initEditor]);

  // Sync with external file changes
  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;
    const currentDoc = view.state.doc.toString();
    if (currentDoc !== activeFile.content) {
      view.dispatch({
        changes: { from: 0, to: currentDoc.length, insert: activeFile.content },
      });
    }
  }, [activeFile.content]);

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

      {/* CodeMirror editor */}
      <div ref={editorRef} style={{ flex: 1, overflow: "hidden" }} />

      {/* Status bar */}
      <div className="code-statusbar">
        <span className={`saved-dot ${state.unsaved ? "unsaved" : "saved"}`} />
        <span>{state.unsaved ? "未保存" : "已保存"}</span>
        <span className="spacer" />
        <span>Lua 5.4</span>
        <span>Ln {viewRef.current ? viewRef.current.state.doc.lines : "?"}</span>
        <span>UTF-8</span>
        <span>Tab: 2</span>
      </div>
    </div>
  );
}
