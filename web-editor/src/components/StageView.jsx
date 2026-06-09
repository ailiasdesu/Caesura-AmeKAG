import { useRef, useEffect, useCallback, useState } from "react";
import { useEditor } from "../context/EditorContext";
import { extractAssetPaths } from "../parser/kagParser";

const GRID_SIZE = 40;

export default function StageView() {
  const { state, dispatch } = useEditor();
  const canvasRef = useRef(null);
  const assets = state.assets;
  const script = state.script;

  const [previewFrame, setPreviewFrame] = useState(null);
  const [livePreview, setLivePreview] = useState(false);
  const previewTimerRef = useRef(null);

  // Live preview polling
  useEffect(() => {
    if (!livePreview) {
      if (previewTimerRef.current) { clearInterval(previewTimerRef.current); previewTimerRef.current = null; }
      setPreviewFrame(null);
      return;
    }
    const poll = async () => {
      try {
        const result = await window.caesura.getFrame(state.resolution.w, state.resolution.h);
        if (result && result.frame) setPreviewFrame(result.frame);
      } catch (e) { /* engine may be busy */ }
    };
    poll();
    previewTimerRef.current = setInterval(poll, 200);
    return () => { if (previewTimerRef.current) clearInterval(previewTimerRef.current); };
  }, [livePreview, state.resolution.w, state.resolution.h]);

  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    const w = state.resolution.w;
    const h = state.resolution.h;

    canvas.width = w;
    canvas.height = h;

    // Background
    ctx.fillStyle = "#1a1a24";
    ctx.fillRect(0, 0, w, h);

    // Live preview frame (from engine)
    if (livePreview && previewFrame) {
      const img = new Image();
      img.onload = () => { ctx.drawImage(img, 0, 0, w, h); };
      img.src = "data:image/png;base64," + previewFrame;
    }

    // Grid
    if (state.stageGrid) {
      ctx.strokeStyle = "#1e1e2a";
      ctx.lineWidth = 1;
      for (let x = 0; x < w; x += GRID_SIZE) {
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, h);
        ctx.stroke();
      }
      for (let y = 0; y < h; y += GRID_SIZE) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
      }
    }

    // Safe area
    ctx.strokeStyle = "#2a2a3a";
    ctx.lineWidth = 2;
    ctx.strokeRect(64, 64, w - 128, h - 128);
    ctx.fillStyle = "#2a2a3a";
    ctx.font = "10px sans-serif";
    ctx.fillText("Safe Area", 72, 60);

    // Draw parsed images
    const imgRe = /KAG\.show_image\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*(?:,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+))?(?:,\s*([\d.]+))?(?:,\s*([\d.]+))?/g;
    let m;
    while ((m = imgRe.exec(script)) !== null) {
      const layer = m[1];
      const file = m[2];
      const x = m[3] ? parseFloat(m[3]) : 0;
      const y = m[4] ? parseFloat(m[4]) : 0;
      const s = m[5] ? parseFloat(m[5]) : 1;
      const a = m[6] !== undefined ? parseFloat(m[6]) : 1;

      const colors = { bg: "#3a5a8a", fg: "#5a3a8a", msg: "#3a8a5a" };
      ctx.globalAlpha = a * 0.6;
      ctx.fillStyle = colors[layer] || "#444";
      ctx.fillRect(x, y, 200 * s, 300 * s);
      ctx.globalAlpha = 1;

      ctx.strokeStyle = colors[layer] || "#888";
      ctx.lineWidth = 2;
      ctx.strokeRect(x, y, 200 * s, 300 * s);

      ctx.fillStyle = "#fff";
      ctx.font = "11px sans-serif";
      const label = file.split("/").pop();
      ctx.fillText(`[${layer}] ${label}`, x + 4, y + 16);
      ctx.font = "9px sans-serif";
      ctx.fillStyle = "#aaa";
      ctx.fillText(`${x},${y} ×${s} α${a}`, x + 4, y + 30);
    }

    // Draw text bubbles
    const txtRe = /KAG\.show_text\s*\(\s*"((?:[^"\\]|\\.)*)"\s*\)/g;
    while ((m = txtRe.exec(script)) !== null) {
      const text = m[1].substring(0, 40) + (m[1].length > 40 ? "..." : "");
      ctx.fillStyle = "rgba(0,0,0,0.8)";
      ctx.fillRect(40, h - 120, w - 80, 80);
      ctx.strokeStyle = "#666";
      ctx.lineWidth = 1;
      ctx.strokeRect(40, h - 120, w - 80, 80);
      ctx.fillStyle = "#fff";
      ctx.font = "13px sans-serif";
      ctx.fillText(text, 56, h - 90);
    }

    // Resolution label
    ctx.fillStyle = "#333";
    ctx.font = "10px monospace";
    ctx.fillText(`${w}×${h}`, w - 70, h - 6);
  }, [script, state.stageGrid, state.resolution, livePreview, previewFrame]);

  useEffect(() => { draw(); }, [draw]);

  const handleDrop = (e) => {
    e.preventDefault();
    const data = e.dataTransfer.getData("text/plain");
    if (!data) return;
    const rect = canvasRef.current.getBoundingClientRect();
    const scaleX = state.resolution.w / rect.width;
    const scaleY = state.resolution.h / rect.height;
    const x = Math.round((e.clientX - rect.left) * scaleX / GRID_SIZE) * GRID_SIZE;
    const y = Math.round((e.clientY - rect.top) * scaleY / GRID_SIZE) * GRID_SIZE;

    let layer = "fg";
    if (data.includes("bg/")) layer = "bg";
    else if (data.includes("char/")) layer = "fg";

    const line = `    KAG.show_image("${layer}", "${data}", ${x}, ${y}, 1, 1, 0)\n`;
    const s = state.script;
    const insertPos = s.lastIndexOf("end\n") > 0 ? s.lastIndexOf("end\n") : s.length;
    const newScript = s.slice(0, insertPos) + line + s.slice(insertPos);
    dispatch({ type: "SET_SCRIPT", payload: newScript });
  };

  return (
    <div className="stage-container" onDragOver={(e) => e.preventDefault()} onDrop={handleDrop}>
      <canvas ref={canvasRef} className="stage-canvas"
        style={{ maxWidth: "100%", maxHeight: "100%", transform: `scale(${state.stageZoom})` }}
      />
      <div className="stage-toolbar">
        <button className={state.stageGrid ? "active" : ""} onClick={() => dispatch({ type: "TOGGLE_GRID" })}>Grid</button>
        <button onClick={() => dispatch({ type: "SET_ZOOM", payload: Math.max(0.25, state.stageZoom - 0.25) })}>−</button>
        <span style={{ color: "#666", fontSize: 10, padding: "0 4px" }}>{Math.round(state.stageZoom * 100)}%</span>
        <button onClick={() => dispatch({ type: "SET_ZOOM", payload: Math.min(2, state.stageZoom + 0.25) })}>+</button>
        <button onClick={() => { dispatch({ type: "SET_ZOOM", payload: 1 }); const nextRes = state.resolution.w === 1280 ? { w: 1920, h: 1080 } : { w: 1280, h: 720 }; dispatch({ type: "SET_RESOLUTION", payload: nextRes }); }}>
          {state.resolution.w}×{state.resolution.h}
        </button>
        <button
          className={livePreview ? "active live-preview" : ""}
          onClick={() => setLivePreview(!livePreview)}
          title="Toggle live engine preview (200ms polling)"
        >{livePreview ? "■ Live" : "▶ Preview"}</button>
      </div>
    </div>
  );
}