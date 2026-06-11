import { useState } from "react";

const FOLDERS = [
  { name: "bgm", icon: "\u266B", files: ["bgm_01.ogg", "bgm_02.ogg"] },
  { name: "bg", icon: "\u25A0", files: ["classroom.png", "street.png", "room_night.png"] },
  { name: "chara", icon: "\u263A", files: ["hero.png", "heroine.png", "sensei.png"] },
  { name: "voice", icon: "\u266A", files: ["line_001.ogg", "line_002.ogg"] },
  { name: "video", icon: "\u25B6", files: ["opening.mp4"] },
  { name: "effect", icon: "\u2726", files: ["sparkle.png", "dust.png"] },
  { name: "font", icon: "F", files: ["NotoSansSC.ttf", "NotoSerifSC.ttf"] },
];

export default function AssetPanel() {
  const [openFolders, setOpenFolders] = useState(new Set(["bg", "bgm"]));
  const [selected, setSelected] = useState(null);
  const [search, setSearch] = useState("");

  const toggleFolder = (name) => {
    setOpenFolders(prev => {
      const next = new Set(prev);
      if (next.has(name)) next.delete(name); else next.add(name);
      return next;
    });
  };

  const filtered = search
    ? FOLDERS.map(f => ({ ...f, files: f.files.filter(fn => fn.toLowerCase().includes(search.toLowerCase())) })).filter(f => f.files.length > 0)
    : FOLDERS;

  return (
    <div className="asset-section">
      <input className="asset-search" placeholder="\u641C\u7D22\u7D20\u6750..." value={search}
        onChange={(e) => setSearch(e.target.value)} />
      {filtered.map(f => (
        <div key={f.name} className={`tree-folder${openFolders.has(f.name) ? " open" : ""}`}>
          <div className="tree-folder-header" onClick={() => toggleFolder(f.name)}>
            <span className="folder-arrow">{'\u25B6'}</span>
            <span>{f.icon}</span>
            <span>{f.name}</span>
            <span style={{ marginLeft: "auto", fontSize: "var(--text-2xs)", color: "var(--vscode-descriptionForeground)" }}>
              {f.files.length}
            </span>
          </div>
          <div className="tree-folder-children">
            {f.files.map(fn => (
              <div key={fn}
                className={`tree-item${selected === fn ? " selected" : ""}`}
                onClick={() => setSelected(fn)}
                draggable
                onDragStart={(e) => { e.dataTransfer.setData("text/plain", f.name + "/" + fn); }}
              >
                <span className="tree-item-icon">{'\u25C6'}</span>
                <span>{fn}</span>
              </div>
            ))}
          </div>
        </div>
      ))}
    </div>
  );
}