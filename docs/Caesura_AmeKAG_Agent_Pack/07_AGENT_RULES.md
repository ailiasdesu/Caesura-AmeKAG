# Caesura Agent Iron Rules

## 1. Status must be evidence-based

严格区分：
- Implemented
- CI verified
- Device verified
- External verified

禁止跨级。

## 2. 不允许历史证据冒充最新 HEAD

旧 commit 的 screenshot / benchmark / device run 不能直接证明最新 HEAD。

每个 release claim 必须有 current commit SHA。

## 3. 不允许用 flaky workaround 隐藏失败

禁止长期用：
- retry
- repeat-until-pass
- sleep
- ignore-failure
- continue-on-error

掩盖真实 bug。

## 4. 平台业务逻辑不得复制

优先：
`Platform Service → stable interface → shared game logic`

避免把游戏逻辑散落在 `#ifdef ANDROID / IOS` 中。

## 5. Handle 必须有明确类型语义

避免一个整数同时作为 texture / RTT / audio / font / resource handle。

优先强类型 handle。

## 6. 新 API 必须有价值

新增接口前必须回答：
1. 现有接口为什么不能表达？
2. 为什么不能复用已有 service？
3. 为什么这个接口值得成为长期 contract？

## 7. 每个真实 bug 都必须留下回归测试

修复 = code + regression test + evidence。

## 8. Web 已经是 RC-ready

只做 regression、release blocker、security、packaging fixes。

## 9. Android 已经有历史 real-device closure

重要 Runtime 修改后必须重新跑 latest-head smoke。

## 10. iOS 没有真机就保持 hardware-gated

CI 编译/Metal shader probe 不等于真实设备验证。

## 11. 文档状态必须唯一

以：
`docs/status/platform-matrix.yaml`
为唯一状态源。

## 12. 当前阶段不要做大功能

没有 P0/P1 理由，不主动做：
- 大型 Timeline
- 大型 UI Designer
- Marketplace
- 大量新 KAG command
- 大型 AI system
- rendering rewrite
