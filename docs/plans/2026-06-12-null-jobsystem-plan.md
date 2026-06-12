# NullJobSystem 实现计划

> 创建日期: 2026-06-12 | 状态: active

## 目标

创建 `NullJobSystem` —— IJobSystem 的同步 Mock 实现，用于加速测试并消除测试对外部线程的依赖。

## 背景

当前 295 个测试中，所有涉及 `AsyncLoader`、`TextureManager`（通过异步加载路径）和 `ParticleSystem` 的测试都隐式依赖真实 `JobSystem`，需要启动 worker 线程池。这导致测试慢、偶发不稳定、且违反单元测试隔离原则。

`IJobSystem` 已是纯虚接口，只需实现 8 个方法即可完成 Mock。

## 实施

### IU-1: 创建 NullJobSystem

**文件:** `tests/mocks/NullJobSystem.h`

```cpp
class NullJobSystem : public IJobSystem {
    // 所有 submit() 调用在当前线程同步执行
    // work() 和 onComplete() 按调用顺序当场完成
};
```

**关键行为:**
- `submit(work, priority, onComplete)`: 立即执行 `work()` → 立即执行 `onComplete()`
- `pollMainThreadJobs()`: 空操作（已当场执行）
- `waitIdle()`: 空操作（同步模式无需等待）
- `workerCount()`: 返回 0
- `pendingJobs()`: 返回 0
- `isRunning()`: 返回 false

### IU-2: 集成到测试基础设施

- `NullJobSystem` 注册到 `BackendRegistry`
- 使用 `NullJobSystem` 的测试在 setUp 中注册，tearDown 中恢复
- 至少改造 `test_async.cpp` 的测试用例验证 Mock 有效

### IU-3: 验证

- 构建零错误
- 使用 NullJobSystem 的测试全部通过
- 使用真实 JobSystem 的测试不受影响
- 295 基线测试全绿

## 验收

- [ ] `tests/mocks/NullJobSystem.h` 文件存在
- [ ] `NullJobSystem` 实现 IJobSystem 的 8 个纯虚方法
- [ ] 至少 3 个测试用例使用 NullJobSystem 并通过
- [ ] 测试执行时间不增加（预期减少）
