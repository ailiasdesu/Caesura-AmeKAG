# NullJobSystem 执行情况详细总结

**项目**: Caesura (AmeKAG) — galgame 引擎
**基线**: Final Phase 已完成（295/295 tests）
**分支**: `codex/null-jobsystem`
**日期**: 2026-06-12

---

## 总体结果

| 指标 | 执行前 | 执行后 | 变化 |
|------|--------|--------|------|
| 测试用例 | 295 | **306** | +11 |
| 断言 | 643 | **663** | +20 |
| 构建错误 | 0 | 0 | — |
| 新增目录 | — | 1 | — |
| 新增文件 | — | 3 | — |
| 修改文件 | — | 2 | — |

---

## 背景

当前 295 个测试中，所有涉及 `AsyncLoader`、`TextureManager`（通过异步加载路径）和 `ParticleSystem` 的测试都隐式依赖真实 `JobSystem`，需要启动 worker 线程池。这导致测试慢、偶发不稳定、且违反单元测试隔离原则。

`IJobSystem` 已是纯虚接口（8 个方法），只需实现同步 mock 即可消除这些依赖。

---

## IU-1: 创建 NullJobSystem Mock

**新增文件**: `tests/mocks/NullJobSystem.h`

```cpp
class NullJobSystem : public IJobSystem {
public:
    void init() override { m_running = true; }
    void shutdown() override { m_running = false; m_jobId = 1; }

    uint64_t submit(JobFn work, JobPriority priority, MainThreadFn onComplete) override {
        (void)priority;
        if (!m_running) return 0;
        uint64_t id = m_jobId++;
        if (work) work();          // 同步立即执行
        if (onComplete) onComplete(); // 同步立即回调
        return id;
    }

    void pollMainThreadJobs() override {}  // 无操作：已同步完成
    void waitIdle() override {}            // 无操作：同步模式无需等待
    int workerCount() const override { return 0; }
    int pendingJobs() const override { return 0; }
    bool isRunning() const override { return m_running; }

private:
    bool m_running = false;
    uint64_t m_jobId = 1;
};
```

**关键行为**:

| 方法 | 行为 |
|------|------|
| `init()` | 设置 running=true |
| `shutdown()` | 设置 running=false，重置 jobId |
| `submit(work, priority, onComplete)` | 忽略 priority；shutdown 后返回 0；否则立即同步执行 work() → 立即同步执行 onComplete() → 返回递增 ID |
| `pollMainThreadJobs()` | 空操作 |
| `waitIdle()` | 空操作 |
| `workerCount()` | 返回 0 |
| `pendingJobs()` | 返回 0 |
| `isRunning()` | 返回 m_running |

---

## IU-2: NullJobSystem 单元测试

**新增文件**: `tests/cpp/test_null_jobsystem.cpp`（8 用例）

| 用例 | 验证内容 |
|------|----------|
| `init sets running` | `isRunning()` 初始 false → init → true |
| `submit executes work synchronously` | submit 设置标志 → 标志已设置（同步执行） |
| `submit executes onComplete synchronously` | submit 设置回调标志 → 回调标志已设置 |
| `shutdown stops running` | init → shutdown → `isRunning() == false` |
| `submit after shutdown returns 0` | shutdown → submit → 返回 0，work 不执行 |
| `pendingJobs always 0` | 任意状态下 `pendingJobs() == 0` |
| `workerCount is 0` | 任意状态下 `workerCount() == 0` |
| `multiple submits get unique IDs` | 三次 submit 返回三个互不相同的 ID |

---

## IU-3: 改造现有测试使用 NullJobSystem

**修改文件**: `tests/cpp/test_async.cpp`

新增 NullJobSystem 初始化辅助函数：

```cpp
static NullJobSystem g_nullJob;

static void initNullJobInfra() {
    detail::g_mainThreadId = std::this_thread::get_id();
    g_nullJob.init();
    BackendRegistry::instance().setJobSystem(&g_nullJob);  // 注册到 BackendRegistry
    AsyncLoader::instance().init();                         // AsyncLoader 通过 BR 获取 JobSystem
}

static void shutdownNullJobInfra() {
    AsyncLoader::instance().shutdown();
    g_nullJob.shutdown();
    BackendRegistry::instance().setJobSystem(nullptr);     // 恢复
}
```

**保留的测试**（6 用例，使用真实 JobSystem）:

| 用例 | 目的 |
|------|------|
| `AsyncLoader::singleton` | 单例同一性 |
| `AsyncLoader::shutdown is idempotent` | 两次 shutdown 幂等 |
| `AsyncLoader::enqueue returns positive id` | 入队返回正数 ID |
| `AsyncLoader::rejects path traversal` | 拒绝 `../` 路径 |
| `AsyncLoader::cancelAll clears queue` | 取消全部清空队列 |
| `AsyncLoader::poll does not crash` | poll 不崩溃 |

**新增测试**（3 用例，使用 NullJobSystem）:

| 用例 | 验证内容 |
|------|----------|
| `AsyncLoader with NullJobSystem: enqueue + poll` | NullJobSystem 下 enqueue 返回正 ID，poll 不崩溃 |
| `AsyncLoader with NullJobSystem: cancelAll` | NullJobSystem 下 cancelAll → pendingCount == 0 |
| `AsyncLoader with NullJobSystem: rejects path traversal` | NullJobSystem 下拒绝 `../` 路径 |

**关键依赖链**: `AsyncLoader` 通过 `BackendRegistry::instance().getJobSystem()` 获取 JobSystem（带 fallback 到 `JobSystem::instance()`），因此注册 NullJobSystem 后自动切换。

---

## CMakeLists 修改

`tests/CMakeLists.txt`:

```
+ target_include_directories(CaesuraTests PRIVATE
+     ${CMAKE_CURRENT_SOURCE_DIR}          # 新增：使 tests/mocks/ 可被 include
      ...
+     cpp/test_null_jobsystem.cpp           # 新增
```

---

## 文件变更清单

### 新增（3）

| 文件 | 说明 |
|------|------|
| `tests/mocks/NullJobSystem.h` | IJobSystem 同步 mock 实现（60 行） |
| `tests/cpp/test_null_jobsystem.cpp` | NullJobSystem 单元测试（8 用例） |
| `docs/plans/F1-F4-execution-summary.md` | Final Phase 执行总结（前置会话产物） |

### 修改（2）

| 文件 | 变更 |
|------|------|
| `tests/CMakeLists.txt` | +test_null_jobsystem.cpp，+`${CMAKE_CURRENT_SOURCE_DIR}` include 路径 |
| `tests/cpp/test_async.cpp` | +NullJobSystem 辅助函数，+3 个 NullJobSystem 变体测试，保留原 6 个真实 JobSystem 测试 |

---

## 验证

```
[doctest] test cases: 306 | 306 passed | 0 failed | 0 skipped
[doctest] assertions: 663 | 663 passed | 0 failed |
[doctest] Status: SUCCESS!
```

- 构建: `cmake --build build --config Debug --parallel` — **零错误**
- NullJobSystem 8 个纯虚方法全部实现 ✅
- AsyncLoader 3 个 NullJobSystem 变体测试通过 ✅
- 原真实 JobSystem 测试不受影响（向后兼容）✅

---

## Git 提交

```
4a04d9b feat(null-jobsystem): NullJobSystem mock for synchronous testing
```

分支: `codex/null-jobsystem` → `github.com:ailiasdesu/Caesura-AmeKAG`
