# Review & Adversarial Analysis Report: Milestone R1

**Milestone**: R1 — IME Virtual Keyboard & Text Input Component (Track IME)  
**Reviewer**: Reviewer 1 (reviewer, critic)  
**Verdict**: **APPROVE**  
**Date**: 2026-08-25T01:12:00Z  

---

## 1. Executive Summary & Verdict

The work product delivered for Milestone R1 (Track IME) implements complete, robust, and cleanly abstracted IME virtual keyboard support and interactive text input components across the C++ platform backend, engine event loop, input router, Lua bindings, and KAG Neo-Genesis scripting runtime.

All acceptance criteria are satisfied:
1. `IPlatformBackend` pure virtual methods implemented without violating `AGENTS.md` module boundaries or leaking third-party types.
2. `SDL3PlatformBackend` and `NullPlatformBackend` implement text input management with full lifecycle and pre-initialization safety guards.
3. `Engine.cpp` properly routes `SDL_EVENT_TEXT_INPUT`, `SDL_EVENT_TEXT_EDITING`, and navigation keys (`SDLK_BACKSPACE`, `SDLK_RETURN`, `SDLK_ESCAPE`) to Lua globals.
4. `InputRouter.cpp` guarantees non-advancing behavior in `InputFocus::KAG` mode, preventing text composition from accidentally advancing dialogue scenes.
5. `DevCoreBinding.cpp`, `backend.lua`, `backend_factory.lua`, and `sandbox.lua` correctly expose and whitelist text input APIs for sandboxed scripts.
6. `[input]` and `[edit]` KAG commands provide a fully functional interactive UI component with adaptive upper-viewport positioning (`y <= 0.45 * height`) to prevent mobile virtual keyboard occlusion, UTF-8 safety, password masking, mouse hit-testing, and variable scoping.
7. Architecture coupling limits pass (`python scripts/count_coupling.py --ci` -> PASS across all 16 modules).
8. C++ doctest suite (`CaesuraTests.exe`) passes with **1034 / 1034 test cases (315,959 assertions passed, 0 failed, 0 skipped)**.
9. Lua test suite (`test_input_cmd.lua`) passes with **100% checks passed**.

---

## 2. Integrity Violation Inspection

An exhaustive integrity check was conducted:
- **No Hardcoded Test Results**: Verification assertions test dynamic state transitions and live event dispatching rather than hardcoded returns.
- **No Dummy / Facade Implementations**: `SDL3PlatformBackend` directly issues `SDL_StartTextInput`, `SDL_StopTextInput`, `SDL_SetTextInputArea`, and `SDL_TextInputActive`; `NullPlatformBackend` maintains stateful tracking fields for headless environments; `TextCommands.input` implements full interactive coroutine yielding and event lifecycle management.
- **No Bypassed Requirements**: The entire stack (C++ interface -> platform backends -> engine loop -> input router -> Lua DevCore -> backend facade -> sandbox -> KAG schema -> KAG interactive UI component -> unit tests) is fully built from scratch.
- **Genuine Independent Verification**: Build commands and test executions were independently triggered and verified clean in the target environment.

---

## 3. Detailed Quality Review

### 3.1 Correctness & Implementation Quality
- **Pure Virtual Interface (`src/platform/api/IPlatformBackend.h`)**:
  - `startTextInput()`, `stopTextInput()`, `setTextInputRect(int, int, int, int, int = 0)`, and `isTextInputActive()` are pure virtual (`= 0`) methods without member variables or third-party dependencies.
- **SDL3 Backend (`src/platform/SDL3PlatformBackend.*`)**:
  - Correctly maps to SDL3 text input functions.
  - Includes `if (!m_window) return false;` safety checks preventing crashes when accessed prior to window creation or after shutdown.
- **Null Backend (`src/platform/NullPlatformBackend.*`)**:
  - Implements complete state tracking (`m_textInputActive`, `m_textInputX`, `m_textInputY`, `m_textInputW`, `m_textInputH`, `m_textInputCursor`).
  - Gated on `m_initialized` and properly resets active status on `shutdown()`.
- **Engine Event Dispatch (`src/entry/Engine.cpp`)**:
  - Safely checks for null `L` and `!isLuaExecutionPaused()` before invoking `_KAG_onTextInput`, `_KAG_onTextEditing`, or `_KAG_onKeyDown`.
  - Uses `lua_pcall` with error logging / stack clearing, preventing Lua runtime panics from aborting the native frame loop.
  - Updates `_GAME_KEY_BACKSPACE` state variable synchronously on `SDL_EVENT_KEY_DOWN` and `SDL_EVENT_KEY_UP`.
- **Input Routing Boundary (`src/input/InputRouter.cpp`)**:
  - In `InputFocus::KAG` mode, `SDL_EVENT_TEXT_INPUT` and `SDL_EVENT_TEXT_EDITING` do not set `m_kagClickPending = true`, ensuring text composition does not advance story dialogues.
  - In `InputFocus::GAME` mode, raw UTF-8 text events are passed through unmodified to game callbacks.
- **Lua Bindings & Sandbox (`src/script/bindings/DevCoreBinding.cpp`, `scripts/sandbox.lua`)**:
  - Safely extracts `IPlatformBackend*` from Lua registry via `getPlatform(L)`.
  - Whitelists platform text input routines in `DEVCORE_WHITELIST`.
- **KAG Neo-Genesis Commands (`scripts/kag/schema.lua`, `scripts/kag/commands/text.lua`)**:
  - Declares typed schema contracts for `[input]` and `[edit]`.
  - Prevents mobile keyboard occlusion by bounding `y <= 0.45 * height`.
  - Supports condition evaluation (`cond`), password masking (`password=true`), backspace/delete, multi-byte UTF-8, mouse OK/Cancel click hit-testing, and scoped variable storage (`f.*`, `tf.*`, `sf.*`).

### 3.2 Architectural Conformance (`AGENTS.md`)
- **Module Boundaries (Rule 1 & Rule 2)**: Only `api/IPlatformBackend.h` is exposed outside `src/platform/`. No implementation headers leaked across modules.
- **BackendRegistry Access (Rule 3 & Rule 4)**: C++ bindings resolve platform backends via `BackendRegistry` registry userdata; concrete objects are constructed solely in the composition root.
- **Coupling Limits (Rule 9)**: `python scripts/count_coupling.py --ci` passed with 0 violations across all 16 modules.

---

## 4. Adversarial Review & Attack Surface Stress-Testing

| Challenge / Attack Angle | Potential Vulnerability | Implemented Defense / Verification Result | Status |
|---|---|---|---|
| **Pre-init / Post-shutdown Safety** | Calling text input APIs before `init()` or after `shutdown()` causes null pointer dereference. | `SDL3PlatformBackend` checks `if (!m_window) return false;`. `NullPlatformBackend` checks `m_initialized`. Verified by `test_platform.cpp`. | **PASS** |
| **IME Composition vs Story Advance** | Text input / editing events arm `m_kagClickPending` and prematurely advance the visual novel dialogue. | `InputRouter.cpp` filters `SDL_EVENT_TEXT_INPUT` and `SDL_EVENT_TEXT_EDITING` out of the KAG advancing event list. Verified by `test_input.cpp`. | **PASS** |
| **Multi-byte UTF-8 & CJK Corruption** | Multi-byte strings (e.g. Japanese kanji/kana, Chinese characters, 4-byte emojis) get truncated midway through code points during backspacing or length calculation. | `text.lua` implements `utf8_length` and `utf8_pop` with byte prefix checking (`b < 0x80 or b >= 0xC0`) and uses Lua 5.4 `utf8` library where available. Verified by `test_input_cmd.lua` and `test_input.cpp`. | **PASS** |
| **Virtual Keyboard Occlusion** | Soft keyboard pops up on mobile touchscreen (Android/iOS) and covers the text box UI at the bottom of the screen. | `text.lua` enforces upper-viewport clamping `box_y = math.min(box_y, math.floor(vh * 0.45 - box_h))`, keeping the component within the top 45% of the viewport. | **PASS** |
| **Hook Leaking & Coroutine Abort** | If the input coroutine is cancelled or completed, global hooks `_G._KAG_onTextInput` remain active, polluting subsequent scenes. | `cleanup_and_finish` restores `oldTextInput`, `oldTextEditing`, `oldKeyDown`, `oldClick`, and destroys scene group `"text_input"`. | **PASS** |
| **Password Data Exposure** | Password strings displayed in plain text in UI scenes or backlog history. | Password mode renders `string.rep("*", utf8_length(buffer))` in the UI while preserving the actual characters for storage in target variables. | **PASS** |

---

## 5. Verified Claims Matrix

| Claim | Verification Method | Result |
|---|---|---|
| Zero build errors on MSVC Debug | `cmake --build build --config Debug` | **PASS (0 errors, 0 warnings)** |
| Architectural coupling within thresholds | `python scripts/count_coupling.py --ci` | **PASS (16/16 modules compliant)** |
| C++ doctest suite passing | `.\build\tests\Debug\CaesuraTests.exe` | **PASS (1034 passed, 0 failed, 0 skipped, 315959 assertions)** |
| Platform unit tests covering IME | `test_platform.cpp` doctest cases | **PASS** |
| Input router unit tests covering IME | `test_input.cpp` doctest cases | **PASS** |
| KAG interactive input unit tests passing | `.\build\lua\Debug\lua.exe tests/scripts/test_input_cmd.lua` | **PASS (22/22 checks passed)** |
| KAG command regression tests passing | `test_kag_commands.lua`, `test_schema.lua` | **PASS (all checks passed)** |

---

## 6. Coverage Gaps & Caveats

- **Caveat**: Font rendering for non-ASCII characters requires loaded TTF font assets with matching Unicode glyph ranges.
- **Coverage**: No unexplored code paths or unaddressed risks identified in Milestone R1 scope.

---

## 7. Final Verdict

**APPROVE** — The Milestone R1 implementation is complete, well-architected, fully tested, and ready for integration.
