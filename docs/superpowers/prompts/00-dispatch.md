# 调度指令: FreeType + zstd 迁移

## 执行模式
按顺序执行两个独立任务。任务间无依赖，但建议顺序执行避免 CMake 冲突。

## 规范引用
工作目录: `D:\文件存放处\codex\Caesura(AmeKAG)`

开始前先读:
1. `docs/superpowers/CONTEXT_ANCHOR.md`
2. `docs/Caesura_功能实现规格说明书_整合版.md` Part 5.2（FreeType）和 CARC-1（zstd）

## Task 1: stb_truetype → FreeType
**提示词文件:** `docs/superpowers/prompts/task-01-freetype.md`
**优先级:** P0（立即执行）
**预计耗时:** 1 个 agent 会话

执行步骤:
1. 读提示词全文
2. 读 `src/Render/FontRenderer.h` 和 `src/Render/FontRenderer.cpp`
3. 实施修改
4. CMake 构建验证
5. 自查: rg "stb_truetype" src/Render/FontRenderer.* 必须返回空

## Task 2: miniz → zstd (CARC 模块)
**提示词文件:** `docs/superpowers/prompts/task-02-zstd.md`
**优先级:** P0（立即执行）
**预计耗时:** 1 个 agent 会话

执行步骤:
1. 读提示词全文
2. 读 `src/Carc/CARCReader.cpp` 和 `src/Carc/CARCWriter.cpp`
3. 实施修改（注意: XP3Archive.cpp 保留 miniz）
4. CMake 构建验证
5. 自查: rg "#include <miniz.h>" src/Carc/ 必须返回空

## 验收标准（最终）
- [ ] CMake configure + build 成功（Debug 和 Release）
- [ ] FontRenderer 使用 FreeType FT_* API（0 个 stb_truetype 调用）
- [ ] CARCReader/CARCWriter 使用 zstd ZSTD_* API（0 个 miniz 调用）
- [ ] XP3Archive 保留 miniz（XP3 格式兼容）
- [ ] 无链接错误（freetype + libzstd_static + miniz 同时链接）
