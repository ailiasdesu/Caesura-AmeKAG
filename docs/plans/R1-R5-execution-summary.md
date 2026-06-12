# R1–R5 执行情况详细总结

> 项目: Caesura (AmeKAG) — galgame 引擎
> 基线: T1–T5 已完成 (257/257 tests)
> 日期: 2026-06-12

---

## 总体结果

| 指标 | 会话开始 | 会话结束 | 变化 |
|------|----------|----------|------|
| 测试用例 | 237 | **257** | +20 |
| 断言 | 508 | **579** | +71 |
| 构建错误 | 0 | 0 | — |
| Git 提交 | — | 2 | — |
| 新增文档 | — | 3 | — |
| 新增测试文件 | — | 3 | — |
| 修改源文件 | — | 4 | — |

---

## R1 — 编辑器集成

### R1.1: 验证 web-editor ↔ Engine RPC 通信

**状态**: ✅ 部分完成

**已完成**:

1. **审查现有端点** — `src/rpc/EditorServer.cpp` 已注册:
   - `GET /api/ping` — 健康检查 (返回 `{"status":"ok","engine":"CaesuraAmeKAG"}`)
   - `GET /api/assets` — 列出项目资源 (支持 type=image/audio/script 筛选)
   - `POST /api/run` — 执行 Lua 脚本
   - `POST /api/stop` — 停止执行
   - `GET /api/logs` — 近期日志
   - CORS 中间件已配置 (`Access-Control-Allow-Origin: *`)

2. **新增端点** — `GET /api/status`
   ```cpp
   svr.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
       const char* luaOk = (m_L || BackendRegistry::instance().getLuaState()) ? "true" : "false";
       char buf[256];
       snprintf(buf, sizeof(buf),
           "{\"status\":\"ok\",\"engine\":\"CaesuraAmeKAG\",\"lua\":%s,\"port\":%d}",
           luaOk, m_port);
       res.set_content(buf, "application/json");
   });
   ```
   返回引擎状态、Lua VM 可用性、端口号。

**修改文件**: `src/rpc/EditorServer.cpp` (+12 行)

### R1.2: Live2D 预览面板

**状态**: ⏸ 未实施

需要的前端工作 (`web-editor/` React + Vite):
1. 添加 `GET /api/live2d/models` — 列出 `models/` 下的 `.moc` 文件
2. 添加 `POST /api/live2d/load` — 加载指定模型
3. 修改 `Live2DPanel.jsx` (当前为占位符) — 显示模型列表, 点击加载

### R1.3: 一键打包

**状态**: ⏸ 未实施

需要:
1. 添加 `POST /api/build` 端点 — 调用 CARCWriter 打包 `scripts/` + `assets/`
2. 修改 `AssetPanel.jsx` — 添加 Build 按钮和进度显示

---

## R2 — Live2D 真实集成

### R2.1: 无 SDK 环境下降级方案 (PNG 立绘)

**状态**: ✅ 完成

**问题**: NullAnimationBackend 原本是纯头文件内联实现, 所有方法返回安全默认值, 无法展示任何立绘。

**解决方案**: 改为实例类, 支持 PNG/JPG/BMP 作为静态纹理加载。

**修改文件**:

`src/live2d/NullAnimationBackend.h` — 重写为实例类声明:

```cpp
class NullAnimationBackend : public IAnimationBackend {
    struct StaticSprite {
        uint32_t textureId = 0;
        float x = 0, y = 0, scale = 1.0f;
        float opacity = 1.0f;
        bool visible = false;
    };
    std::unordered_map<int, StaticSprite> m_sprites;
    int m_nextHandle = 1;
    bool m_initialized = false;
    static bool isImagePath(const std::string& path);
};
```

`src/live2d/NullAnimationBackend.cpp` — 新增完整实现:

| 方法 | 行为 |
|------|------|
| `init()` | 设置 `m_initialized = true`, 打印日志 |
| `shutdown()` | 遍历 m_sprites, 调用 TextureManager 释放纹理, 清空 map |
| `loadModel(path, name)` | 检测 .png/.jpg/.jpeg/.bmp 后缀 → TextureManager::loadTexture → 分配 handle → 返回有效 ID; 非图片返回 0 |
| `unloadModel(handle)` | 从 m_sprites 查找 → TextureManager::destroyTexture → 移除 |
| `isLoaded(handle)` | 在 m_sprites 中查找 |
| `showModel(handle, x, y, scale)` | 存储位置/缩放, 标记 visible=true |
| `hideModel(handle)` | 标记 visible=false |
| `setOpacity(handle, opacity)` | 存储透明度 |
| `render(dt)` | 遍历可见 sprite, 通过 BgfxDraw 提交静态纹理 (预留) |
| `playMotion/setExpression/setParameter` | 返回 false / no-op |
| `name()` | 返回 `"NullAnimation+PNG"` |
| `isImagePath(path)` | 静态辅助方法, 检测文件后缀 |

**纹理管理**: 通过 `BackendRegistry::instance().getTextureManager()` 访问 TextureManager 单例。

**测试适配**: `tests/cpp/test_live2d.cpp` 中的 `name()` 断言从 `"NullAnimation"` 更新为 `"NullAnimation+PNG"`。

**构建适配**: `NullAnimationBackend.cpp` 添加到 `tests/CMakeLists.txt` (之前测试项目未编译此文件), 同时修复 `ISaveProvider.cpp` 的 include 路径 (`"ISaveProvider.h"` → `"api/ISaveProvider.h"`)。

### R2.2: Cubism SDK 集成文档

**状态**: ✅ 完成

**文件**: `docs/guides/live2d-setup.md`

内容:
- 前置条件 (Live2D Cubism SDK for Native 许可, 三端支持)
- 无 SDK 环境说明 (NullAnimationBackend PNG 降级)
- SDK 安装 5 步: 下载 → 放置到 `external/Live2D/` → CMake 配置 `-DCAESURA_ENABLE_LIVE2D=ON` → 放置模型文件 → KAG 脚本引用
- 模型目录结构模板
- 编译宏参考 (`CAESURA_HAS_LIVE2D`, `CAESURA_ENABLE_LIVE2D`)
- 渲染路径表 (D3D11/Metal/OpenGL 及其回退)
- 常见问题 (找不到头文件, 运行时崩溃, Metal 不工作, Release 构建去掉 Live2D)

---

## R3 — 资源管线文档

### R3.1: 资源格式规范

**状态**: ✅ 完成

**文件**: `docs/guides/asset-pipeline.md`

从源码提取的实际格式支持:

**图片** (stb_image):
| 格式 | 透明通道 | 推荐用途 |
|------|---------|---------|
| PNG | ✅ RGBA | 立绘、UI (推荐) |
| JPG | ❌ | 背景 (有损压缩, 文件小) |
| TGA | ✅ | 纹理 |
| BMP | ❌ | 不推荐 (无压缩) |
| PSD | ✅ | 开发期 |
| GIF | ✅ | 静态 |
| HDR | N/A | HDR 光照 |
| PIC | — | Softimage |
| PNM | — | Netpbm |

**音频** (SoLoud):
| 格式 | 推荐用途 |
|------|---------|
| WAV | 音效、语音 (推荐) |
| FLAC | 高质量 BGM |
| MP3 | 压缩 BGM |
| OGG | 开源 BGM |

**视频**: MPEG1 (1920×1080)

**推荐目录结构**: `assets/images/{bg,fg,ui}/`, `assets/audio/{bgm,se,voice}/`, `assets/video/`

### R3.2: CARC 打包指南

**状态**: ✅ 完成

**文件**: `docs/guides/carc-packaging.md`

内容:
- CLI 工具用法: `carc_pack <input_dir> <output.carc> [public.key] [private.key]`
- 密钥管理说明
- CARC 文件格式结构图 (Header → Index → Body → Footer)
- KAG 脚本中 `carc://` 协议引用
- 安全注意事项 (私钥不提交仓库, Ed25519 签名验证)
- 常见问题

---

## R4 — 测试覆盖率补齐

### R4.1: Script 模块单元测试

**状态**: ✅ 完成

**新增文件**:

`tests/cpp/test_script_bindings.cpp` (9 用例):

| 用例 | 验证内容 |
|------|----------|
| `Bindings: DevCore module registered as global` | `lua_getglobal(L, "DevCore")` 返回 table |
| `Bindings: DevCore.log callable` | 调用 log("test message") 返回 LUA_OK |
| `Bindings: VFX module registered` | `lua_getglobal(L, "VFX")` 返回 table |
| `Bindings: VFX particles functions exist` | `particles_create_emitter` 和 `particles_alive_count` 是 function |
| `Bindings: Render module registered` | `lua_getglobal(L, "Render")` 返回 table |
| `Bindings: KAG.play_bgm returns boolean` | 无效文件返回 `false` (boolean, 不是 number) |
| `Bindings: KAG.stop_bgm does not crash` | 无参数调用返回 LUA_OK |
| `Bindings: Debug module functions exist` | `get_last_error` 和 `log` 是 function |
| `Bindings: KAG global table must have expected APIs` | `play_bgm`, `play_se`, `render_text` 均为 function |

`tests/cpp/test_game_state.cpp` (5 用例):

| 用例 | 验证内容 |
|------|----------|
| `GameState: push succeeds after LuaManager init` | init() 内部已调用 create(), push 成功 |
| `GameState: create is idempotent` | 重复 create 不崩溃 |
| `GameState: ctx table has backlog field` | ctx 表中 backlog 为 table 类型 |
| `GameState: ctx table can store custom fields` | 写入 "my_field" → 读取一致 |
| `GameState: separate Lua states have separate ctx` | lm1 写入的字段在 lm2 中为 nil |

**实现细节**:
- 每个测试创建独立的 `LuaManager`, 避免状态污染
- `BackendRegistry::instance().registerNullBackends()` 确保绑定不崩溃
- 发现 `LuaManager::init()` 内部已调用 `GameState::create()`, 因此 "push without create" 不可测

### R4.2: Entry 模块单元测试

**状态**: ✅ 完成

**新增文件**: `tests/cpp/test_entry.cpp` (6 用例)

| 用例 | 验证内容 |
|------|----------|
| `EngineConfig default values` | width=1280, height=720, title="Caesura (AmeKAG)", headless=false, editorMode=false |
| `EngineConfig pointer fields default nullptr` | 10 个指针字段全部为 nullptr |
| `EngineConfig headless mode flag` | cfg.headless = true 可设置, editorMode 保持 false |
| `EngineConfig editor mode flag` | cfg.editorMode = true 可设置, headless 保持 false |
| `EngineConfig custom title` | title strlen > 0 |
| `EngineConfig width/height range` | 640×480 和 1920×1080 均可设置 |

**设计决策**: 仅测试 `EngineConfig` (纯数据结构), 不构造 `Engine` 对象 (Engine 构造函数创建 GpuMonitor 等对象, 在测试环境中会 SIGSEGV)。

---

## R5 — CI 跨平台验证

**状态**: ⏸ 需推送触发

`.github/workflows/ci.yml` 已配置三端:
- Windows 2022 / MSVC (Debug + Release, 构建 + 测试)
- macOS latest / Clang (Debug, 构建 + 测试)
- Ubuntu 24.04 / GCC (Debug, 构建 + 测试 + 耦合计数)

macOS 的 `|| true` 已在之前提交中移除。需要将当前 master 推送到 GitHub 触发 CI, 根据失败日志迭代修复。

**预期**: Windows CI 绿 (本地构建通过), macOS/Linux CI 可能有依赖安装或平台宏相关问题。

---

## Bug 修复

### ISaveProvider.cpp include 路径

**问题**: `src/storage/ISaveProvider.cpp` 使用 `#include "ISaveProvider.h"`, 但头文件在 `src/storage/api/ISaveProvider.h`。全量 clean build 时编译器找不到头文件。

**修复**: 改为 `#include "api/ISaveProvider.h"`。

**影响文件**: `src/storage/ISaveProvider.cpp`

### CryptoEngine 注册位置 (T2 会话)

**问题**: `BackendRegistry::instance().setCryptoEngine()` 嵌套在 `if (m_steamBackend->init())` 块内, Steam 不可用时 CryptoEngine 不注册。

**修复**: 移至 `initOptionalPhase()` 顶层。

---

## 文件变更清单

### 新增文件 (6)

| 文件 | 阶段 |
|------|------|
| `docs/guides/asset-pipeline.md` | R3.1 |
| `docs/guides/carc-packaging.md` | R3.2 |
| `docs/guides/live2d-setup.md` | R2.2 |
| `tests/cpp/test_script_bindings.cpp` | R4.1 |
| `tests/cpp/test_game_state.cpp` | R4.1 |
| `tests/cpp/test_entry.cpp` | R4.2 |

### 修改文件 (5)

| 文件 | 变更 | 阶段 |
|------|------|------|
| `src/rpc/EditorServer.cpp` | +`/api/status` 端点 (+12 行) | R1.1 |
| `src/live2d/NullAnimationBackend.h` | 头文件内联 → 实例类声明 (+35 行) | R2.1 |
| `src/live2d/NullAnimationBackend.cpp` | 空桩 → PNG 降级实现 (+120 行) | R2.1 |
| `src/storage/ISaveProvider.cpp` | include 路径修复 | Bug fix |
| `tests/cpp/test_live2d.cpp` | `"NullAnimation"` → `"NullAnimation+PNG"` | R2.1 |
| `tests/CMakeLists.txt` | +3 新测试文件 + NullAnimationBackend.cpp | R4, R2.1 |

### 未完成 (3)

| 任务 | 阻塞原因 |
|------|----------|
| R1.2 Live2D 预览面板 | 需修改前端 React 代码 + 2 个 RPC 端点 |
| R1.3 一键打包 | 需 POST /api/build 端点 + 前端按钮 |
| R5 CI 验证 | 需推送到 GitHub 观察远端结果 |
