import { generateKAGScript } from "../parser/kagParser";

import { useEditor } from "../context/EditorContext";

export default function PropertyPanel() {
  const { state, dispatch } = useEditor();
  const ev = state.selectedEvent;

  if (!ev) {
    return (
      <div className="panel-content" style={{ padding: 16, color: "#555", fontSize: 11, textAlign: "center" }}>
        Click an event in the timeline<br />to edit its properties.
      </div>
    );
  }

  const updateParam = (key, value) => {
    const updated = { ...ev, params: { ...ev.params, [key]: value } };
    // Rebuild script
    const allEvents = state.timelineEvents.map((e) => (e.id === ev.id ? updated : e)); const newScript = generateKAGScript(allEvents);
    dispatch({ type: "SET_SCRIPT", payload: newScript });
    dispatch({ type: "SET_SELECTED", payload: updated });
  };

  return (
    <div className="panel-content">
      <div className="prop-group">
        <span className="prop-label">Event Type</span>
        <div style={{ color: "#aaa", fontSize: 11 }}>{ev.label}</div>
      </div>

      {ev.type === "image" && (
        <>
          <div className="prop-group">
            <span className="prop-label">Layer</span>
            <select className="prop-select wide" value={ev.params.layer || "fg"} onChange={(e) => updateParam("layer", e.target.value)}>
              <option value="bg">Background (bg)</option>
              <option value="fg">Foreground (fg)</option>
              <option value="msg">Message (msg)</option>
            </select>
          </div>
          <div className="prop-group">
            <span className="prop-label">Image Path</span>
            <input className="prop-input wide" value={ev.params.path || ""} onChange={(e) => updateParam("path", e.target.value)} />
          </div>
          <div className="prop-group">
            <span className="prop-label">Position</span>
            <div className="prop-row">
              <label>X</label><input className="prop-input" type="number" value={ev.params.x || 0} onChange={(e) => updateParam("x", parseFloat(e.target.value) || 0)} />
              <label>Y</label><input className="prop-input" type="number" value={ev.params.y || 0} onChange={(e) => updateParam("y", parseFloat(e.target.value) || 0)} />
            </div>
          </div>
          <div className="prop-group">
            <span className="prop-label">Appearance</span>
            <div className="prop-row">
              <label>Scale</label><input className="prop-input" type="number" step="0.1" min="0.1" max="5" value={ev.params.scale || 1} onChange={(e) => updateParam("scale", parseFloat(e.target.value) || 1)} />
              <label>Opacity</label><input className="prop-input" type="number" step="0.1" min="0" max="1" value={ev.params.opacity ?? 1} onChange={(e) => updateParam("opacity", parseFloat(e.target.value) ?? 1)} />
            </div>
          </div>
        </>
      )}

      {ev.type === "text" && (
        <div className="prop-group">
          <span className="prop-label">Text</span>
          <textarea className="prop-input wide" style={{ minHeight: 60, resize: "vertical" }}
            value={ev.params.text || ""} onChange={(e) => updateParam("text", e.target.value)} />
        </div>
      )}

      {ev.type === "audio" && (
        <>
          <div className="prop-group">
            <span className="prop-label">Audio File</span>
            <input className="prop-input wide" value={ev.params.path || ""} onChange={(e) => updateParam("path", e.target.value)} />
          </div>
          {ev.func === "playBgm" && (
            <div className="prop-group">
              <span className="prop-label">Fade Time (s)</span>
              <input className="prop-input wide" type="number" step="0.1" min="0" value={ev.params.fade || 1} onChange={(e) => updateParam("fade", parseFloat(e.target.value) || 1)} />
            </div>
          )}
        </>
      )}

      {ev.type === "effect" && (
        <div className="prop-group">
          <span className="prop-label">Intensity</span>
          <input className="prop-input wide" type="number" step="0.1" min="0.1" max="50" value={ev.params.intensity || 5} onChange={(e) => updateParam("intensity", parseFloat(e.target.value) || 5)} />
        </div>
      )}
    </div>
  );
}