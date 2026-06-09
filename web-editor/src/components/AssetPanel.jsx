import { useEditor } from "../context/EditorContext";

export default function AssetPanel() {
  const { state } = useEditor();

  const images = state.assets.filter((a) => a.type === "image");
  const audio = state.assets.filter((a) => a.type === "audio");
  const scripts = state.assets.filter((a) => a.type === "script");

  const handleDragStart = (e, asset) => {
    e.dataTransfer.setData("text/plain", asset.path);
    e.dataTransfer.effectAllowed = "copy";
  };

  const Section = ({ title, items }) => (
    <>
      <div className="asset-section-title">{title} ({items.length})</div>
      {items.map((a) => (
        <div key={a.path} className="asset-item" draggable
          onDragStart={(e) => handleDragStart(e, a)}
          title={a.path}
        >
          {a.name}
        </div>
      ))}
    </>
  );

  return (
    <div className="panel-content">
      <Section title="Images" items={images} />
      <Section title="Audio" items={audio} />
      <Section title="Scripts" items={scripts} />
      {state.assets.length === 0 && (
        <div style={{ padding: 16, color: "#555", fontSize: 11, textAlign: "center" }}>
          No assets found.<br />Add files to the assets/ folder.
        </div>
      )}
    </div>
  );
}