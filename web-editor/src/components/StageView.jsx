import { useEditor } from "../context/EditorContext";
import { useEffect, useRef, useCallback, useState } from "react";

export default function StageView() {
  const { state, dispatch } = useEditor();
  const canvasRef = useRef(null);
  const areaRef = useRef(null);
  const [didInitialFit, setDidInitialFit] = useState(false);
  const [isPanning, setIsPanning] = useState(false);
  const panStart = useRef({ x: 0, y: 0, px: 0, py: 0 });
  const { resolution, stageZoom, stagePanX, stagePanY, stageGrid, stageSafe, previewing } = state;

  const VP_W = resolution?.w || 1920;
  const VP_H = resolution?.h || 1080;

  // One-time fit on mount
  useEffect(() => {
    const area = areaRef.current;
    if (!area) return;
    if (!didInitialFit) {
      const aw = area.clientWidth - 48;
      const ah = area.clientHeight - 48;
      const s = Math.min(aw / VP_W, ah / VP_H, 1.0);
      dispatch({ type: "SET_ZOOM", payload: s });
      setDidInitialFit(true);
    }
  }, [VP_W, VP_H, dispatch, didInitialFit]);

  const zoom = (stageZoom != null && stageZoom !== 0) ? stageZoom : 0.55;

  // Draw canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    canvas.width = VP_W;
    canvas.height = VP_H;
    const ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, VP_W, VP_H);
    ctx.fillStyle = "#1a1a24";
    ctx.fillRect(0, 0, VP_W, VP_H);

    if (stageGrid) {
      ctx.strokeStyle = "rgba(255,255,255,0.02)";
      ctx.lineWidth = 1;
      const gs = 40;
      for (let x = gs; x < VP_W; x += gs) {
        ctx.beginPath(); ctx.moveTo(x + 0.5, 0); ctx.lineTo(x + 0.5, VP_H); ctx.stroke();
      }
      for (let y = gs; y < VP_H; y += gs) {
        ctx.beginPath(); ctx.moveTo(0, y + 0.5); ctx.lineTo(VP_W, y + 0.5); ctx.stroke();
      }
    }

    if (stageSafe) {
      const mx = VP_W * 0.1, my = VP_H * 0.1;
      ctx.strokeStyle = "rgba(108, 92, 231, 0.3)";
      ctx.lineWidth = 1;
      ctx.setLineDash([6, 4]);
      ctx.strokeRect(mx, my, VP_W - mx * 2, VP_H - my * 2);
      ctx.setLineDash([]);
    }
  }, [VP_W, VP_H, stageGrid, stageSafe]);

  // Mouse pan handlers
  const handleMouseDown = useCallback((e) => {
    if (e.button === 1 || (e.button === 0 && e.altKey)) {
      e.preventDefault();
      setIsPanning(true);
      panStart.current = { x: e.clientX, y: e.clientY, px: stagePanX, py: stagePanY };
    }
  }, [stagePanX, stagePanY]);

  const handleMouseMove = useCallback((e) => {
    if (!isPanning) return;
    const dx = (e.clientX - panStart.current.x) / zoom;
    const dy = (e.clientY - panStart.current.y) / zoom;
    dispatch({ type: "SET_PAN", payload: { x: panStart.current.px + dx, y: panStart.current.py + dy } });
  }, [isPanning, zoom, dispatch]);

  const handleMouseUp = useCallback(() => {
    setIsPanning(false);
  }, []);

  // Global mouseup to catch releases outside the area
  useEffect(() => {
    window.addEventListener("mouseup", handleMouseUp);
    return () => window.removeEventListener("mouseup", handleMouseUp);
  }, [handleMouseUp]);

  const handleDrop = useCallback((e) => {
    e.preventDefault();
    const data = e.dataTransfer.getData("text/plain");
    if (data) {
      dispatch({ type: "ADD_LOG", payload: { level: "info", message: "\u62D6\u5165\u7D20\u6750: " + data } });
    }
  }, [dispatch]);

  return (
    <div ref={areaRef} className="stage-area"
      onDragOver={(e) => e.preventDefault()}
      onDrop={handleDrop}
      onMouseDown={handleMouseDown}
      onMouseMove={handleMouseMove}
      style={{ cursor: isPanning ? "grabbing" : "default" }}
    >
      <div className={`stage-viewport${previewing ? " preview-active" : ""}`}
        style={{
          width: VP_W + "px", height: VP_H + "px",
          transform: `scale(${zoom}) translate(${stagePanX}px, ${stagePanY}px)`,
        }}
      >
        <canvas ref={canvasRef} className="stage-canvas" />
      </div>

      {/* Floating toolbar */}
      <div className="stage-toolbar">
        <button className={stageGrid ? "active" : ""}
          onClick={() => dispatch({ type: "TOGGLE_GRID" })} title="\u7F51\u683C (G)">
          {'\u229E'}
        </button>
        <button className={stageSafe ? "active" : ""}
          onClick={() => dispatch({ type: "TOGGLE_SAFE" })} title="\u5B89\u5168\u533A (S)">
          {'\u22A1'}
        </button>
        <button onClick={() => dispatch({ type: "SET_ZOOM", payload: Math.max(0.25, zoom - 0.1) })}
          title="\u7F29\u5C0F">\u2212</button>
        <span style={{ font: "var(--text-xs) var(--font-mono)", color: "var(--vscode-descriptionForeground)", minWidth: 36, textAlign: "center" }}>
          {Math.round(zoom * 100)}%
        </span>
        <button onClick={() => dispatch({ type: "SET_ZOOM", payload: Math.min(2, zoom + 0.1) })}
          title="\u653E\u5927">+</button>
        <button onClick={() => {
          const area = areaRef.current;
          if (area) { const s = Math.min((area.clientWidth - 48) / VP_W, (area.clientHeight - 48) / VP_H, 1); dispatch({ type: "SET_ZOOM", payload: s }); dispatch({ type: "RESET_PAN" }); }
        }} title="\u9002\u914D + \u5F52\u4F4D">\u2194</button>
        <select value={`${VP_W}\u00D7${VP_H}`}
          onChange={(e) => { const [w, h] = e.target.value.split("\u00D7").map(Number); dispatch({ type: "SET_RESOLUTION", payload: { w, h } }); }}>
          <option>1920\u00D71080</option>
          <option>2560\u00D71440</option>
          <option>3840\u00D72160</option>
          <option>1280\u00D7720</option>
        </select>
        <button className={previewing ? "active" : ""}
          onClick={() => dispatch({ type: "SET_PREVIEW", payload: !previewing })}
          title="Preview">
          {'\u25B6'} Preview
        </button>
      </div>
    </div>
  );
}
