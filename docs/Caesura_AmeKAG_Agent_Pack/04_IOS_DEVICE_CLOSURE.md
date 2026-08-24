# Task 04 — iOS Real Device Closure

## 目标

将 iOS 从：
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
> 停止在 hardware-gated 状态，不得伪造结果。

## I0 — Build

验证：

```bash
cmake .. -G Xcode   -DCMAKE_SYSTEM_NAME=iOS   -DCMAKE_OSX_SYSROOT=iphoneos   -DCMAKE_OSX_ARCHITECTURES=arm64   -DCMAKE_BUILD_TYPE=Release
```

然后执行真实 xcodebuild。

必须得到真实 iOS target。

## I1 — Metal

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

不要把 Metal shader compile 当作 Metal runtime verified。

## I2 — Lifecycle

验证：
- app launch
- background
- foreground
- audio interruption
- screen lock/unlock
- orientation
- touch
- safe area
- resize

## I3 — Storage

验证：
- app sandbox root
- save
- quicksave
- autosave
- load
- restart persistence

路径不能写死。

## I4 — Audio

通过 `AudioFocusService` 连接：
`AVAudioSession / iOS interruption`

验证：
- BGM
- SE
- focus lost
- focus gained
- interruption begin/end

## I5 — CJK / IME

验证：
- Chinese
- Japanese
- English
- virtual keyboard
- text field
- candidate/composition events

## I6 — Packaging

完成：

`Archive → signed app → install device → launch`

再进入：
`TestFlight`

## 验收

新增：
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

最终状态只有在：
> 真机 First-VN 完整运行

之后，才能写：
`ios.real_device = verified`

否则继续：
`ios.real_device = hardware-gated`
