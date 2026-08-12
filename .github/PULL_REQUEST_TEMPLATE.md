## 变更摘要
<!-- 用一句话描述这个 PR 做了什么 -->

## 关联 Issue
<!-- Closes #NNN -->

## 变更内容
- 

## 验证（合并前必须全部通过）
- [ ] 全量构建零错误：`cmake --build build --config Debug --parallel`
- [ ] `CaesuraTests.exe` 全绿（0 failed, 0 skipped，从 `build/tests/Debug/` 运行）
- [ ] Lua 套件全绿：`lua tests/scripts/run_lua_tests.lua`
- [ ] `ctest -C Debug --test-dir build --output-on-failure` 10/10
- [ ] 耦合门禁：`python scripts/count_coupling.py --ci` PASS
- [ ] `git diff --check` 无空白错误
- [ ] 新增功能带测试（doctest / Lua 套件）

## 模块边界合规
<!-- 勾选适用的项 -->
- [ ] 只通过 `api/` 接口跨模块（未 include 具体实现头）
- [ ] 后端访问走 `BackendRegistry`（未绕过）
- [ ] 未在非组合根位置 new 具体后端
- [ ] 模块目录全小写，命名遵循 `Caesura::` / `I` 前缀规范

## 附加上下文
<!-- 性能数据、设计决策、已知限制 -->
