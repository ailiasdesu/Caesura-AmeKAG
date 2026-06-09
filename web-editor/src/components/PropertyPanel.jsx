import { useEditor } from "../context/EditorContext";

export default function PropertyPanel() {
  const { state, dispatch } = useEditor();
  const obj = state.selectedObject;

  if (!obj) {
    return (
      <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
        <div className="panel-header">
          <span>📐 属性</span>
        </div>
        <div style={{ flex: 1, display: "flex", alignItems: "center", justifyContent: "center", padding: 20 }}>
          <div style={{ textAlign: "center", color: "var(--fg-muted)", fontSize: 11, lineHeight: 1.8 }}>
            <div style={{ fontSize: 32, marginBottom: 8 }}>👆</div>
            选舞台上的素材<br />或从素材树拖入
          </div>
        </div>
      </div>
    );
  }

  // Determine context based on object type
  const type = obj.type;
  const isSprite = type === "sprite" || type === "image";
  const isLive2D = type === "live2d";
  const isMiniGame = type === "minigame";

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
      <div className="panel-header">
        <span>
          {isSprite ? "🖼 素材属性" : isLive2D ? "🎭 Live2D 属性" : isMiniGame ? "🎮 物体属性" : "📐 属性"}
        </span>
        <button className="btn btn-sm" onClick={() => dispatch({ type: "SELECT_OBJECT", payload: null })}>✕</button>
      </div>

      <div style={{ flex: 1, overflowY: "auto" }}>
        {/* Layer */}
        <div className="prop-group">
          <div className="prop-label">图层</div>
          <div className="prop-row">
            <select value={obj.layer || "bg"}
              onChange={e => dispatch({ type: "SELECT_OBJECT", payload: { ...obj, layer: e.target.value } })}>
              <option value="bg">bg — 背景</option>
              <option value="fg">fg — 前景</option>
              <option value="msg">msg — 消息层</option>
            </select>
          </div>
        </div>

        {/* Position */}
        <div className="prop-group">
          <div className="prop-label">位置</div>
          <div className="prop-row">
            <label>X</label>
            <input type="number" value={obj.x || 0}
              onChange={e => dispatch({ type: "SELECT_OBJECT", payload: { ...obj, x: Number(e.target.value) }})} />
            <label>Y</label>
            <input type="number" value={obj.y || 0}
              onChange={e => dispatch({ type: "SELECT_OBJECT", payload: { ...obj, y: Number(e.target.value) }})} />
          </div>
        </div>

        {/* Sprite-specific */}
        {isSprite && (
          <>
            <div className="prop-group">
              <div className="prop-label">缩放</div>
              <div className="prop-row">
                <input type="range" className="prop-slider" value={obj.scale || 1} min={0.1} max={5} step={0.05}
                  onChange={e => dispatch({ type: "SELECT_OBJECT", payload: { ...obj, scale: Number(e.target.value) }})} />
                <span style={{ width: 36, textAlign: "right", fontSize: 10, color: "var(--fg-dim)" }}>
                  {((obj.scale || 1) * 100).toFixed(0)}%
                </span>
              </div>
            </div>
            <div className="prop-group">
              <div className="prop-label">透明度</div>
              <div className="prop-row">
                <input type="range" className="prop-slider" value={obj.opacity ?? 1} min={0} max={1} step={0.01}
                  onChange={e => dispatch({ type: "SELECT_OBJECT", payload: { ...obj, opacity: Number(e.target.value) }})} />
                <span style={{ width: 36, textAlign: "right", fontSize: 10, color: "var(--fg-dim)" }}>
                  {((obj.opacity ?? 1) * 100).toFixed(0)}%
                </span>
              </div>
            </div>
            <div className="prop-group">
              <div className="prop-label">文件</div>
              <div style={{ fontSize: 10, color: "var(--fg-muted)", wordBreak: "break-all", padding: "2px 0" }}>
                {obj.path || "未指定"}
              </div>
            </div>
            <div className="prop-group">
              <div className="prop-label">效果预设</div>
              <div className="prop-row">
                <select defaultValue="none">
                  <option value="none">无</option>
                  <option value="fadeIn">淡入</option>
                  <option value="fadeOut">淡出</option>
                  <option value="shake">震动</option>
                  <option value="zoom">缩放</option>
                </select>
              </div>
            </div>
          </>
        )}

        {/* Live2D specific */}
        {isLive2D && (
          <>
            <div className="prop-group">
              <div className="prop-label">表情</div>
              <select style={{ width: "100%", padding: "3px 6px", border: "1px solid var(--border)", borderRadius: 3, background: "var(--bg-input)", color: "var(--fg)", fontSize: 11 }}>
                <option>默认</option>
                <option>开心</option>
                <option>悲伤</option>
                <option>惊讶</option>
              </select>
            </div>
            <div className="prop-group">
              <div className="prop-label">呼吸幅度</div>
              <input type="range" className="prop-slider" defaultValue={0.5} min={0} max={1} step={0.01} />
            </div>
            <div className="prop-group">
              <div className="prop-label">物理强度</div>
              <input type="range" className="prop-slider" defaultValue={0.3} min={0} max={1} step={0.01} />
            </div>
          </>
        )}

        {/* MiniGame specific */}
        {isMiniGame && (
          <>
            <div className="prop-group">
              <div className="prop-label">位置</div>
              <div className="prop-row"><label>X</label><input type="number" defaultValue={0} /></div>
              <div className="prop-row"><label>Y</label><input type="number" defaultValue={0} /></div>
              <div className="prop-row"><label>Z</label><input type="number" defaultValue={0} /></div>
            </div>
            <div className="prop-group">
              <div className="prop-label">材质 (PBR)</div>
              <div className="prop-row"><label style={{ width: 56 }}>Rough</label><input type="range" className="prop-slider" defaultValue={0.5} min={0} max={1} step={0.01} /></div>
              <div className="prop-row"><label style={{ width: 56 }}>Metal</label><input type="range" className="prop-slider" defaultValue={0} min={0} max={1} step={0.01} /></div>
              <div className="prop-row"><label style={{ width: 56 }}>Spec</label><input type="range" className="prop-slider" defaultValue={0.5} min={0} max={1} step={0.01} /></div>
            </div>
            <div className="prop-row" style={{ gap: 4 }}>
              <button className="btn btn-sm" style={{ flex: 1 }}>碰撞</button>
              <button className="btn btn-sm" style={{ flex: 1 }}>重力</button>
            </div>
          </>
        )}

        {/* KAG Code preview */}
        <div className="prop-group">
          <div className="prop-label">KAG 代码预览</div>
          <pre style={{
            fontSize: 10, fontFamily: "var(--font-mono)", color: "var(--accent-bright)",
            background: "var(--bg-deep)", padding: 6, borderRadius: "var(--radius-sm)",
            overflowX: "auto", whiteSpace: "pre-wrap",
          }}>
            {isSprite && `KAG.show_image("${obj.layer || "bg"}", "${obj.path || "..."}", ${obj.x || 0}, ${obj.y || 0})`}
            {isLive2D && `KAG.show_live2d("${obj.path || "..."}", ${obj.x || 0}, ${obj.y || 0})`}
            {isMiniGame && `KAG.spawn_${obj.objType || "cube"}("${obj.name || "obj"}", ${obj.x || 0}, ${obj.y || 0}, 0)`}
          </pre>
        </div>
      </div>
    </div>
  );
}
