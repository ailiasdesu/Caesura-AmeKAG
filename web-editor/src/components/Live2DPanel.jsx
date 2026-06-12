import { useState, useEffect } from "react";

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

const API_BASE = "http://localhost:9876";

export default function Live2DPanel() {
  const [models, setModels] = useState([]);
  const [model, setModel] = useState("");
  const [expression, setExpression] = useState(L2D_EXPRESSIONS[0]);
  const [motion, setMotion] = useState(L2D_MOTIONS[0]);
  const [params, setParams] = useState(Object.fromEntries(L2D_PARAMS.map(p => [p.id, p.def])));
  const [loadedModel, setLoadedModel] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);

  useEffect(() => {
    fetch(`${API_BASE}/api/live2d/models`)
      .then(r => r.json())
      .then(data => {
        setModels(data);
        if (data.length > 0) setModel(data[0].path || data[0].name);
      })
      .catch(() => setModels([]));
  }, []);

  const handleLoadModel = () => {
    if (!model) return;
    setLoading(true);
    setError(null);
    fetch(`${API_BASE}/api/live2d/load`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ modelPath: model }),
    })
      .then(r => r.json())
      .then(data => {
        if (data.status === "ok") {
          setLoadedModel({ id: data.modelId, name: data.name });
        } else {
          setError(data.error || "Failed to load model");
        }
      })
      .catch(err => setError(err.message))
      .finally(() => setLoading(false));
  };

  return (
    <div className="live2d-panel">
      <div className="aux-bar-title" style={{ marginBottom: 8 }}>Live2D Cubism 5</div>
      <select value={model} onChange={(e) => setModel(e.target.value)}>
        {models.length === 0 && <option value="">No models found</option>}
        {models.map(m => {
          const val = m.path || m.name;
          return <option key={val} value={val}>{m.name || val}</option>;
        })}
      </select>
      <button className="btn btn-sm btn-accent"
        style={{ marginTop: 6, width: "100%" }}
        onClick={handleLoadModel}
        disabled={loading || !model}>
        {loading ? "Loading..." : "Load Model"}
      </button>
      {loadedModel && (
        <div className="prop-row" style={{ marginTop: 4, fontSize: 10, color: "var(--green)" }}>
          Loaded: {loadedModel.name}
        </div>
      )}
      {error && (
        <div className="prop-row" style={{ marginTop: 4, fontSize: 10, color: "var(--red)" }}>
          {error}
        </div>
      )}
      <div className="prop-row">
        <span className="prop-label">Expression</span>
        <select value={expression} onChange={(e) => setExpression(e.target.value)} style={{ flex: 1 }}>
          {L2D_EXPRESSIONS.map(e => <option key={e} value={e}>{e}</option>)}
        </select>
      </div>
      <div className="prop-row">
        <span className="prop-label">Motion</span>
        <select value={motion} onChange={(e) => setMotion(e.target.value)} style={{ flex: 1 }}>
          {L2D_MOTIONS.map(m => <option key={m} value={m}>{m}</option>)}
        </select>
        <button className="btn btn-sm btn-accent">Play</button>
      </div>
      <div style={{ marginTop: 12 }}>
        <div className="aux-bar-title" style={{ marginBottom: 8 }}>Parameters</div>
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
        D3D11 Native — Compiled
      </div>
    </div>
  );
}
