# Caesura (AmeKAG) — 交接文档（2026-08-12 迭代轮）

> 面向后续 agent 的完整上下文。本轮从"代差路线图"（
> `docs/plans/2026-08-12-004-generation-gap-roadmap.md`）出发，完成
> 路线图全部可闭环战役（1c/1d 因范围收敛转 ⏳ 待后续，见 §3）。**先读
> AGENTS.md（模块边界铁律）+ 本文件 + 路线图文档**。

## 1. 本轮成果（15 个提交，自 74e465db 起）

### 路线图战役交付

| 提交 | 战役 | 内容 |
|---|---|---|
| `3f3646f2` | 1b 字节码持久化 | `.ksc` 编译缓存（Lua-literal 格式，5ms/2500token，JSON 7× 提速）；FNV-1a hash 失效；cache/ksc 隔离；sandbox 容错 |
| `dfb254f5` | 2a LSP 服务 | `kag/lsp.lua`（completion/hover/diagnostics，78 契约驱动）+ Monaco providers + eval 桥接（零 C++ 改动） |
| `bca6b925` | 2b 类型深化 | schema 新增 list/enum/file 类型 + storage 路径交叉验证；7 处生产契约升级 |
| `42fbd306` | 2c ks_check 对齐 | lsp.diagnostics = ks_check 全校验（表达式编译/截断/未知命令）+ 宏感知修复 |
| `f1f977ac` | 3a/3b/3c 确定性 | `kag/determinism.lua`（无 GPU 场景执行+状态断言）+ xorshift 随机 fuzz（200 场景零崩溃） |
| `252e6b1a` | 4b 可视化编辑 | Explorer 拖拽 → VisualView 生成 `[bg]`/`[playbgm]`；SceneTree 解析 .ks 点击跳转 Monaco |
| `f7482b2c` | 4c AI 创作 | `kag/aiwriter.lua`（生成对话/续写场景/sanitize/降级）+ IDE AiPanel |
| `e31e3437` + `1e6ce1a5` | 4d E-mote 替代 | SMA 设计定稿 + **S1 接口实现**（IMeshRenderer + Null 后端 + BackendRegistry，C++ 609/609） |
| `c9c4af0d` | 5d CARC 导入 | carc_pack list/extract 子命令 + kag3_import --carc 模式 |
| `d8b59ba4` | P2-8 教程 | `kag-language-tour.md`（13 章）+ demo_tutorial.ks + **附带修复 tokenizer 多裸参数丢失 + ks_check 宏误报** |
| `799fa03b` | P0-3 移动管线 | `android_build.sh` + `mobile-pipeline.md`（IME 对接/接口签名核对） |
| `be763458` | 5a 字节码预烘焙 | `ks_bake.lua`（737ms→25ms 29× 首载加速） |
| `74e465db` | 4a 壳 + 路线图 | Electron 主进程（自动拉起引擎 + /api 代理）+ 代差路线图文档 |
| `09d8cb71` | 4a CJS 修复 | Electron 主进程 CommonJS 语法 + package.json 入口 |

### 测试基线（本轮末实测）

| 套件 | 接手时 | 现在 |
|---|---|---|
| Lua | 103/103 | **112/112**（+9 新测试文件：bytecode_cache/carc_import/ks_bake/lsp/schema_types/determinism/fuzz/aiwriter/tutorial_scene） |
| C++ doctest | 605/605 | **609/609**（+4：test_mesh_renderer） |
| ctest | 10/10 | 10/10 |
| 耦合门禁 | PASS | PASS |

### 顺手修复的现存 bug（3 个）

1. **`[stop]` 分支缺失**（早前轮次）：flow_commands 声明但 scheduler 无分支 → 渲染成文本；已补 `elseif cmd == "stop" then return` + 回归测试
2. **tokenizer.parse_with_offsets 丢多裸参数**（d8b59ba4 附带）：`[set f.hp 80]` 丢 "80"（one_token Ct 嵌套）；ks_check 误报缺 value
3. **ks_check 宏调用误报**（d8b59ba4 附带）：`[macro]` 定义后的调用被当 unknown；扫描宏定义放行

## 2. 架构要点（本轮变化）

- **编译式指令流深化**：compiler.serialize/deserialize（Lua-literal chunk）+ writeCache/readCache（结果缓存 key=path,size,head）+ hashFile（全量算，正确性优先——曾引入缓存因顺序 bug 回退）
- **语言服务**：`kag/lsp.lua` + `kag/aiwriter.lua` 均经 `/api/eval` 暴露（sandbox require 白名单靠 `kag/init.lua` 预加载）
- **确定性测试**：`kag/determinism.lua` 纯 Lua 驱动 scheduler（mock kag 须在 compile 前替换使 handlers 绑定 mock）
- **SMA 接口**：`src/render/api/IMeshRenderer.h`（POD + 纯虚）+ NullMeshRenderer + BackendRegistry（DEF_GETTER/SETTER 宏）
- **CARC 只存路径 SHA-256 哈希**（不存明文名）→ extract 需已知相对路径（保持正斜杠）

## 3. 剩余项（按可闭环性）

| 项 | 约束 | 说明 |
|---|---|---|
| **1c 表达式 AOT** | 无（本机可闭环） | 表达式运行时 load() → 编译期 string.dump 字节码缓存；路线图 ⏳ |
| **1d 宏编译期展开** | 无（本机可闭环） | 参数化宏编译期内联（运行时零 splice）；路线图 ⏳ |
| SMA S2-S5 | 无（本机可闭环） | CPU 软变形渲染实现 + Lua 驱动 + 命令集 |
| 4a Electron 运行时验证 | 网络 | Electron 二进制 postinstall 下载超时；主进程 `node --check` 已过 |
| P1-6 Live2D GL/Steam | 硬件 | GL 需 Linux/macOS；Steam 需开发账号 |
| P0-1 Metal | 硬件 | macOS 实机 |
| P0-3 移动真机验证 | 设备 | Android 构建脚本已交付；APK 真机验证待设备 |
| P2-8 教程扩展 | 无 | 已交付入门级；可扩高级教程 |

## 4. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → CaesuraTests ≥609 → Lua ≥112 → ctest ≥10/10 → 耦合 PASS →
benchmark 无退化（tokenizer ≤52ms/1000tok、scheduler ≥308k tok/s、表达式 ≤165ms/400-if）。

## 5. 注意事项

- Lua 套件**顺序敏感**：写文件的测试（bytecode_cache/carc_import/ks_bake/
  tutorial_scene）必须在 `test_sandbox` 前（sandbox 禁 io 写与 os.execute）
- `cache/`、`tmp/` 已在 .gitignore；运行期 .ksc 缓存放 cache/ksc/
- 编辑器前端 `editor/`：npm 构建（tsc+vite）；Monaco LSP 经 eval 桥接；
  Electron 壳在 `editor/electron/main.cjs`
- 历史交接：`2026-08-02-002-delivery-handoff.md`（008）为上一权威状态
