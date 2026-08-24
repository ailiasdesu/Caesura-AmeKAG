# Forensic Audit Report — Milestone R1 Iteration 2

**Work Product**: Milestone R1 Iteration 2 (IME Virtual Keyboard & Text Input Component, Sandbox Whitelist & Viewport Clamping Fix)
**Profile**: General Project (Development Mode)
**Verdict**: CLEAN

---

### Executive Summary
The forensic integrity audit evaluated the implementation of Milestone R1 and the specific fixes applied in Iteration 2 across:
1. `scripts/sandbox.lua` (whitelisting `_KAG_onTextInput`, `_KAG_onTextEditing`, `_KAG_onKeyDown`, and `_GAME_KEY_BACKSPACE`).
2. `scripts/kag/commands/text.lua` (unconditional upper viewport boundary clamping `math.floor(vh * 0.45 - box_h)` and multibyte UTF-8 byte boundary processing).
3. `tests/scripts/test_input_cmd.lua` and `tests/scripts/test_sandbox.lua` (strict sandbox mode verification and 720p/1080p viewport boundary tests).
4. `tests/scripts/test_ks_i18n_flow.lua` (cross-platform directory management).
5. All C++ and Lua core platform and input layers (`IPlatformBackend`, `SDL3PlatformBackend`, `NullPlatformBackend`, `Engine.cpp`, `InputRouter`, `DevCoreBinding`).

All implementations were verified to be genuine, mathematically sound, free of facades, and fully compliant with AGENTS.md architectural boundaries and coupling constraints.

---

### Phase Results

| Check / Phase | Result | Details |
|---|---|---|
| **Phase 1: Source Analysis — Hardcoded Output Detection** | **PASS** | No hardcoded test responses or facade return values. Viewport clamping and UTF-8 manipulations are genuine algorithms. |
| **Phase 1: Source Analysis — Facade Detection** | **PASS** | `SDL3PlatformBackend` integrates SDL3 text input APIs (`SDL_StartTextInput`, `SDL_StopTextInput`, `SDL_SetTextInputArea`, `SDL_TextInputActive`). `NullPlatformBackend` tracks real lifecycle state. |
| **Phase 1: Source Analysis — Pre-populated Artifact Detection** | **PASS** | No pre-populated test results or fabricated attestation logs. |
| **Phase 2: Architectural Compliance (AGENTS.md)** | **PASS** | `python scripts/count_coupling.py --ci` passed with 0 violations across all 16 modules. |
| **Phase 2: C++ Full Compilation & doctest Suite** | **PASS** | `cmake --build build --config Debug` zero errors; `CaesuraTests.exe` 1041 / 1041 passed (385,095 assertions passed). |
| **Phase 2: Standalone & Full Lua Test Suites** | **PASS** | `test_input_cmd.lua` (42/42 passed), `test_sandbox.lua` (15/15 passed), `run_lua_tests.lua` (134/134 suites passed). |
| **Phase 2: Forensic Stress Testing** | **PASS** | Stress tests verified multibyte Japanese/Chinese/Emoji backspacing and truncation, low-res (320vh) clamping, and scope resolution (`f`, `tf`, `sf`). |

---

### Raw Verification Evidence

#### 1. Architecture Coupling Check (`python scripts/count_coupling.py --ci`)
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

#### 2. C++ Unit Tests (`./build/tests/Debug/CaesuraTests.exe`)
```
===============================================================================
[doctest] test cases:   1041 |   1041 passed | 0 failed | 0 skipped
[doctest] assertions: 385095 | 385095 passed | 0 failed |
[doctest] Status: SUCCESS!
```

#### 3. Standalone Unit Tests (`test_input_cmd.lua`)
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
PASS 720p default y clamped to 144
PASS 720p default y upper viewport bound satisfied (y + h <= 324)
PASS 720p explicit y=500 clamped to 144
PASS 720p explicit y=500 upper viewport bound satisfied
PASS 720p explicit valid y=60 preserved
PASS 1080p default y is 237
PASS 1080p default y upper viewport bound satisfied (y + h <= 486)
PASS 1080p explicit y=800 clamped to 306
PASS 1080p explicit y=800 upper viewport bound satisfied
PASS 1080p explicit valid y=100 preserved
[Sandbox] Strict mode: render operation whitelist active
PASS sandbox module loaded
PASS sandbox allows _GAME_KEY_BACKSPACE
PASS sandbox blocks non-whitelisted globals
PASS strict sandbox coroutine resumes without error
PASS strict sandbox installed _KAG_onTextInput
PASS strict sandbox installed _KAG_onTextEditing
PASS strict sandbox installed _KAG_onKeyDown
PASS strict sandbox input completed
PASS strict sandbox input mode cleared
PASS strict sandbox IME stopped
[test_input_cmd] Done. All checks passed.
```

#### 4. Full Lua Test Suite Runner (`run_lua_tests.lua`)
```
Results: 134 passed, 0 failed, 134 total
```

---

### Conclusion
Milestone R1 Iteration 2 is certified **CLEAN** with zero integrity violations and zero regressions.
