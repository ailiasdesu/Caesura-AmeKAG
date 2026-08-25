# KAG Neo-Genesis 统一语义分析层架构 (Unified Semantic Layer)

> **版本**：v1.0.0  
> **核心目标**：彻底消除多工具间的“解析漂移”（Semantic Drift），确立单一解析真相源。

---

## 1. 架构拓扑 (Single Source of Truth)

```
                       ┌─────────────────────────┐
                       │     KAG Source (.ks)    │
                       └────────────┬────────────┘
                                    │
                       ┌────────────▼────────────┐
                       │   LPeg Tokenizer Engine │
                       │ (scripts/tokenizer.lua) │
                       └────────────┬────────────┘
                                    │
                       ┌────────────▼────────────┐
                       │   Unified Semantic AST  │
                       │ (scripts/kag/semantic)  │
                       └────────────┬────────────┘
         ┌──────────────────────────┼──────────────────────────┐
         │                          │                          │
┌────────▼────────┐        ┌────────▼────────┐        ┌────────▼────────┐
│ Runtime Engine  │        │  Creator Tools  │        │ Analysis & IDE  │
│ (Compiler/VM)   │        │                 │        │                 │
├─────────────────┤        ├─────────────────┤        ├─────────────────┤
│ • compiler.lua  │        │ • Story Flow    │        │ • LSP (lsp.lua) │
│ • scheduler.lua │        │ • i18n Pipeline │        │ • ks_check.lua  │
│ • kag_runner    │        │ • CSV/PO Export │        │ • AI Dev Tools  │
└─────────────────┘        └─────────────────┘        └─────────────────┘
```

---

## 2. 统一语义模型 (Semantic Model Definition)

由 `scripts/kag/semantic.lua` 生成的标准 AST / 语义模型结构：

| 字段 | 类型 | 说明 |
|---|---|---|
| `file` | string | 源文件路径 |
| `nodes` | array | 结构化 AST 节点列表（包含精确 `line`, `col`, `offset`, `end_offset`） |
| `labels` | table | 跳转标签索引（`*name` -> `{ line, col, title, is_entry }`） |
| `jumps` | array | 跳转指令拓扑（`from`, `target`, `storage`, `condition`, `kind`） |
| `calls` | array | 子程序调用拓扑（`from`, `target`, `storage`, `line`） |
| `choices` | array | 玩家分支选项（`from`, `target`, `text`, `line`, `col`） |
| `endings` | array | 结局终结点（`label`, `name`, `line`） |
| `translatables`| array | 可翻译字符串集合（对话、选项、通知，附带确定性内容哈希 Key） |
| `diagnostics` | array | 静态契约违规、未定义跳转目标、死分支告警与未闭合代码块 |
| `flow_graph` | table | 图论流向图（节点 + 有向边，用于 Mermaid 与编辑器拓扑图） |

---

## 3. 各子系统迁移与复用状态矩阵 (Assessment Matrix)

| 工具 / 子系统 | 迁移前机制 | 迁移后机制 | 漂移消除状态 |
|---|---|---|:---:|
| **Story Flow** (`generate_story_flow.py`) | Python 自研局部 Regex | `kag/semantic.lua` 语义图导出 | 🟢 **100% 统一** |
| **i18n Extractor** (`extract_i18n.py`) | Python 自研局部 Regex | `kag/semantic.lua` AST 字符串提取 | 🟢 **100% 统一** |
| **KAG Compiler** (`kag/compiler.lua`) | `tokenizer.parse` | 原生共享 Tokenizer 与 Schema | 🟢 **原生统一** |
| **Language Server** (`kag/lsp.lua`) | `tokenizer.parse_with_offsets` | 原生共享 Tokenizer 与 Schema | 🟢 **原生统一** |
| **Contract Lint** (`ks_check.lua`) | `tokenizer.parse_with_offsets` | 原生共享 Tokenizer 与 Schema | 🟢 **原生统一** |
| **AI Assistant** (`kag/aidev.lua`) | JSON RPC / Schema API | 原生读取 Schema 契约元数据 | 🟢 **原生统一** |

---

## 4. 稳定性与兼容性保证

1. **零运行时依赖**：语义分析层纯 Lua 实现，完全由 `scripts/` 原生驱动，运行时与打包不强绑定外部环境。
2. **KAG3 语法全兼容**：继承 LPeg Tokenizer 对 KAG3 位置参数、属性别名（`elsif` 等）与行内代码块的支持。
3. **确定性多语言哈希**：`translatables` 采用 `<scene>:<scope>:<line>:<content_hash>` 确定性生成算法，保证同源提取多次生成的 Key 严格恒等。
