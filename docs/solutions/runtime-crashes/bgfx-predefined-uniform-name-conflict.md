---
module: minigame
tags: [bgfx, shader, crash, uniform]
problem_type: runtime-crash
---

# bgfx PredefinedUniform 名称冲突导致 createUniform 失败

## 症状

`BgfxMiniGameBackend::enter()` 在真实 GPU（D3D11）上崩溃（SEH / SIGSEGV），
headless 测试无法复现。崩溃点随诊断打印定位到 `bgfx::createUniform("u_viewProj", Mat4)`。

## 根因

`u_viewProj` 是 bgfx 的**预定义 uniform 名**（`PredefinedUniform::ViewProj`，
见 `external/bgfx/bgfx/src/bgfx.cpp` 的 `s_predefinedName[]`，用于自动绑定
相机矩阵）。`bgfx::createUniform()` 内部调用 `isIdentifierValid()` 拒绝
预定义名，Debug 构建触发 `BX_ASSERT`（`__debugbreak` → 被 doctest 捕获为
"Unhandled SEH exception"），并返回 `BGFX_INVALID_HANDLE`。

`u_mtx` 不冲突（非预定义名），所以只有 `u_viewProj` 崩溃——按 handle 分配
顺序二分打印才能定位（`u_mtx idx=12, u_albedo idx=10` 等）。

## 修复

统一改名：`u_viewProj` → `u_miniViewProj`，同步 C++ `createUniform` 调用、
GLSL / HLSL / MSL shader 源码，并**重新编译 DXBC**（fxc）：

```
fxc /T vs_4_0 /E main /Fo minigame_vs.dxbc minigame_vs.hlsl
```

## 教训

- bgfx 预定义 uniform 名（`u_viewRect`/`u_viewProj`/`u_modelViewProj`/`u_alphaRef4` 等）
  是保留名，**用户代码不得创建同名 uniform**。
- 调用 `bgfx::createUniform` 后必须检查 `bgfx::isValid(handle)`——非法名会
  返回无效句柄而非抛错。
- GPU 崩溃（SEH）定位方法：断言消息走 `OutputDebugString` 不可见时，安装
  `bx::setAssertHandler` 自定义 handler 把断言打印到 stderr 并返回 false
  继续执行，可以拿到精确的 `file(line): message`。
