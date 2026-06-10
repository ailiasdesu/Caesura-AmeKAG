import { useState } from "react";
import { useEditor } from "../context/EditorContext";

// =========================================================================
// Settings categories (VSCode-style: left nav + right content)
// =========================================================================
const CATEGORIES = [
  {
    id: "ai",
    icon: "🤖",
    label: "AI 配置",
    groups: [
      {
        title: "AI 后端",
        settings: [
          {
            key: "aiProvider",
            type: "select",
            label: "AI 后端",
            desc: "选择 AI 服务提供商",
            options: [
              { value: "openai", label: "OpenAI" },
              { value: "codex", label: "Codex (本地 IDE)" },
              { value: "custom", label: "自定义端点" },
            ],
          },
        ],
      },
      {
        title: "OpenAI",
        settings: [
          { key: "openaiKey", type: "password", label: "API Key", desc: "OpenAI API 密钥 (sk-...)" },
          { key: "openaiModel", type: "select", label: "模型", desc: "聊天补全模型", options: [
            { value: "gpt-4o", label: "GPT-4o" },
            { value: "gpt-4o-mini", label: "GPT-4o Mini" },
            { value: "gpt-4-turbo", label: "GPT-4 Turbo" },
          ]},
          { key: "temperature", type: "range", label: "Temperature", desc: "生成随机性 (0=确定, 1=创意)", min: 0, max: 1, step: 0.1 },
        ],
      },
      {
        title: "自定义端点",
        settings: [
          { key: "customUrl", type: "text", label: "端点 URL", desc: "OpenAI-compatible API 地址" },
          { key: "customKey", type: "password", label: "API Key", desc: "自定义端点的 API 密钥" },
          { key: "customModel", type: "text", label: "模型名", desc: "模型标识符 (如 gpt-4o)" },
        ],
      },
    ],
  },
  {
    id: "editor",
    icon: "📝",
    label: "编辑器",
    groups: [
      {
        title: "字体",
        settings: [
          { key: "fontSize", type: "number", label: "字号", desc: "编辑器字体大小 (px)", default: 13, min: 10, max: 24 },
          { key: "fontFamily", type: "text", label: "字体族", desc: "CSS font-family 值", default: "'Cascadia Code', 'Fira Code', monospace" },
          { key: "lineHeight", type: "number", label: "行高", desc: "行高倍率", default: 1.6, min: 1.2, max: 2.5, step: 0.1 },
        ],
      },
      {
        title: "显示",
        settings: [
          { key: "tabSize", type: "number", label: "Tab 大小", desc: "空格数", default: 2, min: 1, max: 8 },
          { key: "wordWrap", type: "toggle", label: "自动换行", desc: "超出视口的行自动换行", default: true },
          { key: "lineNumbers", type: "toggle", label: "行号", desc: "显示行号", default: true },
          { key: "minimap", type: "toggle", label: "缩略图", desc: "显示代码缩略图（待实现）", default: false },
        ],
      },
    ],
  },
  {
    id: "stage",
    icon: "🎬",
    label: "舞台",
    groups: [
      {
        title: "舞台显示",
        settings: [
          { key: "stageGrid", type: "toggle", label: "显示网格", desc: "舞台上显示参考网格", default: true },
          { key: "stageSafeZone", type: "toggle", label: "安全区", desc: "显示安全区参考线", default: true },
          { key: "stageSnap", type: "toggle", label: "网格吸附", desc: "拖拽素材时吸附到网格", default: true },
        ],
      },
      {
        title: "默认分辨率",
        settings: [
          { key: "resolution", type: "select", label: "分辨率", desc: "舞台默认分辨率", options: [
            { value: "1280x720", label: "HD — 1280×720" },
            { value: "1920x1080", label: "Full HD — 1920×1080" },
            { value: "2560x1440", label: "QHD — 2560×1440" },
          ]},
        ],
      },
    ],
  },
  {
    id: "engine",
    icon: "⚙",
    label: "引擎",
    groups: [
      {
        title: "引擎集成",
        settings: [
          { key: "enginePath", type: "text", label: "引擎路径", desc: "自定义引擎可执行文件路径（留空使用内置）" },
          { key: "engineArgs", type: "text", label: "启动参数", desc: "引擎启动时的命令行参数" },
        ],
      },
      {
        title: "实时预览",
        settings: [
          { key: "autoReload", type: "toggle", label: "自动重载", desc: "保存后自动重新运行场景", default: false },
          { key: "previewScale", type: "select", label: "预览缩放", desc: "预览窗口缩放比例", options: [
            { value: "1", label: "100%" },
            { value: "0.75", label: "75%" },
            { value: "0.5", label: "50%" },
          ]},
        ],
      },
    ],
  },
  {
    id: "package",
    icon: "📦",
    label: "构建与打包",
    groups: [
      {
        title: "打包配置",
        settings: [
          { key: "packagePlatform", type: "select", label: "默认平台", desc: "打包目标平台", options: [
            { value: "win", label: "Windows" },
            { value: "mac", label: "macOS" },
            { value: "linux", label: "Linux" },
          ]},
          { key: "outputDir", type: "text", label: "输出目录", desc: "打包产物输出路径", default: "./release" },
          { key: "appName", type: "text", label: "应用名称", desc: "发布包中的应用程序名称" },
        ],
      },
      {
        title: "发布配置",
        settings: [
          { key: "appId", type: "text", label: "应用 ID", desc: "反向域名格式 (如 com.example.vn)" },
          { key: "version", type: "text", label: "版本号", desc: "语义化版本 (如 0.1.0)" },
        ],
      },
    ],
  },
];

// =========================================================================
// SettingsPage Component (VSCode-style: left categories + right content)
// =========================================================================
export default function SettingsPage({ onClose }) {
  const { state, dispatch } = useEditor();
  const [activeCategory, setActiveCategory] = useState("ai");
  const [searchQuery, setSearchQuery] = useState("");

  const category = CATEGORIES.find(c => c.id === activeCategory);
  const settings = state.aiSettings;

  const getValue = (key) => {
    // Map to aiSettings or editor state
    if (["openaiKey", "openaiModel", "customUrl", "customKey", "customModel", "temperature"].includes(key)) {
      return settings[key] ?? "";
    }
    if (key === "aiProvider") return state.aiProvider;
    if (key === "packagePlatform") return state.packagePlatform;
    return "";
  };

  const setValue = (key, value) => {
    if (["openaiKey", "openaiModel", "customUrl", "customKey", "customModel", "temperature"].includes(key)) {
      dispatch({ type: "SET_AI_SETTINGS", payload: { [key]: value } });
      try {
        const s = JSON.parse(localStorage.getItem("caesura-ai-settings") || "{}");
        s[key] = value;
        localStorage.setItem("caesura-ai-settings", JSON.stringify(s));
      } catch {}
    }
    if (key === "aiProvider") {
      dispatch({ type: "SET_AI_PROVIDER", payload: value });
      localStorage.setItem("caesura-ai-provider", value);
    }
    if (key === "packagePlatform") {
      dispatch({ type: "SET_PACKAGE_PLATFORM", payload: value });
    }
  };

  const handleApply = () => {
    // Settings are applied in real-time, just close
    onClose();
  };

  const renderSetting = (setting) => {
    const val = getValue(setting.key);

    switch (setting.type) {
      case "select":
        return (
          <select
            className="settings-select"
            value={val}
            onChange={e => setValue(setting.key, e.target.value)}
          >
            {setting.options.map(o => (
              <option key={o.value} value={o.value}>{o.label}</option>
            ))}
          </select>
        );

      case "toggle":
        return (
          <label className="settings-toggle">
            <input
              type="checkbox"
              checked={!!val}
              onChange={e => setValue(setting.key, e.target.checked)}
            />
            <span className="toggle-slider" />
          </label>
        );

      case "password":
        return (
          <input
            type="password"
            className="settings-input"
            value={val || ""}
            onChange={e => setValue(setting.key, e.target.value)}
            placeholder="••••••••"
            autoComplete="off"
          />
        );

      case "range":
        return (
          <div className="settings-range-wrap">
            <input
              type="range"
              className="settings-range"
              value={val || setting.default || 0}
              min={setting.min || 0}
              max={setting.max || 1}
              step={setting.step || 0.1}
              onChange={e => setValue(setting.key, Number(e.target.value))}
            />
            <span className="settings-range-val">{val || setting.default || 0}</span>
          </div>
        );

      case "number":
        return (
          <input
            type="number"
            className="settings-input"
            value={val || setting.default || 0}
            min={setting.min}
            max={setting.max}
            step={setting.step || 1}
            onChange={e => setValue(setting.key, Number(e.target.value))}
          />
        );

      default:
        return (
          <input
            type="text"
            className="settings-input"
            value={val || ""}
            onChange={e => setValue(setting.key, e.target.value)}
            placeholder={setting.default || ""}
          />
        );
    }
  };

  return (
    <div className="settings-page">
      {/* Settings header */}
      <div className="settings-header">
        <div className="settings-header-left">
          <span className="settings-title">⚙ 设置</span>
        </div>
        <div className="settings-search">
          <input
            type="text"
            placeholder="搜索设置..."
            value={searchQuery}
            onChange={e => setSearchQuery(e.target.value)}
            className="settings-search-input"
          />
        </div>
        <div className="settings-header-right">
          <button className="btn" onClick={onClose}>取消</button>
          <button className="btn btn-accent" onClick={handleApply}>确定</button>
        </div>
      </div>

      {/* Settings body: left nav + right content */}
      <div className="settings-body">
        <div className="settings-nav">
          {CATEGORIES.map(c => (
            <div
              key={c.id}
              className={`settings-nav-item${activeCategory === c.id ? " active" : ""}`}
              onClick={() => setActiveCategory(c.id)}
            >
              <span className="settings-nav-icon">{c.icon}</span>
              <span className="settings-nav-label">{c.label}</span>
            </div>
          ))}
        </div>

        <div className="settings-content">
          <div className="settings-content-header">
            <span className="settings-nav-icon">{category?.icon}</span>
            {category?.label}
          </div>

          <div className="settings-scroll">
            {category?.groups.map((group, gi) => (
              <div key={gi} className="settings-group">
                <div className="settings-group-title">{group.title}</div>
                {group.settings.map((setting, si) => (
                  <div key={si} className="settings-row">
                    <div className="settings-row-label">
                      <div className="settings-row-title">{setting.label}</div>
                      <div className="settings-row-desc">{setting.desc}</div>
                    </div>
                    <div className="settings-row-control">
                      {renderSetting(setting)}
                    </div>
                  </div>
                ))}
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* Footer */}
      <div className="settings-footer">
        <div className="settings-footer-left">
          修改即时生效 · 取消将丢弃更改
        </div>
        <div className="settings-footer-right">
          <button className="btn" onClick={onClose}>取消</button>
          <button className="btn btn-accent" onClick={handleApply}>确定</button>
        </div>
      </div>
    </div>
  );
}
