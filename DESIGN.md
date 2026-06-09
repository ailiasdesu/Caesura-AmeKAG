# Caesura Editor — Design System

> 视觉小说引擎可视化编辑器 · 暗色专业风格

## Brand Identity
**Caesura（休止符）** — 乐章中的停顿。编辑器服务于创作者在"停顿"中打磨作品。
关键词：专注、沉浸、专业、克制。

## Colors

| Token | Hex | Usage |
|-------|-----|-------|
| `--bg-root` | `#0a0a0f` | 最底层背景 |
| `--bg-primary` | `#111118` | 主面板背景 |
| `--bg-secondary` | `#16161f` | 次面板/卡片 |
| `--bg-tertiary` | `#1c1c28` | 输入框/选中态 |
| `--bg-hover` | `#222232` | 悬停态 |
| `--border` | `#2a2a3a` | 分割线/边框 |
| `--text-primary` | `#e0e0f0` | 主文字 |
| `--text-secondary` | `#8888aa` | 次要文字 |
| `--text-muted` | `#555570` | 禁用/占位 |
| `--accent` | `#6c5ce7` | 主强调色（紫） |
| `--accent-glow` | `#a78bfa` | 强调色发光 |
| `--success` | `#00d68f` | 成功/运行 |
| `--warning` | `#ffaa00` | 警告 |
| `--error` | `#ff4757` | 错误 |
| `--live2d` | `#ff6b9d` | Live2D 标识 |
| `--minigame` | `#00bcd4` | MiniGame 标识 |

## Typography
- 代码：`JetBrains Mono` / `Consolas`, 13px, 1.6 line-height
- UI 文字：`Inter` / system-ui, 12px base, 1.5 line-height
- 标题：`Inter` SemiBold, 14px panel headers
- 日文正文：`Noto Sans JP` fallback

## Spacing
- 面板间距：`8px` (tight), `12px` (default), `16px` (relaxed)
- 元素内边距：`6px 10px` (compact), `8px 14px` (normal)
- 圆角：`4px` (inputs), `6px` (cards), `8px` (panels)
- 图标尺寸：`14px` (inline), `18px` (toolbar)

## Component Tokens

| Component | Style |
|-----------|-------|
| Panel header | 14px SemiBold, `--text-primary`, `8px` bottom border `--border` |
| Button | `6px 14px`, `4px` radius, `--bg-tertiary` bg, hover→`--accent` |
| Button.active | `--accent` bg, `#fff` text |
| Input/Textarea | `--bg-tertiary` bg, `--border` border, `--text-primary`, monospace |
| Tab | `--text-secondary`, hover→`--text-primary`, active→`--accent` underline |
| Scrollbar | `6px` width, `--bg-tertiary` track, `--border` thumb |
| Toast/Log | `--bg-secondary`, `--border` left accent |
| Tooltip | `--bg-tertiary`, `--text-primary`, `6px 10px`, `4px` radius |
| Dialog | `--bg-primary`, `--border` border, `8px` radius, backdrop blur |

## Layout

```
┌─────────────────────────────────────────────────────┐
│  Toolbar                    Caesura Editor    — □ ✕ │ 36px
├──────────┬──────────────────────────┬───────────────┤
│  Assets  │                          │  Properties   │
│  Browser │      Stage Canvas        │  Panel        │
│  200px   │      flex:1             │  240px        │
│          │                          │               │
│          │                          │               │
├──────────┴──────────────────────────┴───────────────┤
│  Timeline (events)                                   │ 120px
├─────────────────────────────────────────────────────┤
│                                                     │
│  Code Editor (Lua)                                  │ flex:1
│                                                     │
├─────────────────────────────────────────────────────┤
│  Log Panel / AI Panel (tabbed)                      │ 160px
└─────────────────────────────────────────────────────┘
```

## States

| State | Visual |
|-------|--------|
| Engine disconnected | Stage: dim overlay "引擎未连接", Toolbar: `--error` dot |
| Preview running | Stage: `--success` border glow, Preview button solid `--success` |
| AI generating | AI Panel: shimmer skeleton, cursor pulse |
| Script error | Code Editor: `--error` gutter marker, Log: red entry |
| Drag over Stage | Stage: `--accent-glow` dashed border |

## Motion
- Panel resize: 150ms ease-out
- Tab switch: 120ms fade
- AI stream: character-by-character, 50ms easing
- Toast: slide-in from bottom, 300ms, auto-dismiss 3s
- No page-level transitions (SPA, instant)

## Accessibility
- WCAG AA contrast ratio (4.5:1 min)
- Keyboard: Tab navigation, Ctrl+S save, Ctrl+R run, F11 fullscreen
- Focus ring: `--accent-glow` 2px outline
- Screen reader: aria-labels on all interactive elements