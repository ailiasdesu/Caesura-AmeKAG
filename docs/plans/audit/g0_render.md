# render 模块审计

> 审查范围：`src/render/` 全部源码（api/ 接口 + 实现）、现有 render 相关测试、文档。
> 审查维度：模块边界铁律（AGENTS.md §1-§4/§7）、潜在 bug/性能、代码异味/测试缺口。
> 结论：**健康为主，P0 边界问题 2 项需修，P1 数项建议处理，无循环依赖、无绕过 BackendRegistry 的新单例。**
> 规模：~40 个文件（api/ 8 接口 + 30 实现 + 数据文件），render 是引擎 GPU 核心，跨模块依赖 4 个（预算内但达上限）。

## 概述

render 模块是 bgfx 渲染核心：BgfxRenderDevice（组合根）+ BgfxDeviceCore（bgfx 生命周期/RTT/视图）+ BgfxDraw/BgfxQuadBatch（绘制/批渲染）+ BgfxShaderManager/EmbeddedShaders（内嵌着色器）+ TextureManager（纹理生命周期+LRU+配额+设备恢复）+ LayerManager（脏矩形合成）+ ParticleSystem + TextRenderer（位图/TTF/FreeType）+ SmaMeshRenderer/SmaSkinner（CPU/GPU 骨骼蒙皮）+ GpuMonitor（自适应降质）+ VideoPlayer（FFmpeg/pl_mpeg）+ RTTManager（画布池）+ ShaderCache。

**亮点**：接口层质量高——api/I*.h 全部纯虚、用引擎自有不透明句柄（RenderTextureHandle/ViewportHandle/MeshHandle）封装 bgfx 类型，未向接口泄漏 bgfx 具体类型；跨模块头文件只 include `../di/api/*.h` 与 `../job/api/IJobSystem.h`（合规）；设备丢失恢复（IDeviceLostListener）体系完备；测试遵循"默认构造+访问器/无 GPU 安全空操作"模式。

## P0 关键问题

**P0-1 通过 `../../external/` 逃逸 src/ 直接拉第三方实现（违反 AGENTS.md §6/§7 "禁止提交包含 `../../../` 或绝对路径的 include"）**
- 位置：
  - `TextureManager.cpp:18` → `#include "../../external/stb/stb_image.h"`（并 `#define STB_IMAGE_IMPLEMENTATION`，把 stb 实现编译进 render）
  - `RTTManager.cpp:2` → `#include "../../external/stb/stb_image_write.h"`（并 `#define STB_IMAGE_WRITE_IMPLEMENTATION`，实现落在此 TU）
  - `BgfxDebugCallback.cpp:2` → `#include "../../external/stb/stb_image_write.h"`（无 IMLEMENTATION 宏，依赖 RTTManager.cpp 提供实现 → 多 TU 隐式耦合）
  - `VideoPlayer.cpp:8` → `#include "../../external/pl_mpeg/pl_mpeg.h"`（并 `#define PL_MPEG_IMPLEMENTATION`）
- 问题：一是 `../../` 逃出模块目录违反 include 铁律；二是 render 模块内部直接编译 3 个第三方库（stb_image / stb_image_write / pl_mpeg），未走统一的 `external/xxx` CMake target 也不经过 `resource` 模块的图片解码，造成隐藏第三方嵌入与编译粒度混乱。
- 修复建议：(a) 根治——CMake 为 external 提供 include 目录常量目标（如 `Caesura::ExternalStb`），render 用裸名 `<stb/stb_image.h>` include，并把 STB/pl_mpeg 实现拆成独立 TU 或统一经 resource 模块；(b) 近期最低——把 `../../external/` 统一改写为根 CMake 注入的裸路径（与 `bgfx/bgfx.h`、`bx/math.h` 同风格）。
- 工作量：S（近期改 include 路径）/ M（根治拆 TU）

**P0-2 接口风格头文件 `src/render/IVideoDecoder.h` 未放在 api/ 且无命名空间/异常设施**
- 位置：`src/render/IVideoDecoder.h`（模块根而非 `src/render/api/`），且 `IVideoDecoder` 不满足 AGENTS.md 命名约定 `I<ModuleName>`：类是全局作用域（无 `namespace Caesura`）、无 `#pragma once`。
- 问题：该头文件定义了一个对外契约（`open/decodeFrame/close`），但按 AGENTS.md 接口必须统一放到 `api/` 子目录并规范化；现状它躺在实现目录，一旦被别的模块 include 就绕过了 api/ 边界。
- 修复建议：若确为对外契约，迁到 `src/render/api/IVideoDecoder.h`、加 `#pragma once`、包 `namespace Caesura`；若仅是内部未来占位（注释自述 "@Beta 暂无实现"），则删除或加内部标记，避免"看似接口实则非标准接口"的误导。
- 工作量：S

## P1 重要问题

**P1-1 跨模块 include 具体 debug 实现头（strict 读法越界；有文档豁免）**
- 位置：`GpuMonitor.cpp:2`、`VideoPlayer.cpp:10` → `#include "../debug/DebugManager.h"`
- 问题：`DebugManager.h` 是 debug 模块的具体实现头（非 `debug/api/IDebugManager.h`）。AGENTS.md §7.4 明确 `DEBUG_*` 宏可直访 `DebugManager::instance()` 为唯一豁免；但更贴合边界纪律的是经 `IDebugManager` 或把 DEBUG_* 宏声明收敛到 api。当前依赖具体头使 debug 实现变化直接波及 render。
- 建议：将 `DEBUG_WARN/INFO/DBG/ERR` 宏的声明（或一个 `debug/api/IDebugManager.h` 可满足的薄封装头）作为 debug 对外 api 提供，render 改 include api 头。属清理项，不阻塞。
- 工作量：S

**P1-2 TextureManager shutdown() 未清 `m_solidCache`/`m_pathToId`，重初始化后旧 id 悬空**
- 位置：`TextureManager.cpp:91-121`（shutdown 未清这两个缓存）
- 问题：`shutdown()` 只清 `m_cache/m_textureSizes/m_textureDimensions/m_restoreSources/m_textureLRU/quota`，不清 `m_solidCache` 与 `m_pathToId`；而 `initialize()` 把 `m_nextId` 重置为 1。若同实例 shutdown 后再 initialize，旧的 solid/path 缓存 id 指向新会话新分配的纹理 id，导致错误复用/误命中。
- 建议：shutdown() 里补 `m_solidCache.clear(); m_pathToId.clear();`（或 initialize 里清）。
- 工作量：S

**P1-3 TextRenderer 批缓存热路径每帧堆分配 + 脏区未真正增量上传**
- 位置：`TextRenderer.cpp:1184-1231`（`rebuildCache` 内 `draws/glyphFromCjk/verts/indices` 每帧 vector 分配）、`1264-1267`（`bgfx::copy` 全量重传 `m_msgCache.vb/ib`）
- 问题：`updateDirtyRange` 算了脏区（dirtyStart/dirtyEnd），但 `rebuildCache` 仍整段重建并全量 `bgfx::update` 越 0 起步重传，脏区计算白做了；消息层文字频繁变化时每帧多次 heap alloc + 全量上传。中文消息层是高频路径。
- 建议：defer 复用 `draws/`、`verts/`、`indices`（成员 buffer 复用），并按 dirty range 用 `bgfx::update(vb, byteOffset, mem)` 做增量子范围上传。属性能优化，非正确性缺陷。
- 工作量：M

**P1-4 TextRenderer 退化三目（死代码）**
- 位置：`TextRenderer.cpp:1164` ```bgfx::TextureHandle tex = (m_ttf && bgfx::isValid(m_fontTexture)) ? m_fontTexture : m_fontTexture;```
- 问题：两个分支同为 `m_fontTexture`，三目无意义；疑为合并遗留。
- 建议：直接 `bgfx::TextureHandle tex = m_fontTexture;`，并复核后续 `hasCjk`/CJK 纹理选择逻辑是否如注释所述正确（TD-13）。
- 工作量：S

**P1-5 SMA 瞬时索引缓冲在分配前未做可用性检查即 memcpy**
- 位置：`SmaMeshRenderer.cpp:361-364`（GPU 路径）、`396-399`（CPU 路径）
- 问题：CPU 路径画顶点前有 `getAvailTransientVertexBuffer/IndexBuffer` 检查（376-377），但 GPU 路径（361）无任何 `getAvailTransientIndexBuffer` 检查就直接 `allocTransientIndexBuffer` 后 `memcpy(tib.data,...)`；若瞬时池耗尽 `tib.data` 可能为空 → 空指针 memcpy。CPU 路径的 IB 分配同样只在稍后隐式依赖（377 检查的是可用性，但 alloc 在 396，未再复核）。
- 建议：两处 `allocTransientIndexBuffer` 前都加 `getAvailTransientIndexBuffer(idxCount)` 检查并在失败时 return。
- 工作量：S

**P1-6 BgfxDeviceCore::getSolidPixel 用 `bgfx::makeRef` 引用栈上数组**
- 位置：`BgfxDeviceCore.cpp:326-329`
- 问题：`const uint8_t pixel[4]` 是栈上局部，`bgfx::makeRef(pixel,...)` 把内存给 bgfx；`createTexture2D` 对多数后端是同步拷贝，但 bgfx 契约里遇到延迟提交/某些后端存在引用超过调用期的风险。同文件 buildCheckerboardTexture 等都正确用 `bgfx::copy`。风险低但应统一。
- 建议：改用 `bgfx::copy(pixel, 4)`。
- 工作量：S

**P1-7 视图常量矛盾：接口声明 `VIEW_TRANSITION=99`，实现用 `=3`**
- 位置：`api/IRenderDevice.h:14`（`constexpr uint16_t VIEW_TRANSITION = 99;`）vs `BgfxDeviceCore.h:21`（`VIEW_TRANSITION = 3`）
- 问题：公开契约说 transition 视图是 99，实现组视图顺序（`BgfxDeviceCore.cpp:102`）用 3。二者不一致，且 `setupDefaultViews` 完全没建 VIEW_TRANSITION 的 rect/clear/transform（99 或 3 都没有初始化），`submitTransition` 依赖的 transition 视图实为未初始化态。
- 建议：统一为单一常量（建议 API 层的 99，或将 core 对齐），并在 `setupDefaultViews` 中为 transition 视图补 `setViewRect/setViewTransform` 初始化；删除重复定义处之一。
- 工作量：S

**P1-8 帧推进接口语义重叠（`advanceFrame/commit_frame/endFrame` 均映射 `bgfx::frame()`）**
- 位置：`BgfxRenderDevice.cpp:170-182` / `BgfxDeviceCore.cpp:187-196`
- 问题：`endFrame()`、`commit_frame()`、`advanceFrame()` 三者内部全调用 `bgfx::frame()`。当前 Engine.cpp 帧循环只在 `render()` 后调一次 `advanceFrame()`（Engine.cpp:736），无重复提帧；但三语义同一实现极易在后续帧循环里误调两次 → 双 present（GPU 管线错序）。
- 建议：收敛为单一 `present()`；`endFrame/commit_frame/advanceFrame` 保留为兼容别名并加断言语义校验（如状态机防同帧重复调用）。
- 工作量：S

## P2 建议

1. **巨型注释噪声**：`BgfxRenderDevice.cpp:155-206` 等多处整段 `// T T T T ...` 占位注释、大量空行/空段落（130-160, 344-404），应清理。
2. **重复死静态变量**：`BgfxRenderDevice.cpp:76` `s_preferredBackend` 与 `BgfxDeviceCore.cpp:11` 重复定义（render 侧未用，恒为 Direct3D11，且与 core 侧非同步）——删 render 侧。
3. **注释编码损坏**：`BgfxRenderDevice.cpp:413,420-421`、`TextRenderer.cpp:28,33,48,417,966` 等注释出现 `??`（本应为 em-dash，因旧编辑器字符集污染），建议统一替换为 `—`（参考 test_source_encoding 修复路径）。
4. **ShaderCache.cpp:24** ```printf("...\\n")``` 字符串里 `\\n` 双反斜杠使日志无换行。
5. **GpuMonitor.h:3-4** 重复 `#include <deque>`；ParticleSystem.h `SIM_BATCH_SIZE` 常量未用；TextureManager.h:44,54 把 `m_solidCache`/`m_pathToId` 暴露为 public（实现细节外露）。
6. **ParticleSystem.cpp:99-101** `emit()` 内循环里每粒子现构 `std::uniform_real_distribution`，可提升为成员或函数级复用（微优化）。
7. **RTTManager 双池索引映射**：`m_handleToPoolIndex` 存 `pool.size()-1`，但 release 用同一索引同时查 `m_pool2D/m_pool3D`，两池共享一个索引空间可能误匹配——当前靠 handle.id 二次校验兜底，仍建议为每个 handle 记录所在池。
8. **CompositeShaderCache 全局单例**（`ShaderCache.cpp:11-14`）不归 BackendRegistry 管，且带 `bgfx::ProgramHandle`；目前主要被 BgfxShaderManager 初始化，跨模块直用需警惕绕过注册表（当前未见跨模块使用，留记录）。且其 `getProgram` 实际永远 fallback（`compileVariant` 只回 Normal），实质是 LUT 而非编译器，注释与实现职责可澄清。
9. **STB_IMAGE_WRITE 实现错位**：`RTTManager.cpp:1` 定义 `STB_IMAGE_WRITE_IMPLEMENTATION` 但该 TU 内未使用 stbi_write 函数；真正消费它的 `BgfxDebugCallback.cpp` 靠它提供实现——两 TU 隐式耦合，建议把实现单独成 TU 或统一到一处。
10. **测试**：已有 `test_render_device/mesh_renderer/render_pipeline/render_integration/layer_manager/particle_system/texture_manager/sma_skinner` 覆盖较好；缺口——`GpuMonitor`（降质/恢复状态机无单元测试）、`RTTManager` 池复用/释放路径、`TextRenderer::rebuildCache` 的 CJK/缓存键命中（`matches()`）行为、`BgfxQuadBatch::flushBatch` 纹理合并组逻辑大多只能靠 GPU 集成测试。

## 耦合分析

render 的跨模块依赖（按 include 归纳）：

| 被依赖模块 | 文件/形式 | 是否合法 |
|---|---|---|
| di | `../di/BackendRegistry.h`（ParticleSystem/TextureManager/VideoPlayer/RTTManager/LayerManager/TextRenderer）+ `../di/api/{ISandboxQuota,ITextureBudget,ThreadAssert,IDeviceLostListener}.h` | ✅ 合法（BackendRegistry 是 §3 唯一访问点，其余是 api/） |
| debug | `../debug/DebugManager.h`（GpuMonitor/VideoPlayer） | ⚠️ 具体头，§7.4 DEBUG 宏豁免但应收敛到 api |
| audio | `../audio/api/IAudioBackend.h`（VideoPlayer） | ✅ api/ |
| job | `../job/api/IJobSystem.h`（VideoPlayer.h） | ✅ api/ |
| external（非模块） | stb_image / stb_image_write / pl_mpeg（`../../external/…`） | ❌ P0-1 |

**跨模块数：4**（di/debug/audio/job）。AGENTS.md §9：业务模块预算 ≤4、超 5 必须先解耦。render **恰好 4，达标但已至上限**，且其中 debug 为具体头依赖、external 为第三方实现逃逸——建议后续新增依赖前先处理 P0-1 与 P1-1 以留余量。

反向：minigame 仅 include `render/api/IRenderDevice.h`（合规）；entry/script 经 BackendRegistry 访问 render 接口（合规）。render 头文件层面未向其他模块泄漏 `bgfx::*` 具体类型（全部经 api/ 不透明句柄）。无循环依赖（render→di/debug/audio/job，均不反向 include render 实现）。

## 审查结论

render 模块整体**健康**：接口契约遵守模块边界铁律（纯虚、引擎自有句柄、api/ 隔离）、BackendRegistry 为唯一后端访问点、设备丢失恢复与配额机制完善、测试覆盖到位（无 GPU 的安全空操作模式符合规范）。无循环依赖、无非组合根 new 后端、接口未暴露 bgfx 具体类型。

需处理：**P0-2 项**——(1) 三处 `../../external/` 第三方 include 逃逸 src/（违反 include 铁律，建议 CMake 注入裸 include 路径或拆 TU）；(2) `IVideoDecoder.h` 契约头未入 `api/` 且命名不规范。**P1 建议 8 项**——重点是 TextureManager 重初始化缓存悬空、SMA 瞬时 IB 未检可用性即 memcpy、VIEW_TRANSITION 常量矛盾、文本批缓存热路径堆分配、帧推进接口语义重叠。全部可 S/M 工作量内修复，无推倒重来项。
