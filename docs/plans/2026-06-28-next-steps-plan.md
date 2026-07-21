# Caesura (AmeKAG) — 下一步修改方案

> 2026-06-28 | 基于 76 项就绪度审计结果 | 不出代码，仅方案

---

## 当前现状

| 指标 | 值 |
|------|-----|
| 通过率 | 68/76 (89%) |
| P2延期 | 7项 |
| P0阻塞 | 0 |
| 测试 | 412 passed |
| 远程 | 20073e29 |

---

## 方案 A: 修完所有 P2 (7项 → 预计 2-3h)

### A1. D9.4 焦点切换 (已实现，需验证)
**现状:** 9e8ec7a7 已在 BgfxMiniGameBackend::enter/leave 中加入 setFocus 调用
**需做:** 启动引擎 → 进入小游戏 → 确认输入焦点从 KAG 切换到 GAME → 退出 → 确认切回 KAG
**预估:** 15min

### A2. D10.1 PNG立绘渲染 (已实现，需验证)
**现状:** 9e8ec7a7 已在 NullAnimationBackend::render 中加入 bgfx textured quad 提交
**需做:** 启动引擎 → 加载 PNG 立绘 → 确认立绘显示在VIEW_MAIN上
**预估:** 15min

### A3. D4.6 Voice回调 (已实现，需验证)
**现状:** 9e8ec7a7 已在 Engine::run() 中加入 _onVoiceComplete Lua回调
**需做:** 注册 _onVoiceComplete Lua函数 → 播放Voice → 等待播放完毕 → 确认回调触发
**预估:** 15min

### A4. D2.6 图层透明度渐变 (需Lua层实现)
**现状:** C++ 侧 LayerManager::setOpacity 已实现；transition 系统已实现
**方案:** 在 `scripts/layers.lua` 添加 `layers.fade_to(layer, target_opacity, duration)` 函数
```
function layers.fade_to(layer_name, target_opacity, duration_ms)
  -- 使用 coroutine + setOpacity 逐帧过渡
  -- 参考: scripts/transition.lua 的 crossfade 实现模式
end
```
**影响文件:** `scripts/layers.lua`
**预估:** 30min

### A5. D3.8 文字淡入淡出 (需渲染层实现)
**现状:** renderText 支持 alpha 参数但无帧间渐变
**方案1(Lua层):** 在 KAG text 命令中逐帧调用 renderText 调整 alpha
**方案2(C++层):** TextRenderer 添加 renderTextFade() 方法，内置帧间 alpha 渐变
**推荐:** 方案1 (Lua层)，与 D2.6 模式统一
**影响文件:** `scripts/kag/commands/text.lua` 或 `scripts/transition.lua`
**预估:** 45min

### A6. D3.5 文字换行 (需新功能开发)
**现状:** TextRenderer 无自动换行逻辑
**方案:** TextRenderer::renderText 添加 maxWidth 参数：
1. FreeType 测量每个字符宽度 → 累加
2. 超出 maxWidth → 在当前字符前插入换行 → 下移一行
3. 中文字符优先在对齐换行
**影响文件:** `src/render/TextRenderer.cpp/h`, `src/render/BgfxRenderDevice.cpp`
**接口变更:** IRenderDevice::renderText 签名需增加 maxWidth 参数（默认 0 = 不换行）
**预估:** 2h (需要理解 FreeType glyph 度量 + 测试 CJK 换行)

### A7. D7.6 VFX GPU特效 (需验证)
**现状:** VFX.lua 存在，BgfxShaderManager 有 VFX program，BgfxDraw_Effects.cpp 有 VFX 路径
**方案:** 编写测试脚本触发 quake → 验证 bgfx submit 是否正确 → 检查 GPU 特效表现
**影响文件:** 无需修改，仅验证
**预估:** 30min

---

## 方案 B: 代码简化 Round 3 (预计 1h)

### B1. 清理 Engine.cpp 死 include (4行)
- `#include <atomic>` (行43) — 无 std::atomic 直接使用
- `#include <bx/math.h>` (行45) — 无 bx 数学函数
- `#include <bx/bx.h>` (行46) — 无 bx 宏/工具
- `#include <cmath>` (行50) — 无 std::sqrt/sin/cos
**预估:** 5min

### B2. 清理 BgfxRenderDevice.cpp 死 include (7行)
- `#include "ShaderCache.h"` — 无引用
- `#include "../di/thread/ThreadAssert.h"` — 无断言使用
- `#include <bx/math.h>` — 所有数学在 BgfxDraw
- `#include <bimg/decode.h>` — 解码在 TextureManager
- `#include <bx/readerwriter.h>` — 无引用
- `#include <bx/error.h>` — 无引用
- `#include <cstring>` — 无 strcmp/memcpy
**预估:** 5min

### B3. 清理 LayerManager.cpp 冗余 <algorithm> (1行)
- LayerManager.h 已包含 <algorithm>，.cpp 重复
**预估:** 1min

### B4. 清理 BgfxQuadBatch.h 死 include <cstdint> (1行)
- `<cstdint>` 类型由 bgfx.h 透传，不直接使用
**预估:** 1min

### B5. 修复 VIEW_TRANSITION 冲突常量
- IRenderDevice.h:14 `constexpr uint16_t VIEW_TRANSITION = 99` → 改为 3
- 或删除 IRenderDevice.h 中的定义（BgfxDeviceCore.h:21 已定义=3）
**预估:** 5min

### B6. 删除 TextureManager.cpp 重复 <filesystem> (1行)
- 行8和行9都包含 <filesystem>
**预估:** 1min

---

## 方案 C: 写全流程 demo (预计 2h)

写一份 `demo/full_pipeline_test.ks`:

### 覆盖场景
```
[title]         → 标题画面
[new_game]      → 新游戏
[bg]            → 背景切换 (3个不同场景)
[fg]            → 2个角色立绘显示/切换
[ch]+[p]        → 多角色对话 + 点击推进
[ruby]          → 注音文字测试
[if]+[jump]     → 条件分支 (2条路线)
[bgm]+[se]      → BGM播放 + SE音效
[voice]         → 语音播放
[save]          → 存档 (slot 1)
[load]          → 读档 (slot 1)
[trans]         → crossfade/wipe转场
[quake]         → VFX特效
[eval]+[iscript]→ Lua混合脚本
[end]           → 结局
```

### 资源清单 (需准备)
- `assets/bg/scene01.png` — 白天教室
- `assets/bg/scene02.png` — 傍晚走廊
- `assets/bg/scene03.png` — 星空
- `assets/fg/hana.png` — 角色A立绘
- `assets/fg/yuki.png` — 角色B立绘
- `assets/bgm/daily.ogg` — 日常BGM
- `assets/se/click.ogg` — 点击SE
- `assets/voice/line01.ogg` — 语音测试

**影响文件:** `demo/full_pipeline_test.ks` (新建), `demo/full_pipeline_entry.lua` (新建)

---

## 方案 D: 架构级安全审计 (预计 1.5h)

### D1. BackendRegistry null 路径完整性检查
- 审查所有 `BackendRegistry::getXxx()` 调用点
- 确认每个调用都有 null 检查或 null 安全降级
- 已有 null 检查: Engine::run(), NullAnimationBackend::render()
- 需检查: RenderBinding, KAGBinding, VFXBinding, SaveBinding

### D2. Lua error 传播路径检查
- 审查所有 `lua_pcall` 调用，确认每个都有错误处理
- 审查 lua_getglobal + lua_isfunction 模式完整性
- 已有: Engine::run() 中的 _KAG_onClick/_KAG_onCtrlDown 等

### D3. GPU 资源生命周期检查
- 审查 bgfx::createTexture2D / bgfx::destroy 配对
- 审查 TextureManager::onDeviceLost/onDeviceRestored 完整性
- 审查 bgfx::createUniform / bgfx::destroy 配对

---

## 推荐执行顺序

```
1. B: 代码简化 Round 3 (1h, 安全, 零行为变更)
    ↓
2. A: P2验证 (A1-A3, 1h, 只验证不修改)
    ↓
3. A: P2实现 (A4-A7, 按难度递增)
    ↓
4. C: 全流程demo脚本 (2h, 验证所有核心路径)
    ↓
5. D: 架构级安全审计 (1.5h, 全面安全审查)
```

总计: ~8h 工作量，可在 3人并行下 2-3天完成。
