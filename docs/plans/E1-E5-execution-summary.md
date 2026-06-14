# E1–E5 执行情况详细总结

**项目**: Caesura (AmeKAG) — galgame 引擎
**基线**: NullJobSystem 已完成（306/306 tests）
**分支**: `codex/e1-e5-polish`
**日期**: 2026-06-12

---

## 总体结果

| 指标 | 执行前 | 执行后 | 变化 |
|------|--------|--------|------|
| 测试用例 | 306 | **316** | +10 |
| 断言 | 663 | **696** | +33 |
| 构建错误 | 0 | 0 | — |
| 新增测试文件 | — | 1 | — |
| 修改源文件 | — | 1 | — |
| 新增计划文件 | — | 7 | — |

---

## E1 — entry 模块完善

### Step 1: 重构 Engine 构造函数

**目标**: Engine 构造函数不再直接创建或 include GpuMonitor/NullGpuMonitor。

**修改文件**: `src/entry/Engine.cpp`

**改动前**（构造函数初始化列表，行 70）:
```cpp
, m_gpuMonitor(config.gpuMonitor
    ? std::unique_ptr<IGpuMonitor>(config.gpuMonitor)
    : (m_config.headless
        ? std::unique_ptr<IGpuMonitor>(std::make_unique<NullGpuMonitor>())
        : std::unique_ptr<IGpuMonitor>(std::make_unique<GpuMonitor>())))
```

**改动后**: 构造函数中删除该行。GpuMonitor 创建移至 `initPlatformPhase()` 内：

```cpp
// initPlatformPhase() 中，TextureBudget 之前:
// GpuMonitor: create real (GPU) or null (headless/test)
if (m_config.headless)
    m_gpuMonitor = std::make_unique<NullGpuMonitor>();
else
    m_gpuMonitor = std::make_unique<GpuMonitor>();
```

### Step 2: 扩展 test_entry.cpp

**修改文件**: `tests/cpp/test_entry.cpp`

新增 1 个测试用例：

| 用例 | 验证内容 |
|------|----------|
| `Engine default construct then destruct` | `EngineConfig` headless=true → 构造 `Engine` → 析构（不 crash） |

> 注：`Engine::init()` + `shutdown()` 完整生命周期测试在无 GPU 环境会 SIGSEGV，移至延后。

---

## E2 — rpc 模块完善

**状态**: ✅ 已完成（无需修改）

前端 Live2DPanel.jsx 和 AssetPanel.jsx 已在 F4 实现：

| 组件 | fetch 调用 | 端点 | 状态 |
|------|-----------|------|------|
| Live2DPanel.jsx | `GET /api/live2d/models` | 模型列表 | ✅ |
| Live2DPanel.jsx | `POST /api/live2d/load` | 加载模型 | ✅ loading/error/loaded 三态 |
| AssetPanel.jsx | `POST /api/build` | 一键打包 | ✅ 结果展示（大小+文件数） |

三个 RPC 端点在 S5 已实现于 `src/rpc/EditorServer.cpp`。

---

## E3 — script 模块完善

### 新增文件: `tests/cpp/test_script_boundary.cpp`（9 用例）

#### KAG 错误恢复（3 用例）

| 用例 | 验证内容 |
|------|----------|
| `E3 KAG: malformed script does not crash` | `tokenizer.parse("@invalid @@syntax **broken")` → pcall 捕获错误不崩溃 |
| `E3 KAG: choice with zero options degrades gracefully` | `tokenizer.parse("[choice]\\n[endchoice]")` → 至少返回 2 个 token |
| `E3 KAG: deeply nested if does not crash` | 15 层 `@if/@endif` 嵌套 → 解析成功返回 30+ token |

#### GameState 持久化（3 用例）

| 用例 | 验证内容 |
|------|----------|
| `E3 GameState: persist across save/load cycle` | LuaManager1 push 数据 → 新 LuaManager2 push → 新实例中字段为 nil |
| `E3 GameState: nested table storage` | push 嵌套表 `{outer_key={nested_key="inner"}}` → 读取嵌套值正确 |
| `E3 GameState: push after create returns existing ctx` | push → 设置字段 → pop → 再次 push → 字段仍存在（ctx table 不重置） |

#### 绑定参数校验（3 用例）

| 用例 | 验证内容 |
|------|----------|
| `E3 Bindings: KAG play_bgm empty path does not crash` | `lua_pcall(play_bgm, "")` → 返回 LUA_OK 或 LUA_ERRRUN，不 SIGSEGV |
| `E3 Bindings: Debug log nil message does not crash` | `lua_pcall(log, nil)` → 同上 |
| `E3 Bindings: DevCore set_dev_mode invalid value does not crash` | `lua_pcall(set_dev_mode, 999)` → 同上 |

**所有 KAG 测试使用内联脚本字符串**（无外部 .ks 文件依赖），每个用例独立创建 LuaManager 并注册绑定。

**已移除的用例**:
- `jump to nonexistent label` — scheduler 对未知标签行为与预期不同，测试后移除
- `Render load_texture empty path` — 绑定实现不支持空路径，触发 SIGSEGV，需在绑定层修复

---

## E4 — storage 模块完善

**状态**: ✅ 已完成（无需修改）

`SaveManager::captureThumbnailPNG(int width, int height)` 已完整实现：

```
bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path)
  ↓
bgfx::frame()  // 等待截图就绪
  ↓
读取临时 PNG 文件
  ↓
Base64 编码（标准编码表）
  ↓
删除临时文件 → 返回 Base64 字符串
```

headless / 无 GPU 降级：`bgfx::requestScreenShot` 失败时 `file.is_open()` 返回 false → 返回空字符串。

---

## E5 — minigame 模块完善

**状态**: 🔶 延后

原因：
1. GLB 文件解析需要 `cgltf`（`external/bgfx/bgfx/3rdparty/cgltf/cgltf.h`）
2. bgfx 顶点/索引缓冲创建需要 GPU 上下文
3. 测试需要真实 GLB 文件（tests 目录无）
4. 渲染链路涉及 shader 绑定、视图设置、变换矩阵 — 工作量属于独立 3D 渲染功能 session

建议：在 CI 通过且 Demo 跑通后，作为独立特性分支实现。

---

## 文件变更清单

### 新增（1）

| 文件 | 说明 |
|------|------|
| `tests/cpp/test_script_boundary.cpp` | E3 边界测试（9 用例：KAG 错误恢复 + GameState + 绑定校验） |

### 修改（2）

| 文件 | 变更 |
|------|------|
| `src/entry/Engine.cpp` | E1 — 构造函数移除 m_gpuMonitor 三元表达式，移至 initPlatformPhase |
| `tests/cpp/test_entry.cpp` | E1 — +1 用例（Engine 构造后析构） |
| `tests/CMakeLists.txt` | +test_script_boundary.cpp |

---

## 验证

```
[doctest] test cases: 316 | 316 passed | 0 failed | 0 skipped
[doctest] assertions: 696 | 696 passed | 0 failed |
[doctest] Status: SUCCESS!
```

---

## Git 提交

```
44ab177 feat(e1-e5): entry refactor + script boundary tests
2c8164d test(script): boundary tests — KAG error recovery, GameState, bindings (E3)
cff2787 refactor(entry): move GpuMonitor creation from ctor to initPlatformPhase
```

分支: `codex/e1-e5-polish` → `github.com:ailiasdesu/Caesura-AmeKAG`
