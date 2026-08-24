# IME Text Input Bridge & Mobile Input Architecture

本文档定义 Caesura (AmeKAG) 引擎的输入法（IME）文本输入桥接与移动端虚拟键盘交互架构设计。

---

## 1. 设计目标与背景

视觉小说（Visual Novel）在以下核心场景依赖文本输入：
1. **玩家命名**：主角/配角姓名输入（支持中日韩常用输入法、全半角混合）。
2. **存档备注**：玩家自定义存档注释与标签。
3. **交互解谜/小游戏**：密码输入框、指令交互等。

在移动设备（Android / iOS）上，输入法依赖系统软键盘（On-Screen Keyboard），需解决键盘弹出遮挡、候选词组合状态（Composition/Preedit）显示、焦点矩形追踪（TextInputArea）等问题。

---

## 2. SDL3 文本输入生命周期

SDL3 提供了现代化的文本输入与 IME 管理 API：

- **`SDL_StartTextInput(SDL_Window* window)`**：激活软键盘或 IME 候选窗口。
- **`SDL_StopTextInput(SDL_Window* window)`**：关闭键盘与 IME。
- **`SDL_SetTextInputArea(SDL_Window* window, const SDL_Rect* rect, int cursor)`**：告知 OS 输入框在屏幕上的位置，OS 据此避免键盘遮挡输入框，并定位候选词悬浮窗。
- **`SDL_TextInputActive(SDL_Window* window)`**：查询当前输入法激活状态。

### 核心事件流

1. **`SDL_EVENT_TEXT_EDITING`**：输入法正在组合输入（未确认提交）。
   - 携带字段：`text` (UTF-8 组合串), `start` (选区起点), `length` (选区长度)。
   - UI 表现：输入框内以下划线或高亮预览待确认拼音/假名。
2. **`SDL_EVENT_TEXT_INPUT`**：输入法提交最终文本。
   - 携带字段：`text` (UTF-8 确认字符串)。
   - UI 表现：将确认字符追加至目标缓冲区，清空组合预览。

---

## 3. 接口设计 (`IPlatformBackend`)

遵循 `AGENTS.md` 铁律，在 `src/platform/api/IPlatformBackend.h` 中暴露纯虚接口：

```cpp
namespace Caesura {

struct TextInputRect {
    int x;
    int y;
    int w;
    int h;
    int cursorOffset;
};

class IPlatformBackend {
public:
    virtual ~IPlatformBackend() = default;
    
    // ... 原有接口 ...

    // IME & 软键盘控制
    virtual void startTextInput() = 0;
    virtual void stopTextInput() = 0;
    virtual void setTextInputArea(const TextInputRect& rect) = 0;
    virtual bool isTextInputActive() const = 0;
};

} // namespace Caesura
```

---

## 4. 事件路由与 Lua 绑定

1. **C++ 事件分发**：
   - `SDL3PlatformBackend::pollEvents` 捕获 `SDL_EVENT_TEXT_INPUT` 和 `SDL_EVENT_TEXT_EDITING`。
   - 分发至 `InputRouter::onTextInput(const char* text)` 与 `InputRouter::onTextEditing(const char* text, int start, int len)`。
2. **Lua 层暴露**：
   - `Platform.start_text_input(x, y, w, h)`
   - `Platform.stop_text_input()`
   - `Input.set_text_input_handler(callback)`
3. **KAG 命令对接**：
   - `[input name="f.player_name" default="Hero" max=12]` 命令挂载输入框组件，获得焦点时调用 `start_text_input`，失焦或提交时调用 `stop_text_input`。

---

## 5. 移动端视口自适应（避免键盘遮挡）

当软键盘在 Android / iOS 弹出时：
- Android Window 触发尺寸变更或 View Inset 调整。
- Caesura 渲染视口检测输入框底部坐标 `y + h` 是否高于 `screenHeight - keyboardHeight`。
- 若被遮挡，Logical Viewport 施加平滑垂直平移（`translateY = -(boxBottom - visibleBottom)`），键盘收起时自动回弹还原。

