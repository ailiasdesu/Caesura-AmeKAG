import { useEffect, useRef, useCallback } from "react";
import { useEditor } from "../context/EditorContext";
import { parseKAGScript } from "../parser/kagParser";

const PX_PER_SEC = 80;
const TOTAL_WIDTH = 6000; // ~75 seconds at 80px/s

export default function Timeline() {
  const { state, dispatch } = useEditor();
  const containerRef = useRef(null);

  // Parse script → timeline events
  useEffect(() => {
    const events = parseKAGScript(state.script);
    dispatch({ type: "SET_TIMELINE", payload: events });
  }, [state.script]);

  const events = state.timelineEvents;

  // Calculate cumulative time
  let cumTime = 0;
  const positions = events.map((ev) => {
    const x = cumTime * PX_PER_SEC;
    const w = Math.max(80, (ev.duration || 1000) * (PX_PER_SEC / 1000));
    cumTime += (ev.duration || 1000) / 1000;
    return { ...ev, x, w };
  });

  const rulerMarks = [];
  for (let s = 0; s < TOTAL_WIDTH / PX_PER_SEC; s += 5) {
    rulerMarks.push(s);
  }

  return (
    <div className="timeline-container" ref={containerRef}>
      <div className="timeline-ruler" style={{ width: TOTAL_WIDTH }}>
        {rulerMarks.map((s) => (
          <div key={s} className="timeline-ruler-mark" style={{ left: s * PX_PER_SEC }}>
            {s}s
          </div>
        ))}
      </div>
      <div className="timeline-track" style={{ width: TOTAL_WIDTH, position: "relative" }}>
        {positions.map((ev) => (
          <div
            key={ev.id}
            className={`timeline-event ${ev.type} ${state.selectedEvent?.id === ev.id ? "selected" : ""}`}
            style={{ left: ev.x, width: ev.w }}
            onClick={() => dispatch({ type: "SET_SELECTED", payload: ev })}
            title={`${ev.label} (at ${(ev.x / PX_PER_SEC).toFixed(1)}s)`}
          >
            {ev.label}
          </div>
        ))}
      </div>
    </div>
  );
}