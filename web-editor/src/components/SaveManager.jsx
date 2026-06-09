import { useEditor } from "../context/EditorContext";

const AUTO_SAVE = { id: "auto", label: "自动存档", special: "auto" };
const QUICK_SAVE = { id: "quick", label: "快速存档", special: "quick" };
const MANUAL_SLOTS = Array.from({ length: 8 }, (_, i) => ({ id: `slot-${i + 1}`, label: `存档 ${i + 1}` }));

const ALL_SLOTS = [AUTO_SAVE, QUICK_SAVE, ...MANUAL_SLOTS];

export default function SaveManager() {
  const { state, dispatch } = useEditor();

  if (!state.showSaveManager) return null;

  const close = () => dispatch({ type: "SET_SHOW_SAVE_MANAGER", payload: false });

  return (
    <div className="modal-overlay" onClick={close}>
      <div className="modal" onClick={e => e.stopPropagation()} style={{ minWidth: 520 }}>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 12 }}>
          <h2 style={{ fontSize: 14, fontWeight: 700 }}>💾 存档管理</h2>
          <button className="btn btn-sm" onClick={close}>✕</button>
        </div>

        <div className="save-grid">
          {ALL_SLOTS.map(slot => (
            <div key={slot.id} className="save-slot"
              style={slot.special ? { borderColor: "var(--accent)", background: "var(--accent-ghost)" } : {}}
              onClick={() => console.log(`Load: ${slot.id}`)}
              onContextMenu={e => { e.preventDefault(); console.log(`Context: ${slot.id}`); }}
            >
              <div className="slot-num">
                {slot.special === "auto" ? "⚡ 自动" : slot.special === "quick" ? "⚡ 快速" : slot.id}
              </div>
              <div className="slot-scene">空</div>
              <div className="slot-time">--:--:--</div>
            </div>
          ))}
        </div>

        <div style={{ display: "flex", gap: 8, justifyContent: "flex-end", marginTop: 12 }}>
          <button className="btn btn-sm">导出</button>
          <button className="btn btn-sm">导入</button>
          <button className="btn btn-accent btn-sm" onClick={close}>关闭</button>
        </div>
      </div>
    </div>
  );
}
