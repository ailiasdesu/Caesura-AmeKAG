# Task 03 — Android Latest HEAD Regression

## 目标

旧的 Android closure 已经证明某个历史 commit 在小米设备上闭环。

现在必须证明：
> 最新 HEAD 仍然闭环。

## 测试设备

优先：
- Xiaomi 11 / alioth
- Snapdragon 888
- Adreno 660
- Android 14

如果设备不同，完整记录：
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
- pinch if test harness supports it
- orientation behavior

### Save
- manual
- quicksave
- autosave
- load
- screenshot thumbnail behavior

### Lifecycle
- background
- foreground
- rotation
- audio focus

### IME
- virtual keyboard
- text input event
- text editing event
- `[input]`
- CJK input

### Memory
- repeated scene transition
- repeated save
- repeated load
- stress scene
- low-memory callback if controllable

## 特别检查

近期修复过：
- RGBA8 glyph atlas
- transient buffer multi-submit
- RTT/Texture ID collision
- physical→logical input mapping
- screenshot capture mid-frame hazard
- GLES shader conversion
- Android orientation

必须验证这些功能在最新 HEAD 没有回归。

## 验收

生成：
`docs/platform/android-latest-head-validation.md`

包括：
- commit
- APK SHA256
- device
- exact commands
- screenshots/log snippets
- PASS/FAIL table
- known limitations

## 禁止

不能简单引用旧 `docs/plans/2026-08-24-028-android-full-closure.md` 作为最新 HEAD 证据。
旧文档只能作为历史证据。
