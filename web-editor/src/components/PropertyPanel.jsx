import { useState } from "react";

export default function PropertyPanel() {
  const [x, setX] = useState(0);
  const [y, setY] = useState(0);
  const [scale, setScale] = useState(1.0);
  const [opacity, setOpacity] = useState(1.0);

  return (
    <div>
      <div className="aux-bar-section">
        <div className="aux-bar-title">{'\u7D20\u6750\u5C5E\u6027'}</div>
        <div className="prop-row">
          <span className="prop-label">X</span>
          <input className="prop-input" type="number" value={x} onChange={(e) => setX(Number(e.target.value))} />
        </div>
        <div className="prop-row">
          <span className="prop-label">Y</span>
          <input className="prop-input" type="number" value={y} onChange={(e) => setY(Number(e.target.value))} />
        </div>
        <div className="prop-row">
          <span className="prop-label">{'\u7F29\u653E'}</span>
          <input className="prop-slider" type="range" min="0.1" max="3" step="0.1" value={scale} onChange={(e) => setScale(Number(e.target.value))} />
          <span className="prop-val">{scale.toFixed(1)}</span>
        </div>
        <div className="prop-row">
          <span className="prop-label">{'\u900F\u660E'}</span>
          <input className="prop-slider" type="range" min="0" max="1" step="0.05" value={opacity} onChange={(e) => setOpacity(Number(e.target.value))} />
          <span className="prop-val">{opacity.toFixed(2)}</span>
        </div>
      </div>
      <div className="aux-bar-section">
        <div className="aux-bar-title">{'\u4EE3\u7801\u9884\u89C8'}</div>
        <div className="prop-code-preview">
{`KAG.layer_show("bg", "classroom.png", {
  x = ${x}, y = ${y},
  scale = ${scale.toFixed(1)},
  opacity = ${opacity.toFixed(2)}
})`}
        </div>
      </div>
    </div>
  );
}