import { useEditor } from "../context/EditorContext";
import { useState } from "react";

const TEMPLATES = [
  { name: "空白场景", code: "-- New scene\nfunction scene_N()\n    KAG.clear_screen()\nend\n" },
  { name: "教室", code: "-- Classroom scene\nfunction scene_N()\n    KAG.clear_screen()\n    KAG.show_image(\"bg\", \"assets/bg/classroom.png\", 0, 0)\n    KAG.show_text(\"...\")\nend\n" },
  { name: "户外", code: "-- Outdoor scene\nfunction scene_N()\n    KAG.clear_screen()\n    KAG.show_image(\"bg\", \"assets/bg/outdoor.png\", 0, 0)\n    KAG.play_bgm(\"assets/bgm/outdoor.ogg\", 1.0)\nend\n" },
];

export default function SceneList() {
  const { state, dispatch } = useEditor();
  const [showTemplates, setShowTemplates] = useState(false);
  const [dragIdx, setDragIdx] = useState(null);

  const addScene = (template) => {
    const id = `scene_${Date.now()}`;
    const fnName = `scene_${state.scenes.length}`;
    const code = template.code.replace(/scene_N/g, fnName);
    dispatch({ type: "ADD_SCENE", payload: { id, name: template.name, line: 0 } });
    const newScript = state.script + "\n" + code;
    dispatch({ type: "SET_SCRIPT", payload: newScript });
    setShowTemplates(false);
  };

  const onDragStart = (idx) => setDragIdx(idx);
  const onDragOver = (e) => e.preventDefault();
  const onDrop = (idx) => {
    if (dragIdx === null || dragIdx === idx) return;
    const reordered = [...state.scenes];
    const [moved] = reordered.splice(dragIdx, 1);
    reordered.splice(idx, 0, moved);
    dispatch({ type: "REORDER_SCENES", payload: reordered });
    setDragIdx(null);
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
      <div className="panel-header">
        <span>📋 场景</span>
        <button className="btn btn-sm" onClick={() => setShowTemplates(!showTemplates)} title="添加场景">
          +
        </button>
      </div>

      {showTemplates && (
        <div style={{ padding: "4px 8px", borderBottom: "1px solid var(--border)" }}>
          {TEMPLATES.map(t => (
            <div key={t.name} className="scene-item" onClick={() => addScene(t)}
              style={{ justifyContent: "flex-start", gap: 8 }}>
              + {t.name}
            </div>
          ))}
        </div>
      )}

      <div style={{ flex: 1, overflowY: "auto" }}>
        {state.scenes.map((scene, idx) => (
          <div key={scene.id}
            className={`scene-item${state.activeScene === scene.id ? " active" : ""}${dragIdx === idx ? " dragging" : ""}`}
            onClick={() => dispatch({ type: "SET_ACTIVE_SCENE", payload: scene.id })}
            draggable
            onDragStart={() => onDragStart(idx)}
            onDragOver={onDragOver}
            onDrop={() => onDrop(idx)}
            onDragEnd={() => setDragIdx(null)}
          >
            <span style={{ fontSize: 10, color: "var(--fg-muted)" }}>L{scene.line || "?"}</span>
            <span>{scene.name}</span>
          </div>
        ))}
        {state.scenes.length === 0 && (
          <div style={{ padding: 16, textAlign: "center", color: "var(--fg-muted)", fontSize: 11 }}>
            点击 + 添加场景
          </div>
        )}
      </div>
    </div>
  );
}
