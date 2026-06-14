# F1-F6 执行提示词

## 通用规则

```
你是 Caesura (AmeKAG) 引擎开发者。先读 AGENTS.md。
一步一提交。构建零错误，316 测试全绿。
构建：cd build && cmake --build . --config Debug --parallel
测试：cd build\tests\Debug && .\CaesuraTests.exe
```

---

## F1: entry — 消除最后的具体头文件

```
### 缺口
Engine.cpp 行 16: #include "../render/GpuMonitor.h"
Engine.cpp 行 34: #include "../render/NullGpuMonitor.h"

### Step 1: 提取 createGpuMonitor 函数
- 在 Engine.cpp 中 initPlatformPhase() 之前或之后，创建：
  static std::unique_ptr<IGpuMonitor> createGpuMonitor(bool headless) {
      if (headless) return std::make_unique<NullGpuMonitor>();
      return std::make_unique<GpuMonitor>();
  }
- initPlatformPhase() 中调用 m_gpuMonitor = createGpuMonitor(m_config.headless);
- 提交

### Step 2: 将 GpuMonitor/NullGpuMonitor include 移到独立文件
- 创建 src/entry/Engine_Gpu.cpp
- 移入 createGpuMonitor() 函数体
- Engine.cpp 中仅保留函数声明（extern 或 static forward declare）
- 此时 Engine.cpp 不再 include GpuMonitor.h/NullGpuMonitor.h
- 更新 CMakeLists.txt
- 构建 + 测试全绿 → 提交

验收：rg '#include.*GpuMonitor' src/entry/Engine.cpp 返回空。
```

---

## F2: render — api/ 接口去 bgfx 化

```
### 缺口
ITextureManager/ILayerManager/IVideoPlayer 暴露 bgfx::TextureHandle 等 8 处。

### Step 1: 创建不透明句柄
- 创建 src/render/api/RenderTypes.h：
  struct TextureId { uint32_t id = 0; bool valid() const { return id != 0; } };
  struct ProgramId { uint32_t id = 0; bool valid() const { return id != 0; } };
- 提交

### Step 2: 替换接口
- ITextureManager: bgfx::TextureHandle → TextureId（6 处）
- ILayerManager: bgfx::TextureHandle → TextureId, bgfx::ProgramHandle → ProgramId（2 处）
- IVideoPlayer: bgfx::TextureHandle → TextureId（1 处）
- 更新所有实现类（TextureManager/LayerManager/VideoPlayer）：
  内部维护 bgfx handle ↔ 不透明 handle 映射表
- 提交

### Step 3: 消费者适配
- 更新 RenderBinding/KAGBinding/VFXBinding/LayerManager 中所有接口调用
- 构建 → 修复编译错误 → 迭代直到全绿
- 提交

### Step 4: 测试验证
- 构建 + 316 测试全绿（零回归）
- 提交

验收：3 个 api/ 接口文件零 bgfx include。4 次提交。
```

---

## F3: rpc — 端点测试全覆盖

```
### Step 1: 扩展 test_rpc.cpp（+6 用例）
1. /api/ping → {"status":"ok"}
2. /api/status → 包含 "lua" 字段
3. /api/assets?type=image → 返回数组
4. /api/assets?type=script → 返回数组
5. /api/run 执行 print("hello") → 返回 200
6. /api/logs → 返回数组（可能为空）
- 构建 + 测试全绿 → 提交
```

---

## F4: script — 补齐被移除的边界用例

```
### Step 1: 修复 RenderBinding 空路径保护
- render_texture("") 应在绑定层返回 0，不崩溃
- 修改 RenderBinding.cpp 添加空路径检查
- 提交

### Step 2: scheduler 未知标签保护
- conductor.lua 或 scheduler.lua 中 @jump 到不存在 label 时报错不崩溃
- 提交

### Step 3: 扩展 test_script_boundary.cpp（+3 用例）
1. render_texture("") 返回 0
2. @jump 到不存在 label 不崩溃
3. create_emitter 负粒子数返回 -1
- 构建 + 测试全绿 → 提交

验收：316 → 322+。3 次提交。
```

---

## F5: storage — 缩略图测试

```
### Step 1: 扩展 test_storage.cpp（+3 用例）
1. captureThumbnailPNG 调用不崩溃
2. headless 模式返回空字符串
3. 非 headless 模式返回非空 Base64（如 GPU 不可用则跳过）
- 构建 + 测试全绿 → 提交
```

---

## F6: minigame — 完整 3D 渲染链路

```
### Step 1: GLB 解析 + bgfx buffer
- BgfxMiniGameBackend::loadScene() 使用 cgltf 解析 GLB
- 提取顶点位置 + 索引 → bgfx::VertexBuffer/IndexBuffer
- 提交

### Step 2: 渲染管线
- BgfxMiniGameBackend::render() 提交 3D mesh 到 MINIGAME 视图
- 使用 EmbeddedShaders_MiniGame
- 提交

### Step 3: 资源生命周期
- unloadScene/shutdown 正确释放 bgfx 资源
- 提交

### Step 4: 测试
- test_minigame.cpp：loadScene invalid → 0, valid → render → unload
- 提交

验收：BgfxMiniGameBackend 可加载渲染简单 3D 场景。4 次提交。
```
