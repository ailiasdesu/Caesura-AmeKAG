# 2026-08-12-003 — P1-5 rollback 内存成本压测与优化

> 市场分析路线图 P1#5：rollback 内存成本压测（超长对话场景）。
> 结论先行：**text_state.draws 浅拷贝优化使 64 快照栈内存 -85.5%**，
> 典型 VN 规模 64 快照 ~1 MB，上限有效，无需进一步优化。

## 背景

`scripts/kag/snapshot.lua` 的 `capture()` 在每个推进点击时深拷贝
f/sf/tf/mp/variables + call_stack + **text_state（含 draws 绘制指令数组）**
+ layers，压入 `_undoStack`（上限 64）。超长对话场景（2000+ 行）下
每快照成本与栈总内存需要实测验证。

## 压测方法（本机实测）

生成 2000 行对话 + 变量周期性增长场景，headless mock 驱动
kag_runner 逐步点击（8000 clicks），`collectgarbage("count")` 采样
每 200 click 的进程内存与栈深度。

## 结果（优化前）

- 栈满 64 后内存**仍线性增长**（每 200 click +64 KB）——每快照
  深拷贝的 text_state.draws 数组随对话增长（2000 draws × 64 快照
  的深拷贝是主导成本）
- 极限场景（2000 draws + 2000 vars）：**566 KB/快照，64 栈 36 MB**

## 优化：text_state.draws 浅拷贝（提交中）

`copy_text_state()`：draws **数组**复制（新表）、**条目共享引用**。
安全性论证：
- draws 正常播放是 **append-only**（`draws[#draws+1] = draw`）；
  仅 `remove_group` 整体重建数组（替换引用，从不原地改旧数组）
- draw 条目是不可变值表——恢复时按快照长度截断/整体替换，重放
  append 新条目，**从不修改旧条目**
- 实测：2000 draws 深拷贝 517 KB vs 浅拷贝 32 KB（-93.8%）

## 结果（优化后）

| 场景 | 优化前 | 优化后 | 改善 |
|---|---|---|---|
| 极限（2000 draws + 2000 vars） | 566 KB/快照 | 82 KB/快照 | **-85.5%** |
| 典型 VN（300 draws + 200 vars） | — | **15.7 KB/快照** | — |

- **64 栈总内存**：典型 ~1 MB，极限 ~5.2 MB——完全可接受
- **上限验证**：64 栈上限有效（满后内存稳定，无失控）；深拷贝
  变量表（剩余主成本）是 rollback 隔离语义的固有需求（emb/eval
  strict 模式替换引用，restore 必须整体换表），不做共享
- 压测结论：**无需进一步优化**；快照隔离正确性由既有
  test_rollback（5 用例）保证

## 回归防护

`tests/scripts/test_rollback_memory.lua`（11 断言）：
- draws 浅拷贝语义（数组独立/条目共享/append 不泄漏）
- 变量深拷贝隔离（rollback 正确性不变）
- restore 整体替换（draws/vars 内容恢复）
- **内存预算门禁**：典型 64 栈 <3 MB、极限 <12 MB（3x 裕量）
  ——回退到深拷贝或快照膨胀会立即 FAIL

## 验证

- Lua 套件 103/103（+test_rollback_memory 11 断言）
- C++ 605/605、ctest 10/10、耦合 PASS、全量重建零错误
