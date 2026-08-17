# 《单程回信》i18n 工作流验证报告
# The One-Way Reply — i18n workflow verification report

> 依据：R112-A 提取/回填工具链实战验证（round 112，master）
> 对象：demo/example_game/story.ks（447 行） ｜ 词典：assets/lang/{zh,en}.lua
> 日期：2026-08-16　　方法：R112-A 工具 + 引擎 Lua 探针　｜　未 git 提交

---

## 0. 结论摘要

| 项 | 结果 |
|---|---|
| {key} 词典引用提取 | 16 个唯一键（story.ks 实体对白），EN/ZH 双覆盖 |
| 覆盖补齐 | **零缺失**（16 键 × 2 语言全部命中） |
| 回填校验 | **ZERO_MISSING = true**（0 / 32 缺失） |
| 引擎验证 | [i18n en/zh] 切换 + {settings}/结局键展开全部 PASS |
| verify_sample_game.sh | **PASS 5/5** |

story.ks 采用 **顶层 {key} 词典引用 + [i18n] 热切换** 的双语方案（见 README.md
"i18n 双语"行），**不使用** lines 逐行翻译表——字面对白已按语言内联成对渲染。
因此词典覆盖目标 = 对白中引用的顶层 {key} 键，全部就绪。

---

## 1. 提取：{key} 词典引用清单（story.ks）

| # | 键 | 引用行号(1-based) | 用途/上下文 |
|---|---|---|---|
| 1 | settings | 34, 37 | "speaks two languages. {settings}"（EN/ZH 各一次，i18n 首秀） |
| 2 | investigate_sigh | 189, 192 | 阁楼风声句（EN/ZH 各一次） |
| 3 | final_choice | 341 | 计时选择提示 "You have a few seconds…" |
| 4 | end_zero_a | 359 | 真结局（归零）第 1 句 |
| 5 | end_zero_b | 361 | 真结局第 2 句 |
| 6 | end_zero_c | 363 | 真结局澪台词 |
| 7 | end_zero_d | 365 | 真结局收束句 |
| 8 | end_companion_a | 380 | 同行结局第 1 句 |
| 9 | end_companion_b | 382 | 同行结局第 2 句 |
| 10 | end_companion_c | 384 | 同行结局第 3 句 |
| 11 | end_companion_d | 386 | 同行结局收束句 |
| 12 | end_promise_a | 401 | 守约结局第 1 句 |
| 13 | end_promise_b | 403 | 守约结局第 2 句 |
| 14 | end_promise_c | 405 | 守约结局澪台词 |
| 15 | end_promise_d | 407 | 守约结局收束句 |
| 16 | items | 63 | 复数条目引用（items={ one=.., other=.. } 表） |

**字面量文本（lines 维度）**：R112-A ks_i18n.lua --dir demo/example_game
提取出 **110 条** content-addressed 对白（2 个场景：story.ks + story_lastletter.ks），
键形如 story.ks:<fnv1a>。本游戏不启用 lines 逐行翻译，故该表为空属正常设计。

---

## 2. 覆盖矩阵（键 × zh × en × 状态）

> 判定逻辑 = R112-A find_key_missing：顶层字典键须存在（排除 lines/_version/_meta）。
> 已用引擎 i18n.load() 实测确认（非正则猜测）。

| 键 | zh.lua | en.lua | 状态 |
|----|:---:|:---:|:---:|
| settings | ✅ "设置" | ✅ "Settings" | ✓ |
| investigate_sigh | ✅ | ✅ | ✓ |
| final_choice | ✅ | ✅ | ✓ |
| end_zero_a | ✅ | ✅ | ✓ |
| end_zero_b | ✅ | ✅ | ✓ |
| end_zero_c | ✅ | ✅ | ✓ |
| end_zero_d | ✅ | ✅ | ✓ |
| end_companion_a | ✅ | ✅ | ✓ |
| end_companion_b | ✅ | ✅ | ✓ |
| end_companion_c | ✅ | ✅ | ✓ |
| end_companion_d | ✅ | ✅ | ✓ |
| end_promise_a | ✅ | ✅ | ✓ |
| end_promise_b | ✅ | ✅ | ✓ |
| end_promise_c | ✅ | ✅ | ✓ |
| end_promise_d | ✅ | ✅ | ✓ |
| items（复数表） | ✅ | ✅ | ✓ |

**汇总**：16 键 × 2 语言 = 32 个单元格，**0 缺失**（ZERO_MISSING=true）。
无需补齐任何键。items 为复数变体表（en 有 one/other，zh 恒取 other），未缺失。

---

## 3. 回填校验（零缺失）

```bash
# 顶层 {key} 词典引用方向（R112-A find_key_missing 语义，脚本实测）
REFS: end_*(12) final_choice investigate_sigh items settings
== coverage zh   → 全命中
== coverage en   → 全命中
ZERO_MISSING=true
COVERAGE: 16 refs x 2 langs, 0 missing   # exit 0
```

> ⚠️ 注意：ks_i18n.lua --dir demo/example_game --lang en --missing（lines 方向）
> 返回 exit 1——因为本词典无 lines 表，而本游戏不用逐行翻译。这是**工具语义**问题，
> 不是内容缺失（详见 §5 工具链需求）。

---

## 4. 引擎验证（lua 探针）

探针：加载 i18n 模块，用 i18n.set_language 切换 en/zh，经 [i18n] 命令
（kag.commands.system + scheduler）驱动，验证关键台词翻译与 {key} 展开。

| 断言 | 结果 |
|---|---|
| en: t(settings)=="Settings" | PASS |
| en: expand("This sample game speaks two languages. {settings}") 含 "Settings" | PASS |
| en: investigate_sigh / final_choice / end_zero_a / end_companion_d / end_promise_c 命中 | PASS |
| en: t(items) 落到 other 通式（无 n 参数） | PASS |
| zh: t(settings)=="设置" | PASS |
| zh: expand("这个示例游戏支持中英双语。{settings}") == "…设置" | PASS |
| zh: investigate_sigh / final_choice / end_zero_c 命中 | PASS |
| 全部 14 键（结局+final_choice+investigate_sigh）EN/ZH 均解析且无残留 { } 花括号 | PASS |
| [i18n en→zh] schedule 后 current_language=="zh" | PASS |

**PROBE ALL PASS**（exit 0）。{settings} 等键在 en/zh 切换后展开正确，含 {settings} 键展开。

---

## 5. 工具链体验与 R112-A 需求

### 就绪部分（R112-A 工具可用）
- scripts/ks_i18n.lua（extract_messages / collect_entries / find_missing / build_template）
  就绪：字面量 lines 提取、模板生成、--update 合并、--missing 门禁全部可用且实测通过。

### 缺口（需求清单 → 给 R112-A）
1. **{key} 顶层词典引用提取/回填未并入 CLI**。仓库存在 scripts/_ks_i18n_additions.lua
   片段（extract_key_refs / collect_key_refs / find_key_missing），定义了本报告采用的
   {key}-方向接口，但 ① 未合并进 ks_i18n.lua 主模块；② CLI 无 --keys（或
   --missing --strings）开关来跑顶层键门禁。**需求：把该片段并入 ks_i18n.lua，
   新增 CLI 选项（如 --keys-missing）打印 {key} 引用缺失清单，缺失时 exit 1。**
2. **--missing（lines 方向）对本游戏误报**。本游戏不用 lines，--missing 必然 exit 1。
   建议 --missing 同时跑两层（lines + {key} 顶层），或对无 lines 表的项目注明
   "uses {key}-only mode" 而非判失败。
3. **行号输出**（本报告人工补）：collect_key_refs 目前只给 first 文本上下文，
   无行号；给翻译/审查加行号更友好（可作 enhancement）。

---

## 6. 验证命令记录

```bash
# 1) 提取（R112-A 工具，lines 方向）
external/lua/lua.exe scripts/ks_i18n.lua --dir demo/example_game --lang en   # 110 msgs / 2 scenes

# 2) 顶层 {key} 提取 + 覆盖（find_key_missing 语义，临时脚本模拟接口）
#    → 16 refs x 2 langs, 0 missing, ZERO_MISSING=true

# 3) 覆盖校验
external/lua/lua.exe scripts/ks_i18n.lua --dir demo/example_game --lang en --missing
#    → exit 1（lines 表为空，见 §5 说明，非真实缺失）

# 4) 引擎探针（en/zh 切换 + {key} 展开）→ PROBE ALL PASS, exit 0

# 5) 全量验证
bash scripts/verify_sample_game.sh   # → PASS 5/5
```

---

## 7. verify_sample_game.sh 结果

| 检查 | 结果 |
|---|---|
| ks_check: 契约零警告 | PASS |
| headless: 全流程跑至 DONE（RESULT DONE:15671 token） | PASS |
| ending_zero 可达 → DONE | PASS |
| ending_companion 可达 → DONE | PASS |
| ending_promise 可达 → DONE | PASS |
| **合计** | **PASS (5/5)** |

Web smoke（信息项）未在前端执行（需折叠进 web bundle），非门禁项。

---

## 8. 未提交 / 环境备注

- 本报告新增：demo/example_game/i18n-report.md（本文件）。**未执行任何 git 提交。**
- 仓库在会话前后均已存在 R112-A 未提交改动：
  scripts/i18n.lua、scripts/ks_i18n.lua、tests/scripts/test_i18n.lua、
  docs/guides/i18n.md、docs/guides/kag-language-tour.md（M），
  nul（??，Windows 重定向残留文件，162B 错误文本，非本报告产物，建议清理）。
- 本次任务创建的临时探针脚本已清空（tmp/_r112_*），不留杂物。
