# Forensic Integrity Audit Report: Milestone R1

**Work Product**: Milestone R1 (IME Virtual Keyboard & Text Input Component - Track IME)  
**Profile**: General Project  
**Integrity Mode**: Development (from `ORIGINAL_REQUEST.md`)  
**Auditor**: Forensic Auditor (`.agents/auditor_r1_1`)  
**Date**: 2026-08-25  
**Verdict**: **CLEAN** (Integrity Verified; 1 Implementation Defect Identified in `scripts/sandbox.lua`)

---

## Executive Summary

A comprehensive forensic integrity audit was conducted across all changes in Milestone R1, covering C++ platform abstractions, SDL3 implementations, engine event dispatching, Lua C bindings, KAG commands, and test suites.

1. **No Prohibited Patterns Detected**:
   - Zero hardcoded test results.
   - Zero dummy/facade implementations in production code.
   - Zero fabricated verification logs or outputs.
   - Real and genuine SDL3 API integration (`SDL_StartTextInput`, `SDL_StopTextInput`, `SDL_SetTextInputArea`, `SDL_TextInputActive`).
   - Genuine KAG interactive coroutine logic and viewport clamping (`y <= 0.45 * height`).

2. **Architecture Compliance (`AGENTS.md`)**:
   - `IPlatformBackend.h` contains pure virtual methods (`startTextInput()`, `stopTextInput()`, `setTextInputRect()`, `isTextInputActive()`) without leaking third-party types (such as `SDL_Window*` or `SDL_Rect`).
   - `BackendRegistry` remains the sole access point across all 16 modules.
   - `python scripts/count_coupling.py --ci` passed with 0 violations across all 16 modules.

3. **Empirical Verification**:
   - C++ Full Solution Build: **PASS** (Zero errors).
   - C++ Doctest Suite (`CaesuraTests.exe`): **1034 / 1034 passed** (0 failed, 0 skipped, 315,959 assertions passed).
   - Standalone Lua Unit Test (`test_input_cmd.lua`): **PASS** (22 / 22 assertions passed).
   - Full Lua Test Suite (`run_lua_tests.lua`): **133 / 134 passed**, identifying 1 defect in `scripts/sandbox.lua` where new globals `_KAG_onTextInput`, `_KAG_onTextEditing`, `_KAG_onKeyDown`, and `_GAME_KEY_BACKSPACE` were omitted from `_G_whitelist`.

---

## Detailed Forensic Checks

### Phase 1: Source Code & Static Integrity Analysis

| Check | Target | Result | Evidence / Details |
|---|---|---|---|
| Hardcoded Test Results | `tests/cpp/test_platform.cpp`, `test_input.cpp`, `test_input_cmd.lua` | **PASS** | Test assertions check genuine dynamic state mutations (e.g. `ime_active`, `ctx.f.player_name`, `ime_rect.y`). |
| Facade / Dummy Code | `src/platform/SDL3PlatformBackend.cpp` | **PASS** | Real calls to `SDL_StartTextInput(m_window)`, `SDL_StopTextInput(m_window)`, `SDL_SetTextInputArea(m_window, &rect, cursor)`, `SDL_TextInputActive(m_window)`. |
| Headless Mock Fidelity | `src/platform/NullPlatformBackend.cpp` | **PASS** | State variables `m_textInputActive`, `m_textInputX`, `m_textInputY`, `m_textInputW`, `m_textInputH`, `m_textInputCursor` properly tracked and cleared on `shutdown()`. |
| Interface Leakage | `src/platform/api/IPlatformBackend.h` | **PASS** | Interface is pure virtual, namespace `Caesura`, decoupled from third-party headers/types. |
| Composition Root & DI | `src/entry/Engine.cpp`, `src/script/bindings/DevCoreBinding.cpp` | **PASS** | DevCore resolves `IPlatformBackend` via Lua registry pointer populated from `BackendRegistry`. |

### Phase 2: Architecture & Module Boundary Checks

```
Cross-module #include counts:
-------------------------------------------------------
  archive      ->  2/4  modules (  2 total)  debug:1, resource:1
  audio        ->  2/4  modules (  3 total)  debug:1, di:2
  debug        ->  0/4  modules (  0 total)  
  di           -> 13/14 modules ( 23 total)  archive:1, audio:2, debug:1, input:1, job:1, live2d:1, minigame:1, platform:4, render:6, resource:2, script:1, steam:1, storage:1
  entry        -> 14/14 modules ( 78 total)  archive:5, audio:4, debug:5, di:8, input:2, job:2, live2d:3, minigame:3, platform:9, render:20, resource:8, script:3, steam:3, storage:3
  input        ->  0/4  modules (  0 total)  
  job          ->  1/4  modules (  1 total)  di:1
  live2d       ->  3/4  modules ( 10 total)  debug:6, di:1, render:3
  minigame     ->  4/4  modules (  4 total)  debug:1, di:1, input:1, render:1
  platform     ->  0/4  modules (  0 total)  
  render       ->  4/4  modules ( 32 total)  audio:1, debug:14, di:16, job:1
  resource     ->  3/4  modules (  5 total)  debug:3, di:1, job:1
  rpc          ->  2/4  modules (  3 total)  archive:2, debug:1
  script       -> 11/14 modules ( 39 total)  audio:2, debug:5, di:12, input:1, job:1, minigame:2, platform:3, render:9, resource:2, steam:1, storage:1
  steam        ->  0/4  modules (  0 total)  
  storage      ->  4/4  modules (  4 total)  archive:1, debug:1, di:1, steam:1
-------------------------------------------------------
PASS: All modules within thresholds and API boundaries.
```

### Phase 3: Runtime & Test Validation

1. **C++ Doctests (`CaesuraTests.exe`)**:
   ```
   [doctest] test cases:   1034 |   1034 passed | 0 failed | 0 skipped
   [doctest] assertions: 315959 | 315959 passed | 0 failed |
   [doctest] Status: SUCCESS!
   ```
2. **Standalone Lua Test (`test_input_cmd.lua`)**:
   ```
   PASS input schema defined
   PASS edit schema defined
   PASS schema coerces maxlen to number
   PASS schema retains name
   PASS input mode active
   PASS waiting_input flag set
   PASS IME started
   PASS IME rect assigned
   PASS adaptive upper viewport positioning (y <= 0.45 * height)
   PASS text input handler installed
   PASS key down handler installed
   PASS input mode cleared after enter
   PASS waiting_input cleared
   PASS IME stopped after commit
   PASS variable assigned in f scope
   PASS UI elements removed
   PASS password masked in UI display
   PASS plaintext stored in tf scope
   PASS input mode active in ctx3
   PASS click OK saves result
   PASS input mode cleared after click OK
   PASS click Cancel does not save result
   PASS input mode cleared after click Cancel
   [test_input_cmd] Done. All checks passed.
   ```

---

## Defect Finding: `scripts/sandbox.lua` Global Whitelist Gap

### Observation
When executing the full sequential test suite `run_lua_tests.lua`, `test_input_cmd.lua` fails at line 59 with:
```
FAIL text input handler installed
FAIL key down handler installed
[FAIL] test_input_cmd: tests/scripts/test_input_cmd.lua:59: attempt to call a nil value (field '_KAG_onTextInput')
```

### Root Cause Analysis
In `scripts/sandbox.lua`, while `DEVCORE_WHITELIST` (lines 302-305) whitelisted `start_text_input`, `stop_text_input`, `set_text_input_rect`, and `is_text_input_active`, the global whitelist table `_G_whitelist` (lines 161-208) was NOT updated with:
- `_KAG_onTextInput = true,`
- `_KAG_onTextEditing = true,`
- `_KAG_onKeyDown = true,`
- `_GAME_KEY_BACKSPACE = true,`

When `sandbox.lua` is loaded in strict mode, `_G_mt.__newindex` prohibits assigning non-whitelisted globals. When `TextCommands.input` sets `_G._KAG_onTextInput = function(...)`, an unhandled error occurs within the coroutine before yielding, leaving `_KAG_onTextInput` unassigned.

### Remediation Required (for next iteration / fix task)
In `scripts/sandbox.lua` under section `7. GLOBAL ENVIRONMENT METATABLE` in `_G_whitelist`:
```lua
    _KAG_onTextInput    = true,
    _KAG_onTextEditing  = true,
    _KAG_onKeyDown      = true,
    _GAME_KEY_BACKSPACE = true,
```

---

## Final Verdict

**Verdict**: **CLEAN**  
All R1 deliverables are genuinely implemented without deception, facades, or shortcut patterns. The code complies with all `AGENTS.md` and `PROJECT.md` standards.
