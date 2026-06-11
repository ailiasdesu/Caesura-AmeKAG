# Live2D Integration Plan — Caesura (AmeKAG)

> 2026-06-08 | P2 | Phase 1 完成，Phase 2-4 进行中

## ✅ 已完成

### Phase 1: SDK 集成 + CMake (完成)
- [x] `CAESURA_LIVE2D` option（默认 OFF）
- [x] Cubism 5 Core (prebuilt) + Framework 静态库编译
- [x] GLEW 静态链接（Windows OpenGL ES 2 后端）
- [x] `Live2DUserModel` 桥接类（暴露 protected motionManager/expressionManager）
- [x] Debug 构建通过：`CaesuraAmeKAG.exe` + `CaesuraTests.exe` + `carc_pack.exe`

### Live2DBackend 已实现功能
- [x] CubismFramework 初始化和销毁
- [x] 模型加载（`.model3.json` → `.moc3` → 纹理 → 动作/表情缓存）
- [x] 模型显示/隐藏/透明度
- [x] 动作播放（motion）
- [x] 表情设置（expression）
- [x] 参数控制（Cubism 5 API：遍历 `csmGetParameterIds`）
- [x] Lua 绑定（`live2d.*`）
- [x] 渲染管线：OpenGL FBO → `glReadPixels` → bgfx Texture2D

---

## ⚠️ 已知限制

### macOS 兼容性风险

**OpenGL 已被 Apple 弃用（2018），macOS 15+ 可能随时移除。**

| 平台 | bgfx 后端 | Live2D 渲染路径 | 状态 |
|------|-----------|----------------|------|
| Windows | D3D11 | OpenGL ES 2（headless context）| ✅ 可用 |
| Linux | OpenGL/Vulkan | OpenGL ES 2 | ✅ 可用 |
| macOS | Metal | **无可用渲染器** | ❌ 阻塞 |

**原因：**
- Cubism SDK Metal 渲染器 (`Framework/src/Rendering/Metal/`) 仅支持 iOS (`CSM_TARGET_IPHONE`)，未验证 macOS
- macOS 需无窗口 OpenGL Context（CGL），当前未实现
- Apple 可能在 macOS 16 彻底移除 OpenGL 支持

**TODO（需 macOS 开发者）：**
1. 验证 Cubism Metal 渲染器能否在 macOS 工作（需 `MTKView` 或无窗口 `MTLDevice`）
2. 或者适配 macOS CGL headless context + OpenGL ES 2（临时的）
3. 最终方案：为每个 bgfx 后端实现对应的 Cubism 原生渲染器

### 性能：GPU→CPU→GPU 回读

当前每帧 `glReadPixels` → bgfx `updateTexture2D`，高分辨率模型会瓶颈。
后续架构重构计划见下文。

---

## 🚧 下一步：可插拔渲染路径

```
Live2D 顶点/纹理数据
       │
       ▼
IRenderPath 接口
       │
       ├── OpenGLReadbackPath  ← 当前方案（CPU 回读，跨平台后备）
       ├── D3D11NativePath     ← Windows 最优（零拷贝，直接写 bgfx 纹理）
       ├── MetalNativePath     ← macOS 最优（待验证）
       └── OpenGLSharedPath    ← Linux 最优（共享 bgfx GL 上下文）
```

通过 `IAnimationBackend` → `ILive2DRenderPath` 两层抽象，编译期/运行期选择渲染路径。

## Legal Checklist

- [x] Cubism SDK 不在仓库内（`.gitignore` + 用户下载）
- [x] 默认 `OFF`，不编译任何 Live2D 代码
- [x] 所有 Live2D 文件 `#ifdef` 守卫
- [x] README 注明可选依赖及授权条款
- [x] MIT 许可不因 Live2D 选项而改变