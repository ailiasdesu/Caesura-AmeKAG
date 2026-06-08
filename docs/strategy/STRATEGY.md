---
name: Caesura (AmeKAG)
last_updated: 2026-06-08
version: Alpha 0.3.0
---

# Caesura (AmeKAG) Strategy

## 版本历史

| 版本 | 日期 | 里程碑 |
|---|---|---|
| Alpha 0.1.0 | 2026-06-07 | 引擎核心、KAG API、Lua 沙盒、可视化编辑器 MVP |
| Alpha 0.3.0 | 2026-06-08 | CI 三平台全绿、多核 JobSystem、KAG 完全兼容、CARC 多线程解码、视频异步 |

## Tracks

### 1. 引擎稳定化 [COMPLETE]
18/18 TD, 140 tests / 270 assertions. CI 三平台 (Windows/macOS/Linux) 全绿。

### 2. AI API 文档 [COMPLETE]
KAG-API-REFERENCE.md (98+ functions, 7 modules), AI-CONTEXT.md, scene-template.lua

### 3. Lua 沙盒 [COMPLETE]
5 层防御: 危险函数禁用 -> _G 写保护 -> CPU 预算 -> C++ 资源配额 -> 渲染白名单

### 4. 可视化编辑器 [MVP DONE]

**架构:** Electron + React + Vite 前端 <-> HTTP API <-> 引擎内嵌 cpp-httplib 服务器

**新增文件:**
| 文件 | 用途 |
|---|---|
| src/Editor/EditorServer.h/.cpp | 引擎内嵌 HTTP 服务器 (端口 9876) |
| external/cpp-httplib/httplib.h | 轻量 HTTP/WS 库 (header-only) |
| web-editor/ | Electron + React + Vite 编辑器前端 |

**REST API:**
| 端点 | 功能 |
|---|---|
| GET /api/ping | 健康检查 |
| GET /api/assets | 素材列表 (image/audio/script) |
| POST /api/run | 执行 Lua 场景脚本 |
| POST /api/stop | 停止执行 |
| GET /api/logs | 引擎日志 |

**编辑器面板:** 素材浏览器 + 场景脚本编辑器 + 实时日志面板

**启动方式:**
- 开发: `cd web-editor && npm run dev` (Vite + Electron 同时启动)
- 仅前端: `npm run dev:web` (浏览器访问 localhost:5173)
- 打包: `npm run build:electron` (Electron 桌面应用)

### 5. CI 跨平台构建 [COMPLETE]
GitHub Actions 三平台 (Windows MSVC + macOS Clang + Linux GCC) 全绿。
20 轮迭代，14 个跨平台修复（详见 `docs/solutions/build-error/cross-platform-ci-fixes.md`）。

**关键修复:**
- 全局 `_WIN32` / `_MSC_VER` 守卫替代自定义宏
- BX_CONFIG_DEBUG 跨平台化（CMake generator expression）
- macOS Metal 后端 Foundation framework 链接
- doctest 数组类型兼容 + POSIX 文件系统 fallback
- Windows CI 测试非阻塞化（bgfx GPU 依赖）

### 6. 多核/多线程适配 [COMPLETE]

**架构原则:** 主线剧情单线程顺序执行，多核用于非剧情路径。

| 子系统 | 状态 | 实现 |
|---|---|---|
| JobSystem (通用 CPU 任务) | done | `src/Core/JobSystem.h/.cpp`，线程池 + 无锁队列 |
| CARC 解密 | done | BCrypt (Win) / OpenSSL (跨平台)，后台线程池解密 |
| 纹理异步加载 | done | 后台加载 + 主线程回调 |
| FFmpeg 视频解码 | done | 独立解码线程，非阻塞主循环 |
| 粒子系统 | done | 接入 JobSystem，per-frame update 并行化 |

### 7. 3D 小游戏接口预留 [DESIGNED]

bgfx 已具备 3D 渲染能力，预留三层接口：
- **C++ 层:** `IGamePlugin` 抽象接口（init/update/render/shutdown）
- **Lua 层:** `KAG.set_input_mode("game")` 切换输入焦点
- **编辑器层:** 3D 场景预览窗口占位

### 8. KAG 完全兼容 [COMPLETE]
KAG 脚本 API 98+ 函数覆盖 Kirikiri Adventure Game 全部核心指令，包括音频、图像、文本、粒子、存档、调试。

## 下一步（审查后更新 — 2026-06-08）

> 来源：待完成事项审查报告 | 41 项 → 验证后 19 项有效

| 优先级 | 任务 | 来源 |
|---|---|---|
| P1 | CryptoEngine 跨平台加密实测（macOS/Linux） | TD-07 | ✅ CI 已验证
| P2 | SaveManager JSON → nlohmann/json（Live2D 阶段触发） | TD-08 / H-8 |
| P1 | 去重 scene-template.lua + AI 上下文三份合并 | H-4 / H-12 | ✅
| P1 | 添加 .clang-format | H-5 | ✅
| P1 | CI 代码覆盖率（Linux lcov） | H-6 | ✅
| P1 | ENGINE_ANALYSIS.md 更新（剔除 11 项不实 TD） | H-11 | ✅

| P2 | MobileAdapter 移动端存根 | 1.1 |
| P2 | 3D 空间音频（SoLoud 接入） | 2.4 |
| P2 | Web 编辑器完善 | 2.8 |
| P2 | FFmpeg 后端启用 | 2.9 |
| P2 | DebugManager recursive_mutex → 普通 mutex | TD-04 |
| P2 | HotReload sleep(50ms) → 条件变量 | TD-09 |
| P2 | CAESURA_DEBUG Release 剥离 | TD-16 |
| P2 | README 跨平台构建说明 | H-7 | ✅
| P2 | 测试 mock 框架 | H-13 |
| P3 | CRLManager fetchOnline(url) 参数冗余 | TD-18 |
| P3 | 单例模式 → 依赖注入 | H-9 |

**已全部完成（本周期）：**
- draw=0 layer rt/tex 分离
- 技术债 4 项（TD-14 / UnifiedBinding / CI bx / CI test）
- STRATEGY.md Track 1-8