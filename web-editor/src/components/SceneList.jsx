import { useState } from "react";
import { useEditor } from "../context/EditorContext";

const SCENE_TEMPLATES = [
  { id: "prologue", label: "\u5E8F\u5E55" },
  { id: "daily", label: "\u65E5\u5E38" },
  { id: "battle", label: "\u6218\u6597" },
  { id: "hscene", label: "H \u573A\u666F" },
];

export default function SceneList() {
  const { state, dispatch } = useEditor();
  const [showTemplates, setShowTemplates] = useState(false);
  const [dragIdx, setDragIdx] = useState(null);

  const scenes = state.sceneList || [
    { id: "start", name: "start.cae", active: true },
    { id: "prologue", name: "prologue.cae" },
    { id: "ch1", name: "chapter1.cae" },
  ];

  const handleAdd = (template) => {
    dispatch({ type: "ADD_SCENE", payload: { name: `scene_${Date.now()}.cae`, template } });
    setShowTemplates(false);
  };

  return (
    <div className="scene-section">
      <div className="scene-section-header">
        <span className="scene-section-title">\u573A\u666F\u5217\u8868</span>
        <button className="scene-add-btn" onClick={() => setShowTemplates(!showTemplates)} title="\u6DFB\u52A0\u573A\u666F">+</button>
      </div>
      {showTemplates && (
        <div style={{ padding: "0 12px 8px", display: "flex", gap: 4, flexWrap: "wrap" }}>
          {SCENE_TEMPLATES.map(t => (
            <button key={t.id} className="btn btn-sm" onClick={() => handleAdd(t.id)}>{t.label}</button>
          ))}
        </div>
      )}
      <div className="scene-list-body">
        {scenes.map((s, i) => (
          <div key={s.id || i}
            className={`scene-item${s.active ? " active" : ""}${dragIdx === i ? " drag-over" : ""}`}
            draggable
            onClick={() => dispatch({ type: "SET_ACTIVE_SCENE", payload: i })}
            onDragStart={(e) => { setDragIdx(i); e.dataTransfer.effectAllowed = "move"; }}
            onDragOver={(e) => { e.preventDefault(); e.dataTransfer.dropEffect = "move"; }}
            onDragEnter={(e) => { e.preventDefault(); if (dragIdx !== i) setDragIdx(i); }}
            onDragLeave={() => setDragIdx(null)}
            onDrop={(e) => { e.preventDefault(); setDragIdx(null); dispatch({ type: "REORDER_SCENES", payload: { from: dragIdx, to: i } }); }}
            onDragEnd={() => setDragIdx(null)}
          >
            <span className="scene-item-icon">{'\u25C9'}</span>
            <span className="scene-item-label">{s.name}</span>
          </div>
        ))}
      </div>
    </div>
  );
}