# Tech Debt Cleanup Plan — Caesura (AmeKAG)

> Generated: 2026-06-08 | Target: 4 items / ~2h

## Scope

10 项标记中，6 项为功能规划（TD-07阈值已校准、TD-13 CJK atlas、SPIRV、MiniGame、Mobile、FFmpeg），非技术债。真债 4 项。

---

## Item 1: CI 测试可见性 ⚠️ Medium

**文件:** `.github/workflows/ci.yml`

**问题:** macOS/Linux 测试被 `continue-on-error: true` 掩盖，任何失败都无法从 CI 状态察觉。

**修复:**
1. 移除 macOS/Linux 测试的 `continue-on-error`
2. 添加 `--reporters=xml` 输出，上传为 CI artifact
3. Windows 保留 `continue-on-error`（GPU 依赖，等 MockRenderDevice 后再移除）

**预计:** 30 min

---

## Item 2: CI bx 补丁正规化 ⚠️ Low

**文件:** `.github/workflows/ci.yml` + `external/bgfx/bx/CMakeLists.txt`

**问题:** CI 中 `sed` 删除 bx 的 msvc compat include 路径是 hack：
```yaml
sed -i '/include\/compat\/msvc/d' external/bgfx/bx/CMakeLists.txt  # macOS
sed -i '/include\/compat\/msvc/d' external/bgfx/bx/CMakeLists.txt  # Linux
```

**修复:** 直接修改 vendored bx `CMakeLists.txt`，将 msvc compat 路径改为条件包含：
```cmake
if(MSVC)
    target_include_directories(bx PUBLIC include/compat/msvc)
endif()
```
然后删除 CI 中的 sed 步骤。

**风险:** 低。bx 是 vendored submodule，修改后在 CI 验证通过即可。未被 CI 覆盖的平台不受影响。

**预计:** 30 min

---

## Item 3: TD-14 MobileAdapter 触屏事件注入 ⚠️ Low

**文件:** `src/Platform/MobileAdapter.cpp`

**问题:** `onTouchStart/onTouchMove/onTouchEnd/onLongPress` 创建了 SDL 事件但不推入队列。`SDL_PushEvent()` 未调用。

**修复:** 每个事件创建后添加：
```cpp
SDL_PushEvent(&ev);
```

**影响:** MobileAdapter 是存根，修复后不影响桌面端。当 P2 移动端激活时，触屏→鼠标映射即开即用。

**预计:** 15 min

---

## Item 4: UnifiedBinding 废弃标记 ⚠️ Low

**文件:** `src/Scripting/UnifiedBinding.cpp` + `src/Scripting/UnifiedBinding.h` + `src/Core/Engine.cpp`

**问题:** UnifiedBinding (C++) 和 BackendFactory (Lua) 都创建 `_CAESURA_BACKEND` 表。BackendFactory 是首选实现，UnifiedBinding 未被使用但仍在编译和注册。

**修复:**
1. 在 `UnifiedBinding.h` 注释顶部添加 `@deprecated`
2. 在 Engine.cpp 中注释掉 `registerUnifiedBackendBinding(L)` 调用
3. 验证 demo 正常运行（BackendFactory 独立工作）

**预计:** 30 min

---

## Execution Order

| Step | Item | Validation |
|---|---|---|
| 1 | CI bx 补丁正规化 | CI macOS/Linux 构建通过 |
| 2 | CI 测试可见性 | CI 测试结果可查 |
| 3 | TD-14 MobileAdapter | 编译通过 |
| 4 | UnifiedBinding 废弃 | demo 正常启动，_CAESURA_BACKEND 可用 |

**预计总时:** ~2h

---

## Post-Cleanup State

完成后所有技术债清零。P2 功能（MiniGame/Mobile/FFmpeg）按功能路线图推进。
