# Changes Report — Milestone R1 (Sandbox Whitelist & Viewport Clamping Fix)

## Overview
Worker 2 addressed the defects identified during the review and audit for Milestone R1:
1. Whitelisted input event callback globals (`_KAG_onTextInput`, `_KAG_onTextEditing`, `_KAG_onKeyDown`) and key constant (`_GAME_KEY_BACKSPACE`) in `scripts/sandbox.lua`.
2. Fixed virtual keyboard viewport occlusion by applying `max_allowed_y = math.floor(vh * 0.45 - box_h)` unconditionally in `scripts/kag/commands/text.lua` to ensure `box_y` never places the input box inside the bottom 55% virtual keyboard area across all resolutions (720p, 1080p, etc.).
3. Improved multibyte UTF-8 string slicing fallback in `scripts/kag/commands/text.lua` to prevent character corruption when `utf8.codes` is not present.
4. Added test coverage in `tests/scripts/test_input_cmd.lua` and `tests/scripts/test_sandbox.lua` for strict sandbox mode (`_SANDBOX_MODE == "strict"`) and 720p/1080p viewport boundary calculations.
5. Fixed POSIX shell command portability in `tests/scripts/test_ks_i18n_flow.lua` (`mkdirs`/`rmdirs`) so test runner runs cleanly on Windows.

---

## Detailed Modifications

### 1. `scripts/sandbox.lua`
- **Location**: `_G_whitelist` table (lines 164-169 and 195-200)
- **Change**: Added `_KAG_onTextInput = true`, `_KAG_onTextEditing = true`, `_KAG_onKeyDown = true`, and `_GAME_KEY_BACKSPACE = true`.
- **Rationale**: When strict sandbox mode is active, `_G_mt.__newindex` restricts global creation. Whitelisting these symbols allows `TextCommands.input` and `Engine.cpp` to register and dispatch IME events without throwing global security violations.

### 2. `scripts/kag/commands/text.lua`
- **Location**: `TextCommands.input` viewport positioning (lines 1604-1615)
- **Change**: Moved `max_allowed_y = math.floor(vh * 0.45 - box_h)` calculation outside the `if box_y <= 0` branch and applied `box_y = math.max(0, math.min(box_y, max_allowed_y))` unconditionally.
- **Location**: UTF-8 input truncation fallback (lines 1735-1748)
- **Change**: Added safe byte-boundary scanning in the fallback branch so multibyte codepoints are not sliced mid-sequence.
- **Rationale**: At 720p with default `y`, `vh * 0.22` (158px) + 180px box height resulted in 338px (46.9% of viewport), exceeding the 45% upper bound. Clamping unconditionally ensures `box_y` is set to 144px (144 + 180 = 324px = 45% of 720p).

### 3. `tests/scripts/test_input_cmd.lua`
- **Location**: Sections 8 & 9 (lines 130-220)
- **Change**:
  - Section 8: Added 720p and 1080p viewport clamping assertions (testing default `y`, explicit overflow `y`, and explicit valid low `y`).
  - Section 9: Added strict sandbox mode verification (`require("sandbox")` with `_SANDBOX_MODE == "strict"`), testing that `_GAME_KEY_BACKSPACE` is writable, unauthorized globals are blocked, and `TextCommands.input` completes interactive coroutine execution, typing, backspacing, and cleanup without error.

### 4. `tests/scripts/test_sandbox.lua`
- **Location**: Section 5 (lines 56-68)
- **Change**: Added explicit assertions validating that `_KAG_onTextInput`, `_KAG_onTextEditing`, `_KAG_onKeyDown`, and `_GAME_KEY_BACKSPACE` can be set in `_G` without sandbox errors, while non-whitelisted globals remain blocked.

### 5. `tests/scripts/test_ks_i18n_flow.lua`
- **Location**: Setup, sections 4, 5, 6, and cleanup (lines 20-40, 125, 185, 210, 290)
- **Change**: Replaced POSIX `rm -rf` and `mkdir -p` with cross-platform `mkdirs` and `rmdirs` helpers to ensure clean directory isolation across OS environments.

---

## Verification Results

| Verification Suite | Target / Command | Result |
|---|---|---|
| Architecture Coupling | `python scripts/count_coupling.py --ci` | **PASS** (0 violations across all 16 modules) |
| C++ Compilation | `cmake --build build --config Debug` | **PASS** (0 errors, 0 warnings) |
| C++ Unit Tests | `./build/tests/Debug/CaesuraTests.exe` | **PASS** (1041 / 1041 passed, 0 failed, 385,095 assertions passed) |
| Standalone Input Test | `lua tests/scripts/test_input_cmd.lua` | **PASS** (42 / 42 assertions passed) |
| Standalone Sandbox Test | `lua tests/scripts/test_sandbox.lua` | **PASS** (15 / 15 assertions passed) |
| Full Lua Test Suite | `lua tests/scripts/run_lua_tests.lua` | **PASS** (134 / 134 test suites passed, 0 failed) |
