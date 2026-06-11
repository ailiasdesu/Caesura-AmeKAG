import { useState } from "react";

const L2D_MODELS = ["haru", "miku", "koharu"];
const L2D_EXPRESSIONS = ["neutral", "happy", "sad", "angry", "surprised"];
const L2D_MOTIONS = ["idle", "wave", "nod", "headshake", "point"];

const L2D_PARAMS = [
  { id: "ParamAngleX", label: "Angle X", min: -30, max: 30, def: 0 },
  { id: "ParamAngleY", label: "Angle Y", min: -30, max: 30, def: 0 },
  { id: "ParamAngleZ", label: "Angle Z", min: -30, max: 30, def: 0 },
  { id: "ParamEyeLOpen", label: "Eye L Open", min: 0, max: 1, def: 1 },
  { id: "ParamEyeROpen", label: "Eye R Open", min: 0, max: 1, def: 1 },
  { id: "ParamMouthOpenY", label: "Mouth Open", min: 0, max: 1, def: 0 },
  { id: "ParamBodyAngleX", label: "Body X", min: -10, max: 10, def: 0 },
];

export default function Live2DPanel() {
  const [model, setModel] = useState(L2D_MODELS[0]);
  const [expression, setExpression] = useState(L2D_EXPRESSIONS[0]);
  const [motion, setMotion] = useState(L2D_MOTIONS[0]);
  const [params, setParams] = useState(Object.fromEntries(L2D_PARAMS.map(p => [p.id, p.def])));

  return (
    <div className="live2d-panel">
      <div className="aux-bar-title" style={{ marginBottom: 8 }}>Live2D Cubism 5</div>
      <select value={model} onChange={(e) => setModel(e.target.value)}>
        {L2D_MODELS.map(m => <option key={m} value={m}>{m}</option>)}
      </select>
      <div className="prop-row">
        <span className="prop-label">{'\u8868\u60C5'}</span>
        <select value={expression} onChange={(e) => setExpression(e.target.value)} style={{ flex: 1 }}>
          {L2D_EXPRESSIONS.map(e => <option key={e} value={e}>{e}</option>)}
        </select>
      </div>
      <div className="prop-row">
        <span className="prop-label">{'\u52A8\u4F5C'}</span>
        <select value={motion} onChange={(e) => setMotion(e.target.value)} style={{ flex: 1 }}>
          {L2D_MOTIONS.map(m => <option key={m} value={m}>{m}</option>)}
        </select>
        <button className="btn btn-sm btn-accent">{'\u25B6'}</button>
      </div>
      <div style={{ marginTop: 12 }}>
        <div className="aux-bar-title" style={{ marginBottom: 8 }}>{'\u53C2\u6570\u8C03\u8BD5'}</div>
        {L2D_PARAMS.map(p => (
          <div key={p.id} className="prop-row">
            <span className="prop-label" style={{ fontSize: 9 }}>{p.label}</span>
            <input className="prop-slider" type="range" min={p.min} max={p.max}
              step={p.max - p.min > 10 ? 1 : 0.05}
              value={params[p.id]}
              onChange={(e) => setParams({ ...params, [p.id]: Number(e.target.value) })} />
            <span className="prop-val">{typeof params[p.id] === 'number' ? params[p.id].toFixed(2) : params[p.id]}</span>
          </div>
        ))}
      </div>
      <div style={{ marginTop: 12, padding: 8, borderRadius: "var(--radius-sm)", background: "var(--vscode-editor-background-deep)", fontSize: 10, color: "var(--green)" }}>
        {'\u25C9'} D3D11 Native — \u5DF2\u7F16\u8BD1
      </div>
    </div>
  );
}