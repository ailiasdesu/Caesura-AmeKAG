import { useState, useEffect, useRef, useMemo } from "react";

// =========================================================================
// All available commands (VSCode-style command palette)
// =========================================================================
const COMMANDS = [
  // File
  { id: "file.new",     label: "新建文件",        category: "文件", shortcut: "Ctrl+N",    action: "newFile" },
  { id: "file.open",    label: "打开文件...",      category: "文件", shortcut: "Ctrl+O",    action: "openFile" },
  { id: "file.save",    label: "保存",            category: "文件", shortcut: "Ctrl+S",    action: "save" },
  { id: "file.saveAs",  label: "另存为...",        category: "文件", shortcut: "Ctrl+Shift+S", action: "saveAs" },
  { id: "file.saveAll", label: "全部保存",          category: "文件", shortcut: "",          action: "saveAll" },

  // Scene
  { id: "scene.run",    label: "运行场景",          category: "场景", shortcut: "F5",        action: "run" },
  { id: "scene.stop",   label: "停止运行",          category: "场景", shortcut: "Shift+F5", action: "stop" },
  { id: "scene.preview",label: "切换实时预览",       category: "场景", shortcut: "",          action: "togglePreview" },
  { id: "scene.add",    label: "添加场景",          category: "场景", shortcut: "",          action: "addScene" },
  { id: "scene.toggleGrid", label: "切换舞台网格",   category: "场景", shortcut: "",          action: "toggleGrid" },
  { id: "scene.toggleSafe", label: "切换安全区",     category: "场景", shortcut: "",          action: "toggleSafe" },

  // View
  { id: "view.stage",     label: "切换到舞台视图",      category: "视图", shortcut: "", action: "viewStage" },
  { id: "view.code",      label: "切换到代码编辑器",     category: "视图", shortcut: "", action: "viewCode" },
  { id: "view.live2d",    label: "切换到 Live2D 面板",  category: "视图", shortcut: "", action: "viewLive2D" },
  { id: "view.minigame",  label: "切换到 3D 小游戏面板", category: "视图", shortcut: "", action: "viewMiniGame" },
  { id: "view.explorer",  label: "显示资源管理器",       category: "视图", shortcut: "Ctrl+Shift+E", action: "showExplorer" },
  { id: "view.search",    label: "显示搜索",            category: "视图", shortcut: "Ctrl+Shift+F", action: "showSearch" },
  { id: "view.ai",        label: "显示 AI 助手",         category: "视图", shortcut: "Ctrl+Shift+I", action: "showAI" },
  { id: "view.live2dPanel", label: "显示 Live2D 面板",   category: "视图", shortcut: "Ctrl+Shift+L", action: "showLive2D" },
  { id: "view.minigamePanel", label: "显示 3D 小游戏面板", category: "视图", shortcut: "Ctrl+Shift+M", action: "showMiniGame" },

  // Panel
  { id: "panel.problems", label: "显示问题面板",     category: "面板", shortcut: "", action: "showProblems" },
  { id: "panel.output",   label: "显示输出面板",     category: "面板", shortcut: "", action: "showOutput" },
  { id: "panel.logs",     label: "显示调试控制台",    category: "面板", shortcut: "", action: "showLogs" },
  { id: "panel.ai",       label: "显示 AI 对话",     category: "面板", shortcut: "", action: "showAIPanel" },
  { id: "panel.toggle",   label: "切换底部面板",      category: "面板", shortcut: "Ctrl+J", action: "togglePanel" },

  // Build
  { id: "build.packageWin",  label: "打包 Windows 版本",  category: "构建", shortcut: "", action: "packageWin" },
  { id: "build.packageMac",  label: "打包 macOS 版本",    category: "构建", shortcut: "", action: "packageMac" },
  { id: "build.packageLinux",label: "打包 Linux 版本",    category: "构建", shortcut: "", action: "packageLinux" },
  { id: "build.exportCARC",  label: "导出 CARC 资源包...", category: "构建", shortcut: "", action: "exportCARC" },

  // Settings
  { id: "settings.open",  label: "打开设置",          category: "设置", shortcut: "Ctrl+,",  action: "openSettings" },
  { id: "settings.ai",    label: "AI 配置",           category: "设置", shortcut: "",        action: "settingsAI" },
  { id: "palette",        label: "显示所有命令",       category: "其他", shortcut: "Ctrl+Shift+P", action: "commandPalette" },

  // Help
  { id: "help.about",     label: "关于 Caesura...",   category: "帮助", shortcut: "", action: "about" },
  { id: "help.shortcuts", label: "键盘快捷方式参考",    category: "帮助", shortcut: "Ctrl+K Ctrl+S", action: "shortcuts" },
];

// =========================================================================
// CommandPalette Component
// =========================================================================
export default function CommandPalette({ onClose, dispatch, state }) {
  const [query, setQuery] = useState("");
  const [selectedIdx, setSelectedIdx] = useState(0);
  const inputRef = useRef(null);

  const filtered = useMemo(() => {
    if (!query.trim()) return COMMANDS;
    const q = query.toLowerCase();
    return COMMANDS.filter(c =>
      c.label.toLowerCase().includes(q) ||
      c.category.toLowerCase().includes(q) ||
      (c.shortcut && c.shortcut.toLowerCase().includes(q))
    );
  }, [query]);

  useEffect(() => {
    setSelectedIdx(0);
  }, [query]);

  useEffect(() => {
    inputRef.current?.focus();
  }, []);

  const execute = (cmd) => {
    onClose();
    switch (cmd.action) {
      case "newFile":
        dispatch({ type: "OPEN_FILE", payload: { name: `untitled-${Date.now()}.lua`, content: "" } });
        dispatch({ type: "SET_ACTIVE_PANEL", payload: "code" });
        break;
      case "save":
        dispatch({ type: "MARK_SAVED" });
        dispatch({ type: "ADD_LOG", payload: { level: "info", source: "editor", message: "File saved" } });
        break;
      case "run":
        // will be handled by parent via callback
        break;
      case "stop":
        break;
      case "viewStage":
        dispatch({ type: "SET_ACTIVE_PANEL", payload: "scene" });
        break;
      case "viewCode":
        dispatch({ type: "SET_ACTIVE_PANEL", payload: "code" });
        break;
      case "toggleGrid":
        dispatch({ type: "TOGGLE_GRID" });
        break;
      case "toggleSafe":
        dispatch({ type: "TOGGLE_SAFE" });
        break;
      case "addScene":
        break;
      case "togglePreview":
        dispatch({ type: "SET_PREVIEW", payload: !state.previewing });
        break;
      default:
        break;
    }
  };

  const handleKeyDown = (e) => {
    if (e.key === "Escape") { onClose(); return; }
    if (e.key === "ArrowDown") { e.preventDefault(); setSelectedIdx(i => Math.min(i + 1, filtered.length - 1)); return; }
    if (e.key === "ArrowUp") { e.preventDefault(); setSelectedIdx(i => Math.max(i - 1, 0)); return; }
    if (e.key === "Enter") {
      if (filtered[selectedIdx]) execute(filtered[selectedIdx]);
    }
  };

  return (
    <div className="command-palette-overlay" onClick={onClose}>
      <div className="command-palette" onClick={e => e.stopPropagation()}>
        <div className="command-palette-input-wrap">
          <span className="command-palette-prefix">{">"}</span>
          <input
            ref={inputRef}
            className="command-palette-input"
            placeholder="输入命令名称..."
            value={query}
            onChange={e => setQuery(e.target.value)}
            onKeyDown={handleKeyDown}
          />
        </div>
        <div className="command-palette-results">
          {filtered.length === 0 && (
            <div className="command-palette-empty">未找到匹配的命令</div>
          )}
          {filtered.map((cmd, i) => (
            <div
              key={cmd.id}
              className={`command-palette-item${i === selectedIdx ? " selected" : ""}`}
              onClick={() => execute(cmd)}
              onMouseEnter={() => setSelectedIdx(i)}
            >
              <span className="command-palette-label">{cmd.label}</span>
              <span className="command-palette-category">{cmd.category}</span>
              {cmd.shortcut && <span className="command-palette-shortcut">{cmd.shortcut}</span>}
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
