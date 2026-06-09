import { useEditor } from "../context/EditorContext";
import { useRef, useEffect } from "react";

const LEVELS = ["all", "info", "warn", "error", "debug"];

export default function LogPanel() {
  const { state, dispatch } = useEditor();
  const listRef = useRef(null);
  const autoScrollRef = useRef(true);

  useEffect(() => {
    if (autoScrollRef.current && listRef.current) {
      listRef.current.scrollTop = listRef.current.scrollHeight;
    }
  }, [state.logs]);

  const onScroll = () => {
    const el = listRef.current;
    if (!el) return;
    autoScrollRef.current = el.scrollHeight - el.scrollTop - el.clientHeight < 30;
  };

  let filtered = state.logs;
  if (state.logFilter !== "all") {
    filtered = filtered.filter(l => l.level === state.logFilter);
  }
  if (state.logSearch) {
    const q = state.logSearch.toLowerCase();
    filtered = filtered.filter(l => (l.message || "").toLowerCase().includes(q) || (l.source || "").toLowerCase().includes(q));
  }

  return (
    <div className="log-panel">
      <div className="log-toolbar">
        {LEVELS.map(lv => (
          <button key={lv}
            className={`log-filter${state.logFilter === lv ? " active" : ""}`}
            onClick={() => dispatch({ type: "SET_LOG_FILTER", payload: lv })}
          >
            {lv.toUpperCase()}
          </button>
        ))}
        <input
          className="log-search"
          placeholder="搜索日志..."
          value={state.logSearch}
          onChange={e => dispatch({ type: "SET_LOG_SEARCH", payload: e.target.value })}
        />
        <button className="btn btn-sm" onClick={() => dispatch({ type: "CLEAR_LOGS" })}>清除</button>
      </div>

      <div className="log-list" ref={listRef} onScroll={onScroll}>
        {filtered.length === 0 ? (
          <div style={{ textAlign: "center", color: "var(--fg-muted)", padding: 16 }}>
            暂无日志
          </div>
        ) : (
          filtered.map((l, i) => (
            <div key={i} className="log-entry">
              <span className="time">{l.time}</span>
              <span className={`level ${l.level}`}>{l.level}</span>
              <span className="source">{l.source || ""}</span>
              <span className="msg">{l.message}</span>
            </div>
          ))
        )}
      </div>
    </div>
  );
}
