# Contributing to Caesura (AmeKAG)

感谢你考虑为 Caesura (AmeKAG) 贡献代码！这是一份简明的贡献指南。
完整规则请以仓库根目录的 `AGENTS.md`（权威宪章）为准。

## 项目是什么

Caesura (AmeKAG) 是一个跨平台视觉小说引擎：C++20 + bgfx 渲染、
SDL3 窗口、SoLoud 音频、Lua 5.4 脚本。核心是 **KAG Neo-Genesis** ——
脱胎于 KAG3 的新一代现代化标签语法（保留开发者熟悉的 `[标签 参数=值]`
形态，语言核心与工具链完全现代化）。

## 快速开始

```bash
# Windows (MSVC)
cmake -B build -DCAESURA_LIVE2D=OFF
cmake --build build --config Debug --parallel

# macOS / Linux（FFmpeg 常不可用，关闭它）
cmake -B build -DCAESURA_LIVE2D=OFF -DCAESURA_ENABLE_FFMPEG=OFF
cmake --build build -j$(nproc)

# 测试（CWD 必须是 build/tests/Debug —— 资源路径相关）
cd build/tests/Debug && ./CaesuraTests.exe
ctest -C Debug --test-dir build --output-on-failure
```

## 模块边界（铁律摘要，完整见 AGENTS.md §1–4）

- 16 个内部模块（`src/archive` `audio` `debug` `di` `entry` `input` `job`
  `live2d` `minigame` `platform` `render` `resource` `rpc` `script` `steam`
  `storage`）通过 `api/` 子目录的纯虚接口（`I*.h`）对外暴露。
- **禁止模块间 include 具体实现头**；只允许 `I*.h`。
- 唯一例外：`src/entry/` + `src/main.cpp`（组合根）可以 new 具体后端。
- 所有后端访问必须走 `BackendRegistry`（`src/di/`），禁止绕过。

## 提交流程

1. **分支**：`codex/<描述>` 或 `feature/<描述>`。
2. **提交格式**：`type(scope): description`
   - types: `feat` `fix` `test` `docs` `review` `merge` `plan`
   - scopes: 模块名（`render` `script` `storage` `rpc` …）或层（`p1` `p2`
     `kag` `backend`）
   - 示例：`feat(palette): add day/night mode toggle`
3. **合并前必须**：
   - 全量构建零错误：`cmake --build build --config Debug --parallel`
   - `CaesuraTests.exe` 全绿（0 failed, 0 skipped）
   - 测试从 `build/tests/Debug/` 目录执行（CWD 匹配资源路径）
   - 新增功能带测试（doctest `tests/cpp/test_<module>.cpp` 或 Lua 套件
     `tests/scripts/test_*.lua`，注册进 `run_lua_tests.lua`）
   - 耦合门禁：`python scripts/count_coupling.py --ci` PASS
   - `git diff --check` 无空白错误

## 测试规范

- C++ 测试：doctest，`tests/cpp/test_<module>.cpp`，每个新模块至少一个
  用例（构造不崩溃 + 核心功能）。
- Lua 测试：`tests/scripts/test_*.lua`，注册进 `run_lua_tests.lua`
  （顺序敏感——sandbox 锁定 require，新测试放在 `test_sandbox` 之前）。
- 渲染测试不应在无窗口环境创建真实 GPU 资源（默认构造 + 访问器验证）。

## 文档约定

- 新 API 文档 → `docs/api/`；架构/设计 → `docs/design/`；
  使用指南 → `docs/guides/`；执行计划与记录 → `docs/plans/`
  （`YYYY-MM-DD-NNN-描述.md`）；可复用经验 → `docs/solutions/`。
- 一次性执行提示词禁止留在 `docs/`——执行完成后删除，仅保留总结。

## 代码风格

- clang-format（WebKit 风格，C++20，120 列，4 空格缩进，指针左对齐）：
  `clang-format -i src/path/to/file.cpp`
- Lua：遵循现有 `scripts/` 风格（4 空格缩进，`--` 注释）。
- 模块目录全小写；接口 `I` 前缀 + PascalCase；实现 PascalCase；
  命名空间 `Caesura::`。

## 示例游戏

`demo/example_game/` 是一个完整的小型 VN 示例（"The Last Letter"），
展示 KAG Neo-Genesis 的选择/结局/回滚/画廊/宏/插值特性。改剧本只需
编辑 `story.ks`；跑 `lua demo/example_game/entry.lua`。

## 问题反馈

- Bug / 功能请求：用 Issue 模板（`.github/ISSUE_TEMPLATE/`）。
- 代码变更：用 PR 模板（`.github/PULL_REQUEST_TEMPLATE.md`）。
- 安全相关：请直接联系维护者，勿公开披露。

感谢你的贡献！
