# Caesura-AmeKAG Agent Master Orchestrator
## 当前阶段：1.x Release Candidate / Platform Consistency

仓库：`https://github.com/ailiasdesu/Caesura-AmeKAG`

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

## 当前已知事实

当前仓库已经具备：
- 16 module architecture
- 34 pure-virtual interfaces
- 123 KAG command contracts
- 82 tracked capabilities
- Web RC-ready
- Android real-device full-cycle evidence
- Android release signing / AAB pipeline
- IME bridge
- GestureDetector
- AudioFocusService
- Project Manager / Build / Packaging / Debug / LSP
- First-VN E2E
- 大量 C++ / Lua / Web / Editor tests

Web 已经通过完整 W8 gate，不应重新作为大功能开发线；除 regression 外，不要扩大 Web API。

Android 已有真实小米设备闭环，但必须针对最新 HEAD 重新验证，不得永久依赖旧 closure 文档。

iOS 的 CI/toolchain/Metal 仍不等于真机完成。真实 iPhone/iPad 运行、音频会话、签名、TestFlight 仍属于 hardware-gated 工作。

## 总优先级

### P0
1. 最新 HEAD 的平台回归
2. First-VN cross-platform parity
3. iOS 真机闭环
4. Android 最新 HEAD 真机回归
5. Release blocker / crash / save corruption / package failure
6. 状态文档一致性

### P1
1. Web RC maintenance
2. Platform service abstraction polish
3. Diagnostics / crash evidence
4. Release packaging polish

### P2
1. Creator UX polish
2. Documentation polish
3. Performance optimization

### P3
暂缓：
- 大型新 Runtime feature
- 大量新 KAG commands
- Unity 级 Timeline
- Marketplace
- Plugin marketplace
- 大型 AI authoring expansion
- 全新 rendering architecture

## Agent 总规则

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

## 每次任务结束必须报告

```text
## Status
Implemented / Partially implemented / Blocked

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

## Known Issues
...

## Release Impact
...

## Evidence
...
```

## Definition of Done

一个任务只有在以下条件满足时才算 DONE：
- 代码存在
- 测试存在
- 对应平台验证完成
- 文档状态更新
- 没有已知 P0/P1 regression
- 状态矩阵与真实证据一致

如果缺少真实设备或真实凭据：
> 标记 `HARDWARE-GATED` / `CREDENTIAL-GATED`，不得伪造完成。

## 当前执行顺序

先执行：
`01_STATUS_MATRIX.md`

然后并行：
`02_PLATFORM_PARITY.md`
`03_ANDROID_LATEST_HEAD.md`

有 Mac/iPhone 条件后：
`04_IOS_DEVICE_CLOSURE.md`

最后：
`05_RELEASE_CANDIDATE.md`
