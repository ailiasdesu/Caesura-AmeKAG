---
module: project
date: 2026-06-28
problem_type: architecture_pattern
component: development_workflow
severity: medium
tags: [audit, readiness, galgame, quality-assurance, team-workflow]
---

# Galgame Engine Core Readiness Audit Pattern

## Context

Before starting collaborative development on a galgame engine with real game scripts, we needed to know whether the engine could actually support a complete visual novel workflow. The engine had ~400 passing tests but had never been systematically validated against actual galgame use cases.

## Guidance

Run a **structured readiness audit** across all engine modules using these steps:

### 1. Define Functional Domains

Group validation items into meaningful galgame domains (D1-D11):
- D1: Boot & Init (engine lifecycle)
- D2: Background & Layer Display (visual rendering)
- D3: Text Rendering (FreeType, CJK, Ruby)
- D4: Audio System (BGM/Voice/SE buses)
- D5: KAG Script System (tokenizer, scheduler, 68 commands — historical; Neo-Genesis now has 72 contract commands)
- D6: Save/Load (AES-256-GCM, schema migration)
- D7: Transitions & VFX (crossfade, wipe, rule, quake)
- D8: Resource Pipeline (async loading, LRU budget)
- D9: Input & Interaction (click, keyboard, skip, backlog)
- D10: Character Display (Live2D/PNG fallback)
- D11: Integration (full demo flow)

### 2. Audit by Code Review First, Then Runtime

For each item:
1. Verify implementation via code review (grep for function signatures, check implementation files)
2. If the code path exists and logic is correct -> PASS
3. If the code path exists but has gaps -> mark severity (P0/P1/P2) and record phenomenon
4. Only run runtime verification for items that cannot be validated in code

This approach avoids wasting time re-testing known-working code paths.

### 3. Track in Real-Time with Shared Tools

- Use a shared tracking table (Feishu Base recommended for Chinese teams, or GitHub Issues)
- Each item has: ID, domain, description, status (pending/pass/fail/fixing), severity, owner, date
- Add a Kanban view grouped by status for visual progress
- Update status in real-time as items are verified

### 4. Categorize Issues by Severity

| Severity | Definition | Action |
|----------|-----------|--------|
| P0 | Blocking — crash, data loss, regression | Fix immediately |
| P1 | Should fix — feature gap affecting core experience | Fix this cycle |
| P2 | Nice to have — deferrable enhancement | Defer to next iteration |

### 5. Fix the Easy Wins First

After audit, sort issues by encoding difficulty and fix the simplest ones first. This builds momentum and reduces the open issue count quickly.

## Why This Matters

- **Prevents "it compiles but doesn't work" surprises** — a systematic audit catches gaps that unit tests miss
- **Enables parallel development** — each developer knows exactly what they own and what's already done
- **Creates reusable checklists** — the domain list (D1-D11) can be used for future engines or refactors
- **Surfaces hidden depth** — our audit found 10 issues that 400+ passing tests never caught

## Results from This Audit

- 68/76 items passed (89%)
- 3 P1 issues fixed (Ctrl skip, right-click toggle, backlog scrolling)
- 7 items deferred to P2 (layer/text fade effects, text wrapping, VFX, voice callback)
- 0 P0 blocking issues found
- Engine confirmed ready for galgame development

## Prevention

When starting collaborative development on a complex engine:
1. Run a readiness audit **before** assigning modules to developers
2. Use a shared tracking table that everyone can update
3. Audit by code review first (faster than runtime for most items)
4. Fix easy items immediately to reduce open issue count
5. Defer non-critical enhancements to later iterations with clear severity markers
