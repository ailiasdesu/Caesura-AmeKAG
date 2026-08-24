# Task 02 — First-VN Cross-Platform Parity

## 目标

建立同一个 First-VN 在 Desktop / Web / Android / iOS 之间的行为一致性验证。

目标不是像素完全一致，而是：
> Script state / progression / choice / save semantics / localization / engine state 一致。

## 测试项目

使用：
`tests/projects/first_vn/`

不要复制另一套 story。

## 必须验证

### Scene progression
统一记录：
- scene ID
- dialogue index
- choice ID
- ending ID

### Choice
至少验证：
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
Desktop: mouse
Web: pointer/touch where supported
Android: touch
iOS: touch

## State Snapshot

新增轻量级平台无关的 `FirstVNStateSnapshot`。

只包含：
- current label
- choice result
- relevant flags
- language
- save slot
- ending

禁止把 GPU/OS 数据放入 parity snapshot。

## 输出

每个平台生成：
`artifacts/parity/<platform>.json`

示例：

```json
{
  "platform": "android",
  "story": "first_vn",
  "ending": "sunset",
  "language": "zh",
  "flag_is_sun": true,
  "save_roundtrip": true
}
```

然后执行：
`scripts/compare_platform_parity.py`

要求：
`desktop == web == android == ios`

对于尚未可验证的平台：
`status = hardware-gated`

不能伪造 pass。

## 验收

至少完成：
- Windows
- Linux
- Web
- Android

iOS 在设备可用后立即接入。

## 额外目标

保证同一个 `story.ks` 不因为平台条件而产生不同的剧情结果。

如果出现不同：
先找脚本状态 / platform abstraction 根因，而不是在 story 中添加平台 if/else。
