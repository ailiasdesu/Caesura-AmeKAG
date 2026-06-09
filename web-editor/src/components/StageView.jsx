import { useEditor } from "../context/EditorContext";
import { useEffect, useRef } from "react";

export default function StageView() {
  const { state, dispatch } = useEditor();
  const canvasRef = useRef(null);
  const { resolution, stageZoom, stageGrid, stageSafe, previewing } = state;

  const w = resolution.w * stageZoom;
  const h = resolution.h * stageZoom;

  // Draw grid + safe zone on canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    canvas.width = w;
    canvas.height = h;
    const ctx = canvas.getContext("2d");

    // Clear
    ctx.fillStyle = "#1a1a24";
    ctx.fillRect(0, 0, w, h);

    // Grid
    if (stageGrid) {
      ctx.strokeStyle = "rgba(30, 30, 42, 0.4)";
      ctx.lineWidth = 1;
      const gridSize = 40 * stageZoom;
      for (let x = gridSize; x < w; x += gridSize) {
        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
      }
      for (let y = gridSize; y < h; y += gridSize) {
        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
      }
    }

    // Safe zone
    if (stageSafe) {
      const marginX = w * 0.1;
      const marginY = h * 0.1;
      ctx.strokeStyle = "rgba(42, 42, 58, 0.5)";
      ctx.lineWidth = 1;
      ctx.setLineDash([6, 4]);
      ctx.strokeRect(marginX, marginY, w - marginX * 2, h - marginY * 2);
      ctx.setLineDash([]);
    }
  }, [w, h, stageGrid, stageSafe, stageZoom]);

  return (
    <div className="stage-container">
      <div className={`stage-canvas-wrap${previewing ? " previewing" : ""}`}
        style={{ width: w, height: h }}
      >
        {stageGrid && <div className="grid-bg" />}
        {stageSafe && (
          <div className="safe-zone"
            style={{ left: "10%", top: "10%", width: "80%", height: "80%" }} />
        )}
        <canvas ref={canvasRef}
          style={{ width: w, height: h, display: "block" }}
        />
      </div>

      {/* Floating toolbar */}
      <div className="stage-toolbar">
        <button className={`btn btn-icon btn-sm${stageGrid ? " btn-accent" : ""}`}
          onClick={() => dispatch({ type: "TOGGLE_GRID" })}
          title="网格 (G)">⊞</button>
        <button className={`btn btn-icon btn-sm${stageSafe ? "" : ""}`}
          onClick={() => dispatch({ type: "TOGGLE_SAFE" })}
          title="安全区 (S)">⊡</button>
        <button className="btn btn-icon btn-sm"
          onClick={() => dispatch({ type: "SET_ZOOM", payload: Math.max(0.25, stageZoom - 0.25) })}
          title="缩小">−</button>
        <span style={{ fontSize: 10, color: "var(--fg-dim)", minWidth: 36, textAlign: "center" }}>
          {Math.round(stageZoom * 100)}%
        </span>
        <button className="btn btn-icon btn-sm"
          onClick={() => dispatch({ type: "SET_ZOOM", payload: Math.min(2, stageZoom + 0.25) })}
          title="放大">+</button>
        <select className="btn btn-sm" style={{ fontSize: 10 }}
          value={`${resolution.w}×${resolution.h}`}
          onChange={(e) => {
            const [w, h] = e.target.value.split("×").map(Number);
            dispatch({ type: "SET_RESOLUTION", payload: { w, h } });
          }}
        >
          <option>1280×720</option>
          <option>1920×1080</option>
        </select>
        <button className={`btn btn-sm${previewing ? " btn-success" : ""}`}
          onClick={() => dispatch({ type: "SET_PREVIEW", payload: !previewing })}
          title="实时预览"
        >
          ▶ Preview
        </button>
      </div>
    </div>
  );
}
