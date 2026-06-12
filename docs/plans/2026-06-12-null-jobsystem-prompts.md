# NullJobSystem 执行提示词

## 通用前置约束

```
你是 Caesura (AmeKAG) 引擎的开发者。先读 AGENTS.md。

关键规则：
- 新文件放 tests/mocks/ 目录
- 所有后端访问走 BackendRegistry::instance().get*()
- 一步一提交，构建零错误 + 295 测试全绿
- 构建：cd build && cmake --build . --config Debug --parallel
- 测试：cd build\tests\Debug && .\CaesuraTests.exe
```

---

## Step 1: 创建 NullJobSystem 头文件

```
### 任务：创建 NullJobSystem Mock 类

### 执行

创建 tests/mocks/NullJobSystem.h：

```cpp
#pragma once
#include "../../src/job/api/IJobSystem.h"
#include <cstdint>

namespace Caesura {

// NullJobSystem -- synchronous mock for unit testing
// All submitted work runs immediately on the calling thread.
// No worker threads, no queues, no waiting.
// Use in tests to isolate modules from real JobSystem dependency.

class NullJobSystem : public IJobSystem {
public:
    NullJobSystem() = default;
    ~NullJobSystem() override = default;

    void init() override {
        m_running = true;
    }

    void shutdown() override {
        m_running = false;
        m_jobId = 1;
    }

    uint64_t submit(JobFn work,
                    JobPriority priority = JobPriority::Normal,
                    MainThreadFn onComplete = nullptr) override {
        (void)priority;
        if (!m_running) return 0;
        uint64_t id = m_jobId++;
        if (work) work();
        if (onComplete) onComplete();
        return id;
    }

    void pollMainThreadJobs() override {
        // No-op: all jobs already completed synchronously
    }

    void waitIdle() override {
        // No-op: synchronous mode, nothing pending
    }

    int workerCount() const override { return 0; }
    int pendingJobs() const override { return 0; }
    bool isRunning() const override { return m_running; }

private:
    bool m_running = false;
    uint64_t m_jobId = 1;
};

} // namespace Caesura
```

### 验收
- 文件存在
- 构建通过（添加到 CMakeLists 或直接 include）
- 提交
```

---

## Step 2: 创建 NullJobSystem 测试

```
### 任务：验证 NullJobSystem 行为正确

### 执行

创建 tests/cpp/test_null_jobsystem.cpp：

测试用例（至少 4 个）：
1. "NullJobSystem: init sets running" → isRunning() == true
2. "NullJobSystem: submit executes work synchronously" → 提交任务设置标志 → 检查标志已设置
3. "NullJobSystem: submit executes onComplete synchronously" → 提交任务设置回调标志 → 检查标志已设置
4. "NullJobSystem: shutdown stops running" → shutdown() → isRunning() == false
5. "NullJobSystem: submit after shutdown returns 0" → shutdown → submit → 返回 0
6. "NullJobSystem: pendingJobs always 0" → submit 后 pendingJobs() == 0

### 实现注意事项
- 需要 include "mocks/NullJobSystem.h"
- 更新 tests/CMakeLists.txt 添加新文件和 include 路径

### 验收
- 6 个测试用例全部通过
- 295 → 301 测试
- 提交
```

---

## Step 3: 改造现有测试使用 NullJobSystem

```
### 任务：替换 AsyncLoader/TextureManager 测试中的真实 JobSystem

### 执行

修改 tests/cpp/test_async.cpp：
- 在需要 JobSystem 的 TEST_CASE 中：
  - 创建 NullJobSystem 实例
  - BackendRegistry::instance().setJobSystem(&nullJob)
  - 测试结束时恢复（或依赖 tearDown）

修改 tests/cpp/test_render_integration.cpp（如有涉及）：
- 同上模式

### 关键约束
- 不能破坏不使用 NullJobSystem 的测试（保持向后兼容）
- 每个测试用例独立创建/销毁 NullJobSystem
- 不全局替换（让真实 JobSystem 的测试继续验证线程安全）

### 验收
- 所有测试全绿（包括使用 Null 和真实 JobSystem 的）
- 构建零错误
- 提交
```

---

## 验收总清单

- [ ] `tests/mocks/NullJobSystem.h` 存在，实现 8 个 IJobSystem 纯虚方法
- [ ] `tests/cpp/test_null_jobsystem.cpp` 存在，6 个用例全绿
- [ ] 至少 2 个现有测试改用 NullJobSystem 且通过
- [ ] 295 → 305+ 测试全绿
- [ ] 构建零错误
