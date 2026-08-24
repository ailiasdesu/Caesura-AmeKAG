# Caesura-AmeKAG Agent Prompt — 1.x Release Candidate / Platform Consistency

仓库：
`https://github.com/ailiasdesu/Caesura-AmeKAG`

## 使命

把当前已经高度完整的 Caesura Runtime，推进到真正可信的跨平台 Release Candidate。

当前阶段不再以“增加功能数量”为主要目标，而以：

- 平台一致性
- 最新 HEAD 回归
- iOS 真机闭环
- Android 最新 HEAD 回归
- Web RC 维护
- 统一平台状态
- First-VN parity
- 发布证据

为主要目标。

---

# 1. 当前项目事实

当前仓库已经具备：

- 16 module architecture
- 34 pure-virtual interfaces
- 123 KAG Neo-Genesis command contracts
- 82 tracked capabilities
- Project Manager / Asset Browser / Debugger / Build / Packaging / LSP
- First-VN E2E
- Web RC-ready
- Android real-device full-cycle evidence
- Android release signing / AAB pipeline
- IME bridge
- GestureDetector
- AudioFocusService
- 大量 C++ / Lua / Web / Editor tests

Web 已经通过完整 W8 gate，不应重新作为大功能开发线；除 regression 外，不要扩大 Web API。

Android 已有真实小米设备闭环，但必须针对最新 HEAD 重新验证，不得永久依赖旧 closure 文档。

iOS 的 CI/toolchain/Metal 仍不等于真机完成。真实 iPhone/iPad 运行、音频会话、签名、TestFlight 仍属于 hardware-gated 工作。

---

# 2. 总优先级

## P0

1. 最新 HEAD 的平台回归
2. First-VN cross-platform parity
3. iOS 真机闭环
4. Android 最新 HEAD 真机回归
5. Release blocker / crash / save corruption / package failure
6. 状态文档一致性

## P1

1. Web RC maintenance
2. Platform service abstraction polish
3. Diagnostics / crash evidence
4. Release packaging polish

## P2

1. Creator UX polish
2. Documentation polish
3. Performance optimization

## P3

暂缓：

- 大型新 Runtime feature
- 大量新 KAG commands
- Unity 级 Timeline
- Marketplace
- Plugin marketplace
- 大型 AI authoring expansion
- 全新 rendering architecture

---

# 3. Agent 总规则

1. 修改前先读 `AGENTS.md`。
2. 优先复用现有 service/interface。
3. 不得为一个平台单独复制一套上层业务逻辑。
4. 平台专属代码应停留在 Platform/Backend 层。
5. 不得用 retry、sleep、ignore、continue-on-error 隐藏真实失败。
6. 不得把 CI probe 写成 real-device verified。
7. 不得把旧 commit 的验证结果自动升级为 latest HEAD verified。
8. public behavior 改动必须有 regression test。
9. 修 bug 必须优先修 root cause。
10. 新增 API 前必须证明现有 API 无法合理表达需求。

---

# 4. Evidence > Claims

严格区分：

1. Implemented
2. CI verified
3. Real-device verified
4. External verified

不得跨级。

任何“verified / release-ready / complete”声明都必须能够提供：

- 当前 commit SHA
- 测试命令
- 平台
- 设备（如有）
- 日志 / screenshot / report

没有真实设备时：

`HARDWARE-GATED`

没有真实凭据时：

`CREDENTIAL-GATED`

不要伪造完成。

旧 commit 的验证结果不能直接证明最新 HEAD。

---

# 5. 第一步：统一平台状态

建立：

`docs/status/platform-matrix.yaml`

建议状态：

```yaml
version: 1

platforms:
  windows:
    build: verified
    runtime: verified
    first_vn: verified
    packaging: verified
    release: pending

  linux:
    build: verified
    runtime: verified
    first_vn: verified
    packaging: verified
    release: pending

  web:
    build: verified
    runtime: verified
    first_vn: verified
    browser: verified
    release_candidate: verified

  android:
    build: verified
    runtime: verified
    first_vn: verified
    real_device: verified
    signing: verified
    aab: verified
    release: pending

  macos:
    build: probe
    runtime: pending
    first_vn: pending
    real_device: hardware-gated
    release: pending

  ios:
    build: probe
    metal: probe
    runtime: pending
    real_device: hardware-gated
    signing: pending
    testflight: pending
```

状态枚举必须严格限制为：

- `verified`
- `probe`
- `pending`
- `hardware-gated`
- `credential-gated`
- `blocked`
- `not-applicable`

禁止：

- almost done
- basically done
- ready-ish
- complete（没有 evidence）

每个 verified 状态都必须能定位到：

```yaml
evidence:
  commit: "<sha>"
  document: "<path>"
  test: "<command>"
  verified_at: "<timestamp>"
```

同时生成：

`docs/status/platform-status.md`

并建立 schema validation。

---

# 6. 第二步：First-VN Cross-Platform Parity

使用同一个：

`tests/projects/first_vn/`

不要复制另一套 story。

目标：

> Script state / progression / choice / save semantics / localization / engine state 一致。

## 必须验证

### Scene progression

记录：

- scene ID
- dialogue index
- choice ID
- ending ID

### Choice

至少：

- branch A
- branch B

### Save

验证：

- manual save
- quick save
- autosave
- load
- save after branch
- state restoration

### Localization

至少：

- zh
- en
- ja

### Audio

验证：

- BGM start
- SE trigger
- lifecycle pause/resume

### Input

Desktop：
- mouse

Web：
- pointer/touch where supported

Android：
- touch

iOS：
- touch

建立轻量级：

`FirstVNStateSnapshot`

只包含：

- current label
- choice result
- relevant flags
- language
- save slot
- ending

禁止把 GPU/OS 数据放入 parity snapshot。

每个平台生成：

`artifacts/parity/<platform>.json`

然后提供：

`scripts/compare_platform_parity.py`

要求：

```text
desktop == web == android == ios
```

对于尚未可验证的平台：

`status = hardware-gated`

不能伪造 pass。

如果出现平台剧情差异：

先找 Runtime / Platform abstraction 根因。

不要在 story 里添加：

```lua
if android then ...
if ios then ...
if web then ...
```

---

# 7. 第三步：Android 最新 HEAD 真机回归

旧 Android closure 只证明历史 commit。

现在必须重新证明：

> 最新 HEAD 仍然闭环。

优先设备：

- Xiaomi 11 / alioth
- Snapdragon 888
- Adreno 660
- Android 14

如果设备不同，记录：

- model
- SoC
- GPU
- Android version
- ABI
- APK SHA256
- commit SHA

## 必测

### Boot

- install
- launch
- FirstVN discovery
- no linker error
- no crash

### Rendering

- background
- character
- message
- CJK
- multi-texture
- stable frame presentation
- no black-frame flicker

### Input

- tap
- choice
- long press
- pinch if available
- orientation

### Save

- manual
- quicksave
- autosave
- load
- screenshot thumbnail

### Lifecycle

- background
- foreground
- rotation
- audio focus

### IME

- virtual keyboard
- text input
- text editing
- `[input]`
- CJK input

### Memory

- repeated scene transition
- repeated save
- repeated load
- stress scene
- low-memory callback if controllable

## 特别检查近期修复

- R8 → RGBA8 glyph atlas
- bgfx transient buffer multi-submit
- RTT / Texture ID collision
- physical → logical coordinates
- screenshot mid-frame capture
- GLES shader path
- Android orientation
- IME
- GestureDetector

生成：

`docs/platform/android-latest-head-validation.md`

必须记录：

- commit
- device
- APK SHA256
- exact commands
- logs
- screenshots
- PASS/FAIL
- known limitations

不得仅引用旧：

`docs/plans/2026-08-24-028-android-full-closure.md`

作为最新 HEAD 证据。

---

# 8. 第四步：Web 只做 RC Maintenance

Web 已经达到 RC-ready。

因此不要重新扩大 Web feature scope。

只允许：

- regression
- crash
- save corruption
- package failure
- security
- browser compatibility blocker

保持：

- Chrome
- Edge
- offline
- subpath
- CJK
- audio
- save/load
- stress
- tab suspend/resume

除非出现 release blocker，否则不要设计新的 Web API。

---

# 9. 第五步：iOS 真机闭环

目标：

从：

> toolchain / CI probe / architecture prepared

推进到：

> real-device verified

## 前提

需要：

- macOS 14+
- Xcode 15+
- iOS 17+ SDK
- Apple Developer signing environment
- iPhone/iPad

如果没有 Mac/iPhone：

> 保持 hardware-gated，不得伪造。

## I0 Build

验证：

```bash
cmake .. -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Release
```

然后进行真实 `xcodebuild`。

## I1 Metal

验证：

- renderer initialization
- shader compilation
- embedded shader loading
- texture upload
- RGBA8 font atlas
- post-FX fallback
- RTT
- SMA CPU fallback
- Live2D path if enabled

不要把：

> Metal shader compile

等同于：

> Metal runtime verified

## I2 Lifecycle

验证：

- launch
- background
- foreground
- audio interruption
- screen lock/unlock
- orientation
- touch
- safe area
- resize

## I3 Storage

验证：

- sandbox root
- save
- quicksave
- autosave
- load
- restart persistence

路径不得写死。

## I4 Audio

通过 `AudioFocusService` 对接：

`AVAudioSession / iOS interruption`

验证：

- BGM
- SE
- focus lost
- focus gained
- interruption begin/end

## I5 CJK / IME

验证：

- Chinese
- Japanese
- English
- virtual keyboard
- text field
- composition events

## I6 Packaging

完成：

```text
Archive
→ signed app
→ install
→ launch
```

然后才是：

```text
TestFlight
```

输出：

`docs/platform/ios-device-validation.md`

必须包含：

- hardware
- iOS version
- Xcode version
- commit
- app build version
- test list
- screenshots
- logs
- known issues

只有真实 iOS First-VN 完整运行后，才允许写：

`ios.real_device = verified`

---

# 10. Platform Service 架构要求

优先形成：

```text
Shared Game Logic
        ↓
Stable Platform Interface
        ↓
Desktop / Web / Android / iOS Backend
```

而不是：

```cpp
#ifdef ANDROID
...
#elif IOS
...
#endif
```

不要把平台差异泄漏到 KAG / Lua / Story layer。

优先关注：

- Display
- Input
- Lifecycle
- Storage
- AudioFocus
- Clipboard
- URL
- Haptics
- PlatformInfo

如果发现多个平台重复实现同一业务逻辑：

> 优先收敛到 Platform Service。

---

# 11. Handle 类型要求

如果发现以下类型共享裸整数：

- Texture
- RTT
- Audio
- Font
- Resource

优先考虑强类型：

```cpp
TextureHandle
ViewportHandle
AudioHandle
FontHandle
```

避免同一个 `uint32_t` 跨 namespace 混用。

但禁止为了“重构而重构”。
只有发现真实 bug、类型歧义或长期维护风险时才推进。

---

# 12. Bug 修复规则

每个真实 bug：

```text
Reproduce
↓
Root Cause
↓
Fix
↓
Regression Test
↓
Real Validation
```

禁止长期用：

- retry
- sleep
- ignore
- continue-on-error
- repeat-until-pass

隐藏真正失败。

临时 workaround 必须标注：

`TEMPORARY MITIGATION`

并建立后续任务。

---

# 13. 最终 RC Gate

执行完上述任务后，建立最终 RC 门禁。

## Core

- C++ tests 100%
- Lua tests 100%
- coupling PASS
- no known P0
- no known save corruption

## First-VN

- Desktop PASS
- Web PASS
- Android PASS
- iOS PASS（如果声明支持）

## Packaging

每个平台必须：

- clean build
- clean package
- clean install
- clean launch

不能只在开发机工作。

## Release Evidence Bundle

生成：

```text
artifacts/release/
├── manifest.json
├── platform-status.json
├── parity/
├── checksums/
└── reports/
```

manifest 至少：

```json
{
  "version": "...",
  "commit": "...",
  "platforms": {
    "windows": "...",
    "linux": "...",
    "web": "...",
    "android": "...",
    "ios": "..."
  }
}
```

## Release Blockers

以下任何一项存在：

- crash
- save/load corruption
- incorrect branch result
- missing CJK
- broken input
- broken package
- broken platform lifecycle
- broken audio resume
- platform-specific gameplay semantics

不得宣布 RC-GO。

最终输出：

`docs/status/release-candidate-report.md`

状态只能是：

```text
RC-GO
```

或：

```text
RC-NO-GO
```

不得给模糊结论。

---

# 14. API Freeze

进入 RC 后：

- 不新增大量 KAG commands
- 不改变已有 command semantics
- 不进行大型 rendering rewrite
- 不进行大型 editor rewrite

除非真实 release blocker 要求。

新增 API 必须先回答：

1. 现有 API 为什么不能表达？
2. 为什么不能复用已有 service？
3. 为什么值得成为长期 contract？

---

# 15. Agent 每次任务结束必须报告

```text
## Status
DONE / PARTIAL / BLOCKED

## What Changed
...

## Root Cause
...

## Files Changed
...

## Tests
...

## Real Device
...

## Browser
...

## Packaging
...

## Evidence
...

## Known Issues
...

## Release Impact
P0 / P1 / P2 / NONE

## Follow-up
...
```

---

# 16. 最终目标

不是：

> “再实现 20 个 feature”。

而是：

```text
同一个 First-VN
        ↓
Windows
Linux
Web
Android
iOS
        ↓
相同剧情状态
相同选择结果
相同存档语义
相同本地化语义
        ↓
真实平台验证
        ↓
Release Candidate
```

---

# 17. 立即执行顺序

现在开始：

### STEP 1
根据当前 HEAD 重新核对仓库状态。

### STEP 2
执行平台状态收口：
`docs/status/platform-matrix.yaml`

### STEP 3
执行：
`First-VN Cross-Platform Parity`

### STEP 4
执行：
`Android Latest HEAD Regression`

### STEP 5
有 Mac/iPhone 环境后执行：
`iOS Real Device Closure`

### STEP 6
最终执行：
`Release Candidate Gate`

如果一个任务已经完成，不要重新实现；确认现有证据，然后继续下一项。

如果某项因为缺硬件或凭据无法验证：

> 明确记录 BLOCKED / HARDWARE-GATED / CREDENTIAL-GATED，并继续可以独立执行的工作。

当前主线是：

> **Validation → Consistency → Release**

而不是：

> **Feature Expansion**
