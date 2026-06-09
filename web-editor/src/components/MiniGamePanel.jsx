import { useEditor } from "../context/EditorContext";

let nextObjId = 10;
let nextLightId = 10;

export default function MiniGamePanel() {
  const { state, dispatch } = useEditor();
  const objs = state.miniGameObjects;
  const lights = state.miniGameLights;

  const addObject = (type) => {
    const id = nextObjId++;
    dispatch({ type: "ADD_MINIGAME_OBJECT", payload: {
      id, type, name: `${type}_${id}`,
      pos: [0, 0, 0], scale: [1, 1, 1],
      roughness: 0.5, metallic: 0, specular: 0.5,
      collision: true, gravity: true,
    }});
  };

  const updateObj = (id, field, val) => {
    dispatch({ type: "UPDATE_MINIGAME_OBJECT", payload: { id, [field]: val } });
  };

  const addLight = (type) => {
    const id = nextLightId++;
    dispatch({ type: "ADD_MINIGAME_LIGHT", payload: {
      id, type, color: "#ffffff", intensity: 0.8,
      direction: type === "directional" ? [0, -1, 0] : undefined,
      pos: type === "point" ? [0, 2, 0] : undefined,
      range: type === "point" ? 10 : undefined,
    }});
  };

  const updateLight = (id, field, val) => {
    dispatch({ type: "UPDATE_MINIGAME_LIGHT", payload: { id, [field]: val } });
  };

  return (
    <div style={{ padding: 10, height: "100%", overflowY: "auto" }}>
      {/* Objects */}
      <div className="prop-group">
        <div className="prop-label" style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
          物体
          <span style={{ display: "flex", gap: 4 }}>
            <button className="btn btn-sm" onClick={() => addObject("Cube")} title="Cube">◻</button>
            <button className="btn btn-sm" onClick={() => addObject("Sphere")} title="Sphere">●</button>
            <button className="btn btn-sm" onClick={() => addObject("Plane")} title="Plane">▭</button>
          </span>
        </div>
        {objs.map(o => (
          <div key={o.id} style={{ border: "1px solid var(--border)", borderRadius: "var(--radius-sm)", padding: 6, marginBottom: 6 }}>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 4 }}>
              <span style={{ fontSize: 11, fontWeight: 600 }}>{o.name}</span>
              <button className="btn btn-sm" onClick={() => dispatch({ type: "REMOVE_MINIGAME_OBJECT", payload: o.id })}>×</button>
            </div>
            <div className="prop-row">
              <label>X</label><input type="number" value={o.pos[0]} onChange={e => updateObj(o.id, "pos", [Number(e.target.value), o.pos[1], o.pos[2]])} />
              <label>Y</label><input type="number" value={o.pos[1]} onChange={e => updateObj(o.id, "pos", [o.pos[0], Number(e.target.value), o.pos[2]])} />
              <label>Z</label><input type="number" value={o.pos[2]} onChange={e => updateObj(o.id, "pos", [o.pos[0], o.pos[1], Number(e.target.value)])} />
            </div>
            {/* PBR Material */}
            <div className="prop-label" style={{ fontSize: 10, marginTop: 4 }}>材质 (PBR)</div>
            <div className="prop-row"><label style={{ width: 56 }}>Roughness</label>
              <input type="range" className="prop-slider" value={o.roughness} min={0} max={1} step={0.01}
                onChange={e => updateObj(o.id, "roughness", Number(e.target.value))} />
              <span style={{ width: 28, textAlign: "right", fontSize: 10 }}>{o.roughness.toFixed(2)}</span>
            </div>
            <div className="prop-row"><label style={{ width: 56 }}>Metallic</label>
              <input type="range" className="prop-slider" value={o.metallic} min={0} max={1} step={0.01}
                onChange={e => updateObj(o.id, "metallic", Number(e.target.value))} />
              <span style={{ width: 28, textAlign: "right", fontSize: 10 }}>{o.metallic.toFixed(2)}</span>
            </div>
            <div className="prop-row"><label style={{ width: 56 }}>Specular</label>
              <input type="range" className="prop-slider" value={o.specular} min={0} max={1} step={0.01}
                onChange={e => updateObj(o.id, "specular", Number(e.target.value))} />
              <span style={{ width: 28, textAlign: "right", fontSize: 10 }}>{o.specular.toFixed(2)}</span>
            </div>
            <div className="prop-row" style={{ marginTop: 4 }}>
              <button className="btn btn-sm" onClick={() => updateObj(o.id, "collision", !o.collision)}
                style={{ background: o.collision ? "var(--accent-dim)" : "transparent", color: o.collision ? "var(--accent-bright)" : "var(--fg-dim)" }}>
                碰撞
              </button>
              <button className="btn btn-sm" onClick={() => updateObj(o.id, "gravity", !o.gravity)}
                style={{ background: o.gravity ? "var(--accent-dim)" : "transparent", color: o.gravity ? "var(--accent-bright)" : "var(--fg-dim)" }}>
                重力
              </button>
            </div>
          </div>
        ))}
      </div>

      {/* Lights */}
      <div className="prop-group">
        <div className="prop-label" style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
          光照
          <span style={{ display: "flex", gap: 4 }}>
            <button className="btn btn-sm" onClick={() => addLight("ambient")} title="环境光">☀</button>
            <button className="btn btn-sm" onClick={() => addLight("directional")} title="方向光">→</button>
            <button className="btn btn-sm" onClick={() => addLight("point")} title="点光源">☀</button>
          </span>
        </div>
        {lights.map(l => (
          <div key={l.id} style={{ border: "1px solid var(--border)", borderRadius: "var(--radius-sm)", padding: 6, marginBottom: 6 }}>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 4 }}>
              <span style={{ fontSize: 11, fontWeight: 600, textTransform: "capitalize" }}>{l.type}</span>
              <button className="btn btn-sm" onClick={() => dispatch({ type: "REMOVE_MINIGAME_LIGHT", payload: l.id })}>×</button>
            </div>
            <div className="prop-row">
              <label style={{ width: 40 }}>颜色</label>
              <input type="color" value={l.color} style={{ width: 36, height: 22, padding: 0, border: "1px solid var(--border)", borderRadius: 3, background: "none" }}
                onChange={e => updateLight(l.id, "color", e.target.value)} />
              <label style={{ width: 40 }}>强度</label>
              <input type="range" className="prop-slider" value={l.intensity} min={0} max={2} step={0.05}
                onChange={e => updateLight(l.id, "intensity", Number(e.target.value))} />
            </div>
          </div>
        ))}
      </div>

      <button className="btn btn-accent btn-sm" style={{ width: "100%", marginTop: 8 }}>
        生成 Lua 代码
      </button>
    </div>
  );
}
