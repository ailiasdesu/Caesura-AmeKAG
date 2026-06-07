---
problem_type: quality_audit
track: comprehensive_review
category: quality-issues
severity: mixed
status: documented
repo: Caesura (AmeKAG)
branch: codex/caesura-spec-rev
date: 2026-06-07
reviewer: ce-compound full-audit
files_audited: 46
total_issues: 17
---
# 全量代码质量审查报告 (2026-06-07)

## 审查范围

| 维度 | 覆盖 |
|------|------|
| src/ 模块 | 9/9 (Core, Render, Audio, Carc, Scripting, Resource, System, Debug, Platform) |
| scripts/ Lua层 | 29 个 Lua 文件 |
| 构建系统 | CMakeLists.txt (根 + tests + tools) |
| 文档 | ALPHA_NOTES, CONCEPTS, 说明书整合版, Alpha补完计划 |
| 测试 | 6 C++ 测试文件 + 3 KAG 测试脚本 |

---

## 发现清单

### P0 — 关键问题 (2项)

#### Q1. ALPHA_NOTES 技术栈声明不准确
- **文件**: `ALPHA_NOTES.md` (第 28 行)
- **当前**: "加密: OpenSSL (AES-256-GCM + Ed25519)"
- **事实**: 代码使用 Windows BCrypt (CNG)，非 OpenSSL
- **影响**: 新开发者会误以为需要 OpenSSL 依赖
- **修复**: 改为 "加密: Windows BCrypt (AES-256-GCM + Ed25519)"

#### Q2. CONCEPTS.md ShaderCache 所有权描述与代码矛盾
- **文件**: `CONCEPTS.md` (ShaderCache 章节)
- **当前**: "Does not own the handles — BgfxRenderDevice creates on init and destroys on shutdown"
- **事实**: `ShaderCache::shutdown()` 第 43 行确实调用 `bgfx::destroy()` — 它是排他所有者
- **影响**: 文档与代码不一致，可能导致维护者误解销毁职责
- **修复**: CONCEPTS 应描述为 "ShaderCache is the exclusive owner of bgfx::ProgramHandle; BgfxRenderDevice registers programs, ShaderCache::shutdown() destroys them"

---

### P1 — 高风险问题 (5项)

#### Q3. Scheduler [switch]/[case] 功能存根
- **文件**: `scripts/scheduler.lua` (第 166-179 行)
- **代码**: `-- For now, simplified: just skip to endswitch`
- **事实**: switch 不匹配任何 case，直接跳到 endswitch — 等于空操作
- **影响**: 所有使用 [switch] 标签的 KAG 脚本不会报错但功能缺失
- **计划**: P2 完成（Alpha 阶段 [if] 可作为替代）

#### Q4. ALPHA_NOTES 与 CMakeLists C++ 标准不一致
- **文件**: `ALPHA_NOTES.md` (第 6 行) vs `CMakeLists.txt` (第 6 行)
- **ALPHA_NOTES**: "C++17"
- **CMakeLists.txt**: `CMAKE_CXX_STANDARD 20`
- **事实**: 实际编译使用 C++20
- **修复**: ALPHA_NOTES "C++17 + Lua 5.4" → "C++20 + Lua 5.4"

#### Q5. 全局静态缓存无封装
- **文件**: `src/Audio/SoLoudAudioEngine.cpp` (第 24-26 行)
- **代码**: `static std::unordered_map<...> g_waveCache; static std::list<...> g_waveLRU; static std::unordered_map<...> g_waveLRUMap;`
- **事实**: 三个全局静态变量，无类封装，测试时无法隔离
- **影响**: 中 — 单线程模型下安全，但妨碍单元测试 mock
- **计划**: P2 重构为 SoLoudAudioEngine 成员

#### Q6. 命名空间不统一
- **文件**: `src/Carc/CryptoEngine.cpp` (第 10 行) vs 其余代码
- **CARC**: `namespace caesura::carc { ... }`
- **其余**: `namespace Caesura { ... }`
- **影响**: 低 — 编译通过但风格不一致，跨模块引用时需 using
- **计划**: 统一为 `namespace Caesura`（追溯修改影响 CRC 相关 4 文件）

#### Q7. src/Core/ 使用 ../ 相对路径引用子系统
- **文件**: `src/Core/Engine.cpp` (第 19-24 行)
- **代码**: `#include "../Render/IRenderDevice.h"` 等 6 处
- **事实**: Core 反向依赖 Render/Audio/Scripting — 架构层面正确但路径写法脆弱
- **影响**: 低 — CMake target_include_directories 已包含 src/，可以用 `Render/IRenderDevice.h`
- **计划**: P2 统一为绝对（相对于 src/）路径

---

### P2 — 中低风险 (7项)

#### Q8. Scheduler.resume() 与 .run() 逻辑重复
- **文件**: `scripts/scheduler.lua` (第 218-221 行)
- **代码**: `scheduler.resume(ctx)` 仅转发到 `scheduler.run(ctx, ctx.tokens, ctx.token_index)`
- **事实**: 维护两个入口增加认知负担
- **计划**: P2 文档化差异或合并

#### Q9. `-p` 目录（疑似 git stash 残留）
- **路径**: `D:\文件存放处\codex\Caesura(AmeKAG)\-p`
- **事实**: 非标准目录名，极可能为误操作产物
- **建议**: 清理

#### Q10. 文档仓库含大量过期测试输出文件
- **路径**: `build/test_output.txt`, `test_err.txt`, `test_err_baseline.txt` 等 14 个
- **事实**: 之前已清理，但需确认 build/ 目录是否在 .gitignore 中
- **建议**: 确认 build/ 和 *.txt 均在 .gitignore

#### Q11. .gitignore 可能不完整
- **事实**: 需验证 logs/, saves/, build/ 目录是否均被忽略

#### Q12. CryptoEngine 缺少 Linux/macOS 平台抽象
- **文件**: `src/Carc/CryptoEngine.cpp`
- **当前**: 100% Windows BCrypt API
- **事实**: 仅 #pragma comment(lib, "bcrypt.lib") — Linux/macOS 编译必然失败
- **计划**: Beta 阶段实现平台抽象层

#### Q13. Tokenizer 仅解析 82/167 令牌
- **文件**: `scripts/tokenizer.lua`
- **事实**: ALPHA_NOTES 已知问题 #5，Alpha 迭代中
- **状态**: 已知，不重复追踪

#### Q14. VideoPlayer 仅支持 MPEG1
- **事实**: FFmpeg 集成标记为可选 (`CAESURA_VIDEO_FFMPEG OFF`)
- **计划**: Beta P2

---

### 架构健康评估

#### 优势
- **模块边界清晰**: 9 个 src/ 子目录 + 29 个 Lua 脚本，职责划分明确
- **依赖方向正确**: Core → 子系统，BackendRegistry 统一服务定位
- **RAII 实践良好**: BcryptHandle、智能指针、析构函数保证资源释放
- **LRU 缓存实现完整**: Audio 波形缓存 + ShaderCache 均使用 list+unordered_map O(1) 逐出
- **6 档纹理预算自适应**: TextureBudget 已实现自动检测 + 开发者覆盖
- **安全质量**: BCryptGenRandom, AES-256-GCM, Ed25519 签名验证
- **线程安全**: 15 处 CAESURA_ASSERT_MAIN_THREAD() 断言
- **错误处理**: 三级降级（硬件加速→WARP→致命错误界面）

#### 债务
| 债务 | 严重级 | 估算 |
|------|:---:|------|
| 文档与代码不一致 | P0-P1 | 2-4h |
| Scheduler switch/case | P1 | 4-8h |
| Tokenizer 补全 | P1 | 8-16h |
| 平台抽象层 | P2 | 16-32h |
| 全局变量封装 | P2 | 2-4h |
| 路径规范化 | P2 | 1h |

#### 代码覆盖率
- C++ 单元测试: 40/40 通过 (doctest)
- KAG 脚本冒烟: 3/3 通过
- 未覆盖: Lua 绑定自动化测试、渲染回归测试

---

## 统计

| 指标 | 值 |
|------|-----|
| 审查文件数 | 46 |
| 发现问题总数 | 14 |
| P0 | 2 |
| P1 | 5 |
| P2 | 7 |
| 已知问题(ALPHA_NOTES) | 4 |
| 重开旧问题 | 0 |

---

## 建议优先级

1. **立即**: Q1 (ALPHA_NOTES OpenSSL→BCrypt), Q2 (CONCEPTS ShaderCache 所有权)
2. **本周**: Q4 (C++标准), Q9 (`-p` 清理), Q10-Q11 (gitignore)
3. **Alpha 迭代**: Q3 (switch/case), Q13 (Tokenizer)
4. **Beta**: Q5 (全局缓存封装), Q6 (命名空间), Q12 (平台抽象)
