import { useState } from "react";

const GEOMETRIES = [
  { id: "cube", label: "Cube", icon: "\u25A0" },
  { id: "sphere", label: "Sphere", icon: "\u25CF" },
  { id: "plane", label: "Plane", icon: "\u25AC" },
  { id: "light_point", label: "Point Light", icon: "\u2600" },
  { id: "light_dir", label: "Directional", icon: "\u2192" },
  { id: "camera", label: "Camera", icon: "\u25C9" },
];

export default function MiniGamePanel() {
  const [objects, setObjects] = useState([
    { id: "cube_0", type: "cube", roughness: 0.4, metallic: 0.0, specular: 0.5 },
  ]);
  const [lightColor, setLightColor] = useState("#ffffff");
  const [lightIntensity, setLightIntensity] = useState(1.0);

  const addObject = (type) => {
    setObjects([...objects, { id: `${type}_${Date.now()}`, type, roughness: 0.4, metallic: 0.0, specular: 0.5 }]);
  };

  return (
    <div className="minigame-panel">
      <div className="aux-bar-title" style={{ marginBottom: 8 }}>{'\u7269\u4F53\u5217\u8868'}</div>
      <div style={{ display: "flex", gap: 4, flexWrap: "wrap", marginBottom: 12 }}>
        {GEOMETRIES.map(g => (
          <button key={g.id} className="btn btn-sm" onClick={() => addObject(g.type)}
            title={g.label}>{g.icon} {g.label}</button>
        ))}
      </div>
      {objects.map(obj => (
        <div key={obj.id} style={{ padding: 6, marginBottom: 4, borderRadius: "var(--radius-sm)", background: "var(--vscode-editor-background-deep)", fontSize: 11 }}>
          <div style={{ color: "var(--vscode-descriptionForeground)", marginBottom: 4 }}>{obj.id}</div>
          <div className="prop-row">
            <span className="prop-label">Rough</span>
            <input className="prop-slider" type="range" min="0" max="1" step="0.05" value={obj.roughness}
              onChange={(e) => { const n = Number(e.target.value); setObjects(objects.map(o => o.id === obj.id ? { ...o, roughness: n } : o)); }} />
            <span className="prop-val">{obj.roughness.toFixed(2)}</span>
          </div>
          <div className="prop-row">
            <span className="prop-label">Metal</span>
            <input className="prop-slider" type="range" min="0" max="1" step="0.05" value={obj.metallic}
              onChange={(e) => { const n = Number(e.target.value); setObjects(objects.map(o => o.id === obj.id ? { ...o, metallic: n } : o)); }} />
            <span className="prop-val">{obj.metallic.toFixed(2)}</span>
          </div>
          <div className="prop-row">
            <span className="prop-label">Spec</span>
            <input className="prop-slider" type="range" min="0" max="1" step="0.05" value={obj.specular}
              onChange={(e) => { const n = Number(e.target.value); setObjects(objects.map(o => o.id === obj.id ? { ...o, specular: n } : o)); }} />
            <span className="prop-val">{obj.specular.toFixed(2)}</span>
          </div>
        </div>
      ))}
      <div className="aux-bar-title" style={{ marginBottom: 8, marginTop: 12 }}>{'\u5149\u7167'}</div>
      <div className="prop-row">
        <span className="prop-label">{'\u989C\u8272'}</span>
        <input type="color" value={lightColor} onChange={(e) => setLightColor(e.target.value)}
          style={{ width: 24, height: 24, border: "none", cursor: "pointer", background: "transparent" }} />
      </div>
      <div className="prop-row">
        <span className="prop-label">{'\u5F3A\u5EA6'}</span>
        <input className="prop-slider" type="range" min="0" max="5" step="0.1" value={lightIntensity}
          onChange={(e) => setLightIntensity(Number(e.target.value))} />
        <span className="prop-val">{lightIntensity.toFixed(1)}</span>
      </div>
    </div>
  );
}