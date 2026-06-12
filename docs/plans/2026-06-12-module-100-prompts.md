# E1-E5 执行提示词

## 通用规则

```
你是 Caesura (AmeKAG) 引擎开发者。先读 AGENTS.md。
一步一提交。构建零错误，306 测试全绿。
构建：cd build && cmake --build . --config Debug --parallel
测试：cd build\tests\Debug && .\CaesuraTests.exe
```

---

## E1: entry — 构造函数解耦（90% → 100%）

```
### 缺口
Engine 构造函数用三元表达式创建 GpuMonitor/NullGpuMonitor，直接 include 两个具体头文件。
测试中构造 Engine 仍需 NullGpuMonitor.h。

### 执行

Step 1: 重构构造函数
- Engine() 中 m_gpuMonitor 默认 nullptr
- 移除构造函数的 GpuMonitor/NullGpuMonitor 创建逻辑
- 在 initPlatformPhase() 中创建：
  如果 headless → make_unique<NullGpuMonitor>()
  否则 → make_unique<GpuMonitor>()
- 删除 Engine.cpp 中的 #include "GpuMonitor.h" 和 #include "NullGpuMonitor.h"
- 在 initPlatformPhase() 所在位置（Engine.cpp 中）保留这两个 include
- Engine.h 中 m_gpuMonitor 保持 unique_ptr<IGpuMonitor>
- 构建 + 测试全绿 → 提交

Step 2: 扩展 test_entry.cpp
- 添加：Engine 默认构造不崩溃（不 init，不 shutdown）
- 添加：Engine headless init → shutdown 生命周期
- 构建 + 测试全绿 → 提交

验收：Engine 构造函数零外部具体头文件依赖。2 次提交。
```

---

## E2: rpc — 前端面板确认（95% → 100%）

```
### 缺口
之前标注 Live2D 面板未完成。实际代码已有 fetch 调用。需审查确认。

### 执行

Step 1: 审查 Live2DPanel.jsx（已有 2 个 fetch）
- 读取 web-editor/src/components/Live2DPanel.jsx
- 逐行审查：models 列表获取、模型加载、状态展示
- 确认 GET /api/live2d/models 和 POST /api/live2d/load 调用正确
- 提交审查结果

Step 2: 审查 AssetPanel.jsx（已有 1 个 fetch）
- 读取 web-editor/src/components/AssetPanel.jsx
- 确认 POST /api/build 调用 + 结果展示
- 提交审查结果

Step 3: RPC 端点测试
- 扩展 tests/cpp/test_rpc.cpp：
  - /api/live2d/models 返回 JSON 数组
  - /api/build 返回 {"success":true,"path":"...","size":...}
- 构建 + 测试全绿 → 提交

验收：3 端点前后端可验证。3 次提交。
```

---

## E3: script — 边界测试补齐（90% → 100%）

```
### 缺口
41 个测试用例缺少 KAG 错误恢复、GameState 持久化、绑定参数校验。

### 执行

Step 1: KAG 错误恢复测试
扩展 tests/cpp/test_kag_execution.cpp（+4 用例）：
1. 语法错误的 KAG 脚本不崩溃，返回错误
2. @jump 到不存在的 label 不崩溃
3. @choice 选项数为 0 时的降级行为
4. 嵌套 @if 超过 10 层的保护
- 构建 + 测试全绿 → 提交

Step 2: GameState 持久化测试
扩展 tests/cpp/test_game_state.cpp（+3 用例）：
1. 存档后读档，GameState 恢复正确
2. GameState 存储嵌套表（table in table）
3. clear 后重新 push 正确隔离
- 构建 + 测试全绿 → 提交

Step 3: 绑定参数校验测试
扩展 tests/cpp/test_script_bindings.cpp（+5 用例）：
1. KAGBinding play_bgm 空字符串不崩溃
2. RenderBinding load_texture 路径为空不崩溃
3. VFXBinding create_emitter 负粒子数返回 -1
4. DebugBinding log nil 参数不崩溃
5. DevCoreBinding set_dev_mode 非法值不崩溃
- 构建 + 测试全绿 → 提交

验收：306 → 318+。3 次提交。
```

---

## E4: storage — 缩略图截图（95% → 100%）

```
### 缺口
captureThumbnailPNG() 返回空字符串（stub）。

### 执行

Step 1: 实现 RTT 读回
- 修改 SaveManager::captureThumbnailPNG()
- 创建临时 RTT（320×180）
- bgfx::setViewRect + bgfx::touch 设置视图
- 渲染一帧到 RTT
- bgfx::readTexture 读回 RGBA 数据
- 注意：readTexture 是异步的，需要等到下一帧
- 提交

Step 2: PNG 编码
- 使用 stb_image_write 将 RGBA 编码为 PNG
- 返回 PNG 数据的 Base64 字符串
- 包含 stb_image_write.h（已在 external/stb/）
- 提交

Step 3: 测试
扩展 tests/cpp/test_storage.cpp（+2 用例）：
1. captureThumbnailPNG 返回非空字符串
2. 缩略图数据可 Base64 解码（验证非空）
- 构建 + 测试全绿 → 提交

验收：captureThumbnailPNG 返回有效 PNG 数据。3 次提交。
```

---

## E5: minigame — 核心渲染链路（30% → 60%）

```
### 缺口
BgfxMiniGameBackend 全部 no-op。有结构定义和 shader，无实现。

### 执行

Step 1: GLB 场景加载
- 修改 BgfxMiniGameBackend::loadScene()
- 使用 cgltf（external/bgfx/bgfx/3rdparty/cgltf/cgltf.h）解析 GLB
- 提取第一个 mesh 的顶点位置数据
- 创建 bgfx::VertexBuffer + bgfx::IndexBuffer
- 存储到内部 map<sceneHandle, SceneData>
- 无效文件返回 0
- 构建 + 测试全绿 → 提交

Step 2: 场景渲染
- 修改 BgfxMiniGameBackend::render()
- 对当前 active scene：
  - 设置视图（MINIGAME view ID）
  - 绑定 vertex/index buffer
  - 使用 EmbeddedShaders_MiniGame 的 program
  - bgfx::submit()
- 无 active scene 时 no-op
- 构建 + 测试全绿 → 提交

Step 3: 场景卸载与清理
- 修改 BgfxMiniGameBackend::unloadScene() + shutdown()
- 释放所有 bgfx 资源（vertex/index buffer）
- shutdown 时清理所有 scene
- 构建 + 测试全绿 → 提交

Step 4: 测试
扩展 tests/cpp/test_mini_game.cpp 或 test_minigame.cpp（+3 用例）：
1. loadScene 无效路径返回 0
2. loadScene → enter → render → leave → unloadScene 不崩溃
3. 两次 loadScene 返回不同 handle
- 构建 + 测试全绿 → 提交

验收：BgfxMiniGameBackend 可加载渲染简单 3D 场景。4 次提交。
```

---

## 验收总清单

| 模块 | 目标测试 | 关键产出 |
|------|---------|---------|
| entry | 308+ | 构造函数零具体头文件依赖 |
| rpc | 310+ | 3 RPC 端点端到端验证 |
| script | 320+ | 12 个新边界用例 |
| storage | 322+ | 缩略图截图功能 |
| minigame | 325+ | 3D GLB 加载+渲染+卸载 |
