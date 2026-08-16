# Scene Builder RPC 桥 — 资产发现与生成行校验（R108-A）

> 供 R108-A（编辑器 Scene Builder，拖拽生成 .ks 行）消费的引擎 RPC 复用说明。
> 结论：**不新增专用端点**。资产发现复用（已扩展）`GET /api/assets`；生成行校验复用既有
> LSP 通道（`/api/eval` + `kag.lsp.json()`），命令/参数契约校验能力完整，无需新增。

---

## 1. 资产发现

### 端点

```http
GET /api/assets?type={image|audio|script|bg|fg|bgm|se|voice|char|ui|scripts}
```

- `type` 省略 → 返回全部目录。
- `type` 可传**粗类别**（`image`/`audio`/`script`，历史兼容）或**目录槽位**
  （`bg`/`fg`/`bgm`/`se`/`voice` 等，Scene Builder 按槽位取资源）。
- 每项返回 `path`/`name`/`type`（粗类别）/`kind`（目录槽位）。

### 响应示例

```json
[
  {"path":"assets/bg/scene01.png","name":"scene01.png","type":"image","kind":"bg"},
  {"path":"assets/fg/girl_uniform.png","name":"girl_uniform.png","type":"image","kind":"fg"},
  {"path":"assets/bgm/theme.ogg","name":"theme.ogg","type":"audio","kind":"bgm"},
  {"path":"assets/voice/line01.ogg","name":"line01.ogg","type":"audio","kind":"voice"},
  {"path":"assets/se/click.ogg","name":"click.ogg","type":"audio","kind":"se"},
  {"path":"assets/scripts/chapter1.ks","name":"chapter1.ks","type":"script","kind":"scripts"}
]
```

### 扫描目录映射（引擎 C++ 侧 `std::filesystem` 直扫，可靠）

| type | 扫描目录 | kind |
|------|---------|------|
| `image` | `assets/bg/`, `assets/fg/`, `assets/char/`, `assets/ui/` | `bg`/`fg`/`char`/`ui` |
| `audio` | `assets/bgm/`, `assets/voice/`, `assets/se/` | `bgm`/`voice`/`se` |
| `script` | `assets/scripts/` | `scripts` |

### Editor 客户端

`editor/src/lib/rpc.ts` 的 `EngineClient.assets(type)` 已同步支持该契约；
`AssetEntry` 新增 `kind` 字段。Scene Builder 可直接：

```ts
const fg = await client.assets('fg')            // 前景精灵
const bgm = await client.assets('bgm')          // BGM 槽位
const bg = await client.assets('bg')            // 背景槽位
// fg[i].path 即 [ch name=... text=...] 需要的素材路径
```

### 为何不用 Lua fileutil.scan_dir

`scripts/fileutil.lua` 的 `FileUtil.scan_dir` 依赖 `lfs`（LuaFileSystem）或
`io.popen`/`io.open`，在 RPC eval 沙箱里不可靠（`io.open` 白名单仅
`scripts/assets/tests/demo` 前缀，且 sandbox 下 `io.popen` 受限）。引擎 C++ 侧
`/api/assets` 用 `std::filesystem::directory_iterator` 直扫进程 CWD，是已验证的
可靠路径。资产目录缺失时端点返回空数组（不报错）。

---

## 2. 生成行校验（命令/参数契约）

### 复用方式：`/api/eval` + `kag.lsp.json('diagnostics', text)`

LSP 模块（`scripts/kag/lsp.lua`）已内置完整契约校验，与运行时
`kag/schema.lua` 共享同一份声明式命令契约注册表（`schema.dumpContracts()`，
118 个命令）。编辑器通过 `/api/eval` 桥接（与现有 `lspCall()` 同路径）：

```lua
-- 校验一段生成的 .ks 文本（可整段或按行）
local lsp = require('kag.lsp')
return lsp.json('diagnostics', [[
[ch name="A" text="hi"]
[playbgm vol=1]
[blurrr oops=1]
]])
```

返回 `[{line, col, message, severity}]`，`severity: 1=error, 2=warning`
（Monaco MarkerSeverity 兼容，可直接上屏）：

| 校验项 | 示例触发 | severity |
|--------|---------|----------|
| 命令不存在 | `[blurrr oops=1]` → "unknown KAG command" | 2 |
| 必填参数缺失 | `[ch text=...]` 缺 `text=` 或 `[playbgm vol=1]` 无 file/storage | 1 |
| `_require_any` 违约 | `[playbgm vol=1]`（需 file 或 storage） | 1 |
| 未知参数 | `[blur duration=1]` 的拼错参数 → "unknown param"（KAG3 别名给出建议） | 2 |
| expression 编译失败 | `[if exp="f.hp >"]` → "does not compile" | 1 |
| `${...}` 插值失败 | `[ch text="a ${bad &&}"]` → "interpolation" | 1 |

### Editor 客户端用法（`evalRaw`）

```ts
const json = await client.evalRaw(
  "local lsp = require('kag.lsp'); return lsp.json('diagnostics', " +
  JSON.stringify(ksText) + ")",
)
const markers = JSON.parse(json)  // [{line,col,message,severity}]
```

### 增量语义（按生成行校验）

生成单行时逐行校验即可（tokenizer 处理单行文本稳定）；整行/整段一次校验性能
更好。已有 smoke 探针覆盖全部校验项（`tests/headless_rpc_smoke.py` round 57/71 组）。

### 实测：校验抓出 Scene Builder 生成行的契约违例（R108-A 必读）

对当前 `editor/src/lib/sceneBuilder.ts` 生成的 4 行做诊断，**抓出 1 处真实错误**：

```
[bg storage="bg/classroom.png"]
[csp storage="fg/girl_uniform.png" x=100 y=200]   ← [csp] missing required param 'name' (severity 1)
[ch name="Hero" text="Hello"]
[p]
```

`[csp]` 契约（`scripts/kag/commands/character.lua`）将 `name` 声明为
**必填（positional_index=1）**——它是角色 id / 图层键；`storage` 只是可选的文件覆盖。
正确写法：`[csp name="girl_uniform" storage="fg/girl_uniform.png" x=100 y=200]`。
因此 **`buildSpriteLine()` 应补 `name=`**（可用资产 file stem 派生）。这正是校验桥的价值：
生成即校验，错误在上屏前被标记。

---

## 3. 端到端接线图

```
Scene Builder (editor/)
  │
  ├─ 资产清单   GET /api/assets?type=fg|bg|bgm|se|voice
  │            EngineClient.assets(kind) → AssetEntry[]（含 kind）
  │
  └─ 行校验     POST /api/eval  body: lsp.json('diagnostics', ksText)
               → [{line,col,message,severity}]（Markers 上屏）
               （completion: lsp.json('completion', lineText) 可选用于输入提示）
```

## 4. 变更记录

- `src/rpc/EditorServer.cpp` — `/api/assets` 扩展：新增 `fg` 目录扫描；响应增加
  `kind` 字段；`type` 过滤同时接受粗类别与目录槽位（向后兼容）。
- `editor/src/lib/rpc.ts` — `AssetEntry.kind` 字段；`assets()` 类型参数扩展。
- `tests/cpp/test_rpc.cpp` — 新增 `EditorServer /api/assets lists scanned dirs with
  kind + type filters` 用例（26 断言）。
- 校验侧零改动（复用既有 LSP 通道）。
