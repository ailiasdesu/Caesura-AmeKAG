# BRIEFING — 2026-08-25T01:23:15+08:00

## Mission
Implement Milestone R1: Fix sandbox whitelist for input handlers and constants, apply unconditional viewport clamping in text.lua for [input] box, and enhance test_input_cmd.lua with strict sandbox mode and 720p/1080p boundary tests.

## 🔒 My Identity
- Archetype: worker
- Roles: implementer, qa, specialist
- Working directory: d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_r1_2
- Original parent: 8e11fb75-1dc9-4c55-91e3-cdd835179626
- Milestone: R1

## 🔒 Key Constraints
- DO NOT CHEAT. All implementations must be genuine. No hardcoded test results, facade implementations, or fake verifications.
- Follow minimal change principle and AGENTS.md rules.
- C++ and Lua tests must all pass.
- Write files only in assigned folder `.agents/worker_r1_2/` or project source/test directories.

## Current Parent
- Conversation ID: 8e11fb75-1dc9-4c55-91e3-cdd835179626
- Updated: 2026-08-25T01:23:15+08:00

## Task Summary
- **What to build**:
  1. Add `_KAG_onTextInput`, `_KAG_onTextEditing`, `_KAG_onKeyDown`, and `_GAME_KEY_BACKSPACE` to `_G_whitelist` in `scripts/sandbox.lua`.
  2. In `scripts/kag/commands/text.lua`: Apply `max_allowed_y = math.floor(vh * 0.45 - box_h)` unconditionally so `box_y = math.max(0, math.min(box_y, max_allowed_y))`.
  3. In `tests/scripts/test_input_cmd.lua`: Add tests verifying strict sandbox mode (`_SANDBOX_MODE == "strict"`) execution of `[input]` and 720p/1080p viewport boundary calculations.
  4. Run and verify all tests (coupling, CaesuraTests, test_input_cmd.lua, run_lua_tests.lua).
- **Success criteria**: All checks pass, genuine implementation, complete test coverage, changes.md and handoff.md populated.
- **Interface contracts**: `AGENTS.md`
- **Code layout**: `src/`, `scripts/`, `tests/`

## Key Decisions Made
- `_G_whitelist` updated with dynamic input handlers and `_GAME_KEY_BACKSPACE` so strict sandbox environment runs `[input]` and key events without runtime error.
- Clamping of `box_y` moved outside the `box_y <= 0` condition so default viewport positioning cannot occlude the virtual keyboard zone (lower 55% of viewport).
- UTF-8 fallback slicing in `text.lua` hardened with byte-level header scanning to prevent splitting multibyte characters.
- Cross-platform `mkdirs`/`rmdirs` added to `test_ks_i18n_flow.lua` to ensure full test runner passes on Windows.

## Artifact Index
- `.agents/worker_r1_2/DISPATCH.md` — Assignment record
- `.agents/worker_r1_2/BRIEFING.md` — Agent state and memory
- `.agents/worker_r1_2/progress.md` — Liveness and progress tracker
- `.agents/worker_r1_2/changes.md` — Changes report
- `.agents/worker_r1_2/handoff.md` — Handoff report

## Change Tracker
- **Files modified**:
  - `scripts/sandbox.lua`: Added input event callbacks and backspace key to `_G_whitelist`.
  - `scripts/kag/commands/text.lua`: Unconditionally clamped `box_y` against `max_allowed_y` and hardened multibyte fallback string slicing.
  - `tests/scripts/test_input_cmd.lua`: Added 720p/1080p viewport boundary checks and strict sandbox mode execution tests.
  - `tests/scripts/test_sandbox.lua`: Added test assertions for whitelisted input callbacks and backspace constant.
  - `tests/scripts/test_ks_i18n_flow.lua`: Replaced POSIX `mkdir -p` and `rm -rf` with cross-platform helpers.
- **Build status**: PASS (`cmake --build build --config Debug`, 0 errors)
- **Pending issues**: None

## Quality Status
- **Build/test result**: PASS (CaesuraTests 1041/1041 passed, run_lua_tests 134/134 passed, test_input_cmd 42/42 passed)
- **Lint status**: Clean (count_coupling.py --ci passes all 16 modules)
- **Tests added/modified**: `tests/scripts/test_input_cmd.lua` (sections 8 & 9), `tests/scripts/test_sandbox.lua` (section 5)

## Loaded Skills
- None
