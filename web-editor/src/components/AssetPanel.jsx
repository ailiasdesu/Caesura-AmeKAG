import { useEditor } from "../context/EditorContext";
import { useState } from "react";

const FOLDERS = [
  { name: "bg", label: "🎨 背景", ext: [".png", ".jpg"] },
  { name: "char", label: "👤 角色", ext: [".png"] },
  { name: "bgm", label: "🎵 音乐", ext: [".ogg", ".mp3"] },
  { name: "voice", label: "🎤 语音", ext: [".ogg", ".mp3"] },
  { name: "se", label: "🔔 音效", ext: [".ogg", ".wav"] },
  { name: "scripts", label: "📜 脚本", ext: [".lua"] },
  { name: "video", label: "🎬 视频", ext: [".mp4", ".webm"] },
];

export default function AssetPanel() {
  const { state } = useEditor();
  const [openFolders, setOpenFolders] = useState(["bg"]);
  const [search, setSearch] = useState("");

  const toggleFolder = (name) => {
    setOpenFolders(prev =>
      prev.includes(name) ? prev.filter(f => f !== name) : [...prev, name]
    );
  };

  const getFolderAssets = (folder) => {
    if (!state.assets || !Array.isArray(state.assets)) return [];
    return state.assets.filter(a => {
      const name = typeof a === "string" ? a : a.name || a.path || "";
      return name.toLowerCase().includes(folder.toLowerCase()) || name.startsWith(`assets/${folder}`);
    });
  };

  const handleDragStart = (e, asset) => {
    const name = typeof asset === "string" ? asset : asset.name || asset.path || "";
    e.dataTransfer.setData("text/plain", name);
    e.dataTransfer.effectAllowed = "copy";
    e.target.style.opacity = "0.5";
  };

  const handleDragEnd = (e) => {
    e.target.style.opacity = "1";
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
      <div className="panel-header">
        <span>📁 素材</span>
      </div>
      <div style={{ padding: "4px 8px" }}>
        <input
          placeholder="搜索素材..."
          value={search}
          onChange={e => setSearch(e.target.value)}
          style={{
            width: "100%", padding: "3px 8px", border: "1px solid var(--border)",
            borderRadius: "var(--radius-sm)", background: "var(--bg-input)",
            color: "var(--fg)", fontSize: 11,
          }}
        />
      </div>
      <div style={{ flex: 1, overflowY: "auto" }}>
        {FOLDERS.map(folder => {
          const isOpen = openFolders.includes(folder.name);
          const assets = getFolderAssets(folder);

          return (
            <div key={folder.name} className="asset-folder">
              <div className="asset-folder-header" onClick={() => toggleFolder(folder.name)}>
                <span className={`arrow${isOpen ? " open" : ""}`}>▶</span>
                <span>{folder.label}</span>
                <span style={{ fontSize: 9, color: "var(--fg-muted)", marginLeft: "auto" }}>
                  {assets.length > 0 ? assets.length : ""}
                </span>
              </div>
              {isOpen && (
                <div className="asset-folder-children">
                  {assets.length === 0 ? (
                    <div style={{ padding: "4px 12px", fontSize: 10, color: "var(--fg-muted)", fontStyle: "italic" }}>
                      (空)
                    </div>
                  ) : (
                    assets.map((a, i) => {
                      const name = typeof a === "string" ? a : a.name || a.path || `asset_${i}`;
                      return (
                        <div key={i} className="asset-item"
                          draggable
                          onDragStart={(e) => handleDragStart(e, a)}
                          onDragEnd={handleDragEnd}
                          title={name}
                        >
                          <span style={{ fontSize: 10 }}>📄</span>
                          <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                            {name.split("/").pop() || name}
                          </span>
                        </div>
                      );
                    })
                  )}
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}
