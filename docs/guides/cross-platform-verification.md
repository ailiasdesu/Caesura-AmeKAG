# Caesura (AmeKAG) — 跨平台真机验证清单

> 状态：**草稿，待真机执行**（维护者以清单逐项打勾后更新此文，并在
> `docs/design/engine-capability-matrix.md` 的 evidence 列补标注）。
> 关联：`docs/guides/release-process.md`（Windows 发布流程）、`.github/workflows/ci.yml`（三平台 CI）。
> 轮次锚点：round 98（能力矩阵刷新点，79 项）、round 101–102（示例游戏立项 + 后处理特效栈 v1）。

---

## 0. 验证三元组（平台 → 构建工具链 → 渲染后端）

| 平台 | CI job | 编译器 | 真实 GPU 渲染后端（bgfx 动态选择） |
|---|---|---|---|
| Windows x64 | `build-windows`（Debug+Release，MSVC） | MSVC (VS2022) | **D3D11 / D3D12**（主）＋ OpenGL 4.3（次）、Vulkan |
| macOS | `build-macos`（Debug，Clang） | Apple Clang | **Metal**（嵌入 MSL 后端）＋ OpenGL |
| Linux | `build-linux`（Debug，GCC） | GCC | **OpenGL 4.3**（主）＋ Vulkan |

CI 三平台**构建 + 无 GPU 测试**已绿（round 102 基线：C++ 972/972,
Lua 131/131 + 21 孤儿, ctest 10 + AI 跳过, 耦合/覆盖 PASS）。
本清单区分「**CI 已验证**」（构建/无头逻辑已由 CI 覆盖）与「**待真机**」（需真实 GPU/音频设备/人工感官确认，CI 无法覆盖）。

---

## 1. 矩阵总览

> 图例：**[CI]** = 三平台 CI 编译/无头覆盖已绿；**[Win真机]** / **[mac真机]** / **[Linux真机]** = 该平台真机实测状态（_未验证_ = 待打勾）。

| 能力域 | 能力项 | Windows | macOS | Linux | 说明 / 证据 |
|---|---|---|---|---|---|
| **构建** | 全量编译（模块静态库 + 可执行） | [CI] ✓ | [CI] ✓ | [CI] ✓ | 三 job 均 `cmake --build` 零错误；Windows 另做 Release。矩阵 P1/P2 |
| **构建** | 资源/脚本拷贝到 build 输出 | [CI] ✓ | [CI] ✓ | [CI] ✓ | CMake 后处理步骤，三平台一致 |
| **构建** | DXBC shader 二进制完整性（checkout 不被 CRLF 破坏） | [CI] ✓ | — | — | `.gitattributes` 含 `*.dxbc binary`（round 94 同坑已防）；见 §3.1 |
| **测试** | C++ doctest 套件（headless/Null 后端） | [CI] ✓ | [CI] ✓ | [CI] ✓ | ctest `--repeat until-pass:2`；Linux 附 ctest 日志上传 |
| **测试** | Lua 主套件 + 孤儿套件 | [CI] ✓ | [CI] ✓ | [CI] ✓ | run_lua_tests / run_orphan_tests |
| **测试** | Editor vitest + Web 脚本索引保鲜 | [CI] ✓ (仅 Windows) | — | — | ci.yml 仅 Windows job 跑 vitest；见 §4 建议 3 |
| **渲染** | sprite / layer 合成 / 文字 | [Win真机] ✓ (D3D11+GL4.3) | [mac真机] 未验证 | [Linux真机] 未验证 | 矩阵 R1/R7：Windows 真机已验证；Metal/Linux 硬件未验证 |
| **渲染** | **后处理特效链 v1（bloom/vignette/LUT/柔焦）** | [Win真机] ✓ (D3D11) | [mac真机] 待真机 | [Linux真机] 待真机 | round 102 新增；GL/Metal/Vulkan 现为恒等拷贝降级；见 §3.3 |
| **渲染** | 转场 / 滤镜 / RTT viewport blit | [Win真机] ✓ | [mac真机] 未验证 | [Linux真机] 未验证 | 矩阵 R8；2026-08-06 双后端验证在 Windows |
| **渲染** | SMA GPU 蒙皮 / 3D 小游戏骨架 | [Win真机] ✓ (D3D11) | [mac真机] 待真机 | [Linux真机] 待真机 | 矩阵 D10/C2；GL/Metal/SPIR-V 回退 CPU skinner |
| **音频** | 3 总线 / 交叉淡化 / 3D | [Win真机] ✓ | [mac真机] 待真机 | [Linux真机] 待真机 | 矩阵 A1–A4 ✓ 仅指接口/单测；SoLoud 平台后端需真机听测 |
| **存档** | 加密存/读 + schema 迁移 | [CI] ✓ | [CI] ✓ | [CI] ✓ | 矩阵 C3/C4 单测级；真机文件系统/路径未跨平台实测 |
| **Web** | web/scripts-index 生成 + 保鲜 | [CI] ✓ | [CI] ✓ | [CI] ✓ | `gen-index.mjs --check` 三平台跑 |

---

## 2. 逐平台真机验证清单（执行时勾选）

### 2.1 Windows（MSVC，主平台，真机已大部分验证）

> 引用 capability-matrix P1/P2：Windows D3D11+GL 真机像素已验证；锚点 2026-08-06（R7/R8/C2 双后端）。

- [ ] 构建：Debug + Release 全绿（CI 已验证）
- [ ] 渲染：D3D11 真实设备截图比对各像素（sprite/layer/text）——**已做**
- [ ] 渲染：OpenGL 4.3 真实设备截图比对（转场 flash+crossfade、3D 小游戏）——**已做**（2026-08-06）
- [ ] 渲染：**后处理特效栈 v1：bloom/vignette/LUT/柔焦 在 D3D11 真机逐特效目测 + 截图回归**——待执行（round 102 新能力，仅单测/无 GPU gate）
- [ ] 后处理：resize / 窗口大小变化时 m_sceneRtt 重建 + chain 资源释放（shutdown/resize/recoverDevice）真机回归
- [ ] 音频：BGM 交叉淡化 / 语音打断 / SE 3D 听测
- [ ] 存档：真机磁盘读写 + 加密往返（round 79 单测已覆盖逻辑）
- [ ] Web/editor：HTTP editor + stdio RPC smoke（CI 的 headless 已覆盖，GUI 真机可补）

### 2.2 macOS（Clang，Metal）

> 引用 capability-matrix：「Metal engine-side complete；runtime validation requires macOS
> hardware」「remaining gap: Metal backend and macOS/Linux hardware validation」。

- [ ] 构建：macOS Clang Debug 全绿（CI 已验证）
- [ ] 渲染：**Metal 后端首次真实设备启动**（嵌入 MSL 后端 + backend 选择 + Live2D Metal 渲染路径）
- [ ] 渲染：文字 CJK 渲染（FreeType）在 Metal 真机
- [ ] 渲染：**后处理特效栈：Metal 现为恒等拷贝降级，确认真机上 `[set_postfx]` 不崩溃、画面正确（应为原图/近原图）**
- [ ] 渲染：SMA GPU 蒙皮（Metal→CPU 回退路径确认）＋ 3D 小游戏
- [ ] 音频：SoLoud 在 macOS（CoreAudio）听测
- [ ] 存档：`~/Library/...` 或相对路径落盘真机
- [ ] 窗口/输入：SDL3 Cocoa 窗口、事件路由

### 2.3 Linux（GCC，OpenGL）

> 引用 capability-matrix：同 macOS；「GL shader deployment fixed」仅指代码，运行时仍需 Linux 硬件。

- [ ] 构建：Linux GCC Debug 全绿（CI 已验证，含 SDL3 源码编译）
- [ ] 渲染：**OpenGL 4.3 后端真实设备启动**（X11/Wayland）＋ sprite/layer/text
- [ ] 渲染：**后处理特效栈：GL 现为恒等拷贝降级，确认 `[set_postfx]` 不崩溃、画面正确**
- [ ] 渲染：转场/滤镜/RTT viewport blit（矩阵 R7/R8 代码级，Linux 待真机）
- [ ] 渲染：SMA GPU 蒙皮（GL 430 GLSL，若支持）＋ 3D 小游戏
- [ ] 音频：SoLoud（ALSA/PulseAudio）听测
- [ ] 存档：`$HOME` 相对路径落盘真机
- [ ] 窗口/输入：SDL3 于 X11/Wayland

---

## 3. round-102 新增代码三平台风险核对表

### 3.1 `shaders/dx11/*.dxbc`（Windows 专用二进制）— 已核查，无风险

- `.gitattributes` 第 ~31 行已有 `*.dxbc binary`；`git check-attr shaders/dx11/fs_postfx_vignette.dxbc` 返回 `binary: set / text: unset`。
- 16 个 `.dxbc` 经 `git ls-files shaders/dx11/` 全部 tracked，工作树干净，无 CRLF 破坏（round 94 同坑已规避）。
- `.gitattributes` 中 `* text=auto` 在 `*.dxbc binary` 之前，靠更具体的 `*.dxbc` 规则覆盖——**顺序正确**。**无需修改。**

### 3.2 `EmbeddedShaders.cpp`（DXBC 内嵌）— 已核查，非条件编译，无害

- `cmake/CaesuraModules.cmake` line 212 把 `src/render/EmbeddedShaders.cpp` **无条件**加入 Render 模块源列表（无 `if(WIN32)`）。
- 因此 **macOS Clang / Linux GCC 都编译 DXBC 字节数组**。这些是 `const uint8_t[]`，只在 D3D11/D3D12 后端被引用；GL/Metal/Vulkan 走恒等拷贝降级（ROADMAP-200 round 102）。编译无害，且天然获得三平台编译覆盖。**确认。**

### 3.3 `RenderBinding.cpp` 新方法 — 已核查，无平台假设

- `set_postfx` / `destroy_postfx` / `clear_postfx` / `is_postfx_supported` / `is_postfx_active` 全部只调用 `IRenderDevice` 纯虚接口。
- 实现 `BgfxRenderDevice::isPostFxSupported` 是**后端无关**的（`m_bgfxInitialized && m_shaders != nullptr`，不论 D3D/GL/Metal）。
- 含义：macOS/Linux 真机上 `set_postfx` 会返回支持=true，但实际走恒等拷贝降级（round 102 约定）。**不崩溃**，但**特效真实效果待 macOS/Linux 真机验证**——列入 §2.2/§2.3 待真机项。

### 3.4 附带发现（非 CI 阻塞，建议清理）

- `src/render/EmbeddedShaders.h`：round-102 postfx 五个符号的 `extern` 声明块**重复了两次**（每个符号声明 2 次）。重复 `extern` 声明合法、不影响编译/链接，属冗余，建议维护者后续去重（不阻塞合并）。

---

## 4. ci.yml 建议清单（**仅建议，未修改**——是否应用由主代理决定）

| # | 建议 | 风险 / 收益 | 是否需改 ci.yml |
|---|---|---|---|
| 1 | **无需改动**：三平台 CI 已无条件编译 round-102 全部代码（含 DXBC 内嵌数组），编译覆盖充分 | 无 | 否 |
| 2 | **可选增强**：macOS / Linux job 增加「无 GPU 后处理降级」的无头冒烟（headless 下 `is_postfx_supported` 应为 false、`set_postfx` 返回 0）。现有 test_render_postfx.cpp 已覆盖 Null/Bgfx-uninit 门禁，三层 job 的 ctest 均包含它 | 低风险，锁定降级行为 | 可选；若加，建议作为新增测试而非改 yml 结构 |
| 3 | **可选**：把 Editor vitest 从「仅 Windows job」推广到 macOS/Linux（现 ci.yml 只在 build-windows 跑 vitest） | 中成本（每台跑 npm ci + vitest），收益是前端逻辑三平台一致 | 可选 |
| 4 | **可选**：release job（仅 Windows CPack）后续可扩为 macOS dmg / Linux 打包 | 范围外，属产品化阶段 | 否（本期） |

> **结论**：round-102 代码对三平台 CI 是**无风险的**，ci.yml **无需强制改动**。
> 唯一实质缺口是真机验证（§2），不是 CI 配置。

---

## 5. 证据来源

- `docs/design/engine-capability-matrix.md`：P1（跨平台构建覆盖、真机未验）、P2（CI 基线 r98 963/963）、R1/R7/R8/C2/D10、C3/C4、A1–A4。
- `docs/plans/audit/ROADMAP-200.md` round 102：后处理特效栈、GL/Metal/Vulkan 恒等拷贝降级说明。
- `docs/plans/audit/ROADMAP-200.md` 立项说明：阶段 G 目标含「真机验证」。
- `.github/workflows/ci.yml`：三 job 结构、vitest 仅 Windows、ctest repeat、资源处理。
- `cmake/CaesuraModules.cmake` line 212：EmbeddedShaders.cpp 无条件编译。
- `src/render/BgfxRenderDevice.cpp` line 460–506：isPostFxSupported 后端无关 + 真机需求。
