# Task 05 — Release Candidate Gate

## 目标

在平台工作基本收口后，建立最终 RC 门禁。

## RC 必须验证

### Core
- C++ tests 100%
- Lua tests 100%
- coupling PASS
- no known P0
- no known save corruption

### First-VN
- Desktop PASS
- Web PASS
- Android PASS
- iOS PASS（如果声明支持）

### Web

当前已经达到 RC-ready，只允许 regression 修复。

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

### Android

必须使用 latest HEAD validation。

### iOS

只有真机通过才能升为 verified。

### Packaging

每个平台必须：
- clean build
- clean package
- clean install
- clean launch

不能只在开发机工作。

## Release Evidence Bundle

新增：

```text
artifacts/release/
├── manifest.json
├── platform-status.json
├── parity/
├── checksums/
└── reports/
```

manifest 至少包含：

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

以下任何一项存在，RC 不得宣布：
- crash
- save/load corruption
- incorrect branch result
- missing CJK
- broken input
- broken package
- broken platform lifecycle
- broken audio resume
- platform-specific gameplay semantics

## Non-blockers

可以接受：
- visual polish
- editor UX imperfections
- documentation typos
- optional experimental integrations

但必须记录。

## API Freeze

进入 RC 后：
- 不新增大量 KAG commands
- 不改变已有 command semantics
- 不进行大型 rendering rewrite
- 不进行大型 editor rewrite

除非真实 release blocker 要求。

## 最终输出

生成：
`docs/status/release-candidate-report.md`

最终状态：
`RC-GO`
或
`RC-NO-GO`

禁止模糊结论。
