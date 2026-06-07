---
title: "中文 Markdown 说明书编码损毁与 PowerShell 批量编辑踩坑"
date: 2026-06-07
category: docs/solutions/documentation-gaps
module: 说明书 (docs/Caesura_功能实现规格说明书_整合版.md)
problem_type: documentation_gap
component: documentation
severity: high
applies_when:
  - "编辑含中文的 UTF-8 Markdown 文件时"
  - "用 PowerShell 对含中文的文件做批量正则替换"
  - "Git 仓库中 reopen 已损毁编码的历史文件"
tags: [encoding, utf8, bom, gbk, ffdd, powershell, markdown, chinese, section-renumbering]
---

# 中文 Markdown 说明书编码损毁与 PowerShell 批量编辑踩坑

## Context

`docs/Caesura_功能实现规格说明书_整合版.md` 是引擎的权威实现规格文档（~795 行，全中文）。在多次迭代中（添加 Beta Job System 章节、修复乱码、代码↔说明书对齐），反复遇到编码问题和脚本编辑问题。

## 问题 1: 编码损毁 — FFFD 8100+ 乱码

### 症状
- 文件声明为 UTF-8 BOM，但实际字节是 GBK 编码的中文
- `Get-Content -Encoding UTF8` 读取后全文出现 8100+ 个 `�`（U+FFFD）替换字符
- 所有中文内容不可读，Git 历史中永久丢失原始字节
- 前两次提交（`f9701f8` 和更早版本）均已损毁

### 根因
第一次写入说明书时，编辑工具或脚本以系统默认编码（GBK/CP936，Windows 中文版）写入字节，但文件头保留了 UTF-8 BOM 标记。后续所有读→改→写操作都基于已损毁的版本，错误累积。

### 解决
1. 从 Git 历史中找到唯一干净的版本（commit `b61d0d1`），该版本中文字节正确
2. 用 `[System.IO.File]::ReadAllLines` + `UTF8Encoding($true)`（带 BOM）读取干净版本
3. 在此基础上重新应用所有 Beta 内容修改（Part 8 + 决策 72-78）
4. 修复后 FFFD 计数从 8100+ 降为 1（仅 BOM 自身）

### 预防
- **写入前验证**: 修改中文文件后用 FFFD 计数检查：`([int]$_ -eq 0xFFFD).Count`
- **提交前验证**: 将 FFFD 检查纳入 pre-commit hook
- **编码写入规范**: 始终用 `[System.Text.UTF8Encoding]::new($true)` 写回，确保 BOM 和字节一致

---

## 问题 2: Section 编号级联替换错误

### 症状
将 Part 10 的 `[9.1]~[9.9]` 全部 +2 改为 `[9.3]~[9.11]` 时，最终结果变成 `[9.5][9.6][9.7][9.10][9.11]...`，数字跳跃且重复。

### 根因
正序遍历替换时，`[9.1]→[9.3]` 创造了一个新的 `[9.3]`，然后轮到 `[9.3]→[9.5]` 时把这个新创建的也替换了，形成级联（cascading）。

### 解决
两步法：
1. 逆序（9.9→9.1）用临时占位符替换：`[9.9]→##TMP11##, [9.8]→##TMP10##, ...`
2. 再统一替换占位符：`##TMP3##→[9.3], ##TMP4##→[9.4], ...`

### 预防
- 涉及数字递变的批量替换，优先用占位符两步法
- 或使用 `-replace` 时从大到小逆序遍历

---

## 问题 3: 数组插入导致行索引漂移

### 症状
在 `[10.2.32]` 决策段落后插入 Beta 迁移计划行后，原文出现了重复行。

### 根因
`$lines = @($lines[0..($i+2)]; $beta; $lines[($i+3)..end])` 拼接时，第一段包含了已修改的 `$lines[$i+2]`，但原 `$lines[$i+3]`（即下一段 `### [10.2.33]`）被正确移位。问题出在修改 `$lines[$i+1]` 和 `$lines[$i+2]` 时未考虑原始行结构（有空白行），导致实际修改了错误的行。

### 解决
- 先用 `git show HEAD:file` 确认原始行结构
- 插入后逐行验证 `$i-2` 到 `$i+6` 范围
- 发现重复行后按索引精确删除

### 预防
- 任何涉及行插入/删除的操作后，立即对变更区域 ±5 行做逐行验证
- 用 `git diff` 确认净变更量符合预期（本次为 20 insertions / 18 deletions）

---

## 最终产出

- FFFD 计数: **1**（干净）
- Git diff: **20 insertions, 18 deletions**（精确）
- 8 项说明书对齐修改全部完成，已推送至 GitHub (`565920f`)