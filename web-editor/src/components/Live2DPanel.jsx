import { useEditor } from "../context/EditorContext";

export default function Live2DPanel() {
  const { state, dispatch } = useEditor();

  if (!state.live2dEnabled) {
    return (
      <div style={{ display: "flex", flexDirection: "column", height: "100%", padding: 16, alignItems: "center", justifyContent: "center" }}>
        <div style={{ color: "var(--live2d)", fontSize: 48, marginBottom: 12 }}>🎭</div>
        <div style={{ color: "var(--fg-dim)", fontSize: 13, marginBottom: 8 }}>
          Live2D Cubism 5 — 未启用
        </div>
        <div style={{ color: "var(--fg-muted)", fontSize: 11, textAlign: "center", lineHeight: 1.6 }}>
          请在引擎构建设置中开启 CAESURA_ENABLE_LIVE2D<br />
          macOS / Linux / iOS 需共同开发者适配
        </div>
      </div>
    );
  }

  return (
    <div style={{ padding: 10, height: "100%", overflowY: "auto" }}>
      <div className="prop-group">
        <div className="prop-label">模型</div>
        {state.live2dModels.length === 0 ? (
          <div style={{ color: "var(--fg-muted)", fontSize: 11, padding: "8px 0" }}>暂无模型</div>
        ) : (
          state.live2dModels.map(m => (
            <div key={m.id}
              className={`scene-item${state.activeLive2DModel === m.id ? " active" : ""}`}
              onClick={() => dispatch({ type: "SET_ACTIVE_LIVE2D_MODEL", payload: m.id })}
            >
              {m.name}
            </div>
          ))
        )}
      </div>

      <div className="prop-group">
        <div className="prop-label">表情</div>
        <div className="prop-row">
          <select style={{ flex: 1 }}>
            <option>默认</option>
            <option>开心</option>
            <option>悲伤</option>
            <option>惊讶</option>
            <option>生气</option>
          </select>
        </div>
        <button className="btn btn-sm mt-2" style={{ width: "100%" }}>切换</button>
      </div>

      <div className="prop-group">
        <div className="prop-label">动作</div>
        <div className="prop-row">
          <select style={{ flex: 1 }}>
            <option>待机</option>
            <option>点头</option>
            <option>摇头</option>
            <option>挥手</option>
          </select>
        </div>
        <button className="btn btn-sm mt-2" style={{ width: "100%" }}>播放</button>
      </div>

      <div className="prop-group">
        <div className="prop-label">参数调试</div>
        {[
          ["呼吸幅度", 0.5],
          ["物理强度", 0.3],
          ["头部 X", 0],
          ["头部 Y", 0],
          ["眼睛开合", 1],
        ].map(([label, val]) => (
          <div key={label} className="prop-row">
            <label style={{ width: 56 }}>{label}</label>
            <input type="range" className="prop-slider" defaultValue={val} min={-1} max={1} step={0.01} />
          </div>
        ))}
      </div>

      <div className="prop-group">
        <div className="prop-label">状态</div>
        <div style={{ fontSize: 11, color: "var(--green)" }}>✅ Windows · D3D11Native</div>
        <div style={{ fontSize: 11, color: "var(--fg-muted)", marginTop: 4 }}>⚠️ macOS/Linux · 移交共同开发者</div>
      </div>
    </div>
  );
}
