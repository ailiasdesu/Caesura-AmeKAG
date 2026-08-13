# Git 工作流

## 提交信息格式

```
<type>: <description>

<optional body>
```

类型: feat, fix, refactor, docs, test, chore, perf, ci

## Pull Request 工作流

创建 PR 时：
1. 分析完整的提交历史（不仅仅是最新的提交）
2. 使用 `git diff [base-branch]...HEAD` 查看所有变更
3. 撰写全面的 PR 摘要
4. 包含带 TODO 的测试计划
5. 如果是新分支，推送时使用 `-u` 标志

## 功能实现工作流

1. **先规划**
   - 使用 **planner** agent 创建实现计划
   - 识别依赖和风险
   - 分解为多个阶段

2. **TDD 方法**
   - 使用 **tdd-guide** agent
   - 先编写测试（红灯）
   - 实现代码使测试通过（绿灯）
   - 重构（改进）
   - 验证 80%+ 覆盖率

3. **代码审查**
   - 编写代码后立即使用 **code-reviewer** agent
   - 解决关键和高优先级问题
   - 尽可能修复中等优先级问题

4. **提交和推送**
   - 详细的提交信息
   - 遵循约定式提交格式

## 输出规则
- 只输出提交信息本身，不要添加任何签名、标记或元信息
- 不要包含 "Generated with Claude Code"、"Co-Authored-By" 等署名内容
- 不要使用 emoji 表情符号
- 保持简洁专业的风格
