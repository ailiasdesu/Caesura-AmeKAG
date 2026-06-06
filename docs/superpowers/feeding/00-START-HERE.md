# Caesura (AmeKAG) — 投喂文件索引

> 每次开启新的 Codex 开发会话时，按以下顺序投喂文件。

---

## 每次会话必投 (建立上下文)

| 顺序 | 文件 | 大小 | 说明 |
|:---:|------|:---:|------|
| 1 | `context/CONTEXT_ANCHOR.md` | 5.8KB | 核心约束速查 — 架构不变式 + 64 项决策索引 + 绝对不做的 10 件事 |
| 2 | `spec/Caesura_功能实现规格说明书_整合版.md` | 55.3KB | 完整规格说明书 — Part 0-11, 64 项设计决策 |

---

## 按开发阶段投喂

### 第一阶段: 骨架搭建 (不可跳过)

| 顺序 | 文件 | 大小 | 耗时 |
|:---:|------|:---:|:---:|
| 3 | `phases/00-project-structure.md` | 6.9KB | 5min |
| 4 | `phases/01-phase-0-skeleton.md` | 22.8KB | 2-4h |

**验收**: `./caesura.exe` → SDL3 窗口 + bgfx 彩色三角形 + Lua print 输出

---

### 第二阶段: 统一抽象层 (强制依赖)

| 顺序 | 文件 | 大小 | 耗时 |
|:---:|------|:---:|:---:|
| 5 | `phases/02-phase-0.5-backend-api.md` | 22.2KB | 3-5h |

**验收**: Lua 侧 `backend.load_image()` 返回合法 TextureHandle, `backend.is_valid()` 可调用

---

### 第三阶段: 四路并行 — 同时开 4 个子代理 ⚡

| 子代理 | 文件 | 大小 | 耗时 |
|:---:|------|:---:|:---:|
| Agent A | `phases/03-phase-1-script-engine.md` | 9.2KB | 4-6h |
| Agent B | `phases/04-phase-2-render-pipeline.md` | 4.2KB | 4-6h |
| Agent C | `phases/05-phase-3-audio-system.md` | 2.9KB | 2-3h |
| Agent D | `phases/09-phase-7-carc-container.md` | 5.7KB | 3-5h |

---

### 第四阶段: KAG 集成点 (需 Phase 1+2+3)

| 顺序 | 文件 | 大小 | 耗时 |
|:---:|------|:---:|:---:|
| 6 | `phases/06-phase-4-kag-tags.md` | 4.4KB | 4-6h |

---

### 第五阶段: 高级渲染 + 存档 (可并行)

| 子代理 | 文件 | 大小 | 耗时 |
|:---:|------|:---:|:---:|
| Agent A | `phases/07-phase-5-advanced-render.md` | 6.1KB | 5-8h |
| Agent B | `phases/08-phase-6-save-i18n.md` | 3.6KB | 3-4h |

---

### 第六阶段: 开发工具 (需全部 Phase 1-7)

| 顺序 | 文件 | 大小 | 耗时 |
|:---:|------|:---:|:---:|
| 7 | `phases/10-phase-8-dev-tools.md` | 5.5KB | 4-6h |

---

### 第七阶段: 安全与平台 (收尾)

| 顺序 | 文件 | 大小 | 耗时 |
|:---:|------|:---:|:---:|
| 8 | `phases/11-phase-9-security-platform.md` | 4.3KB | 2-4h |

---

## 参考文件 (非必须，按需查阅)

| 文件 | 说明 |
|------|------|
| `phases/12-appendix-a-dependencies.md` | Phase 间依赖关系图 |
| `phases/13-appendix-b-acceptance.md` | 各 Phase 独立验收标准 |
| `phases/14-appendix-c-codex-feeding.md` | Codex 投喂策略建议 |
| `2026-06-06-caesura-implementation.md` | 完整实现计划 (104KB) — 当需要全局视角时查阅 |

---

## 投喂模板 (复制粘贴用)

```
会话开始时:
  1. 读 feeding/context/CONTEXT_ANCHOR.md，记住所有核心约束
  2. 读 feeding/spec/Caesura_功能实现规格说明书_整合版.md，理解完整规格
  3. 读 feeding/phases/XX-phase-X-xxx.md，开始实现此 Phase
```

---

## 目录结构

```
feeding/
├── 00-START-HERE.md           ← 你正在看的文件
├── context/
│   └── CONTEXT_ANCHOR.md     ← 每次必投 (5.8KB)
├── spec/
│   └── ...说明书.md          ← 每次必投 (55.3KB)
├── phases/
│   ├── 00-project-structure.md
│   ├── 01-phase-0-skeleton.md
│   ├── 02-phase-0.5-backend-api.md
│   ├── 03-phase-1-script-engine.md
│   ├── 04-phase-2-render-pipeline.md
│   ├── 05-phase-3-audio-system.md
│   ├── 06-phase-4-kag-tags.md
│   ├── 07-phase-5-advanced-render.md
│   ├── 08-phase-6-save-i18n.md
│   ├── 09-phase-7-carc-container.md
│   ├── 10-phase-8-dev-tools.md
│   ├── 11-phase-9-security-platform.md
│   ├── 12-appendix-a-dependencies.md
│   ├── 13-appendix-b-acceptance.md
│   └── 14-appendix-c-codex-feeding.md
└── 2026-06-06-caesura-implementation.md  ← 完整计划 (104KB)
```
