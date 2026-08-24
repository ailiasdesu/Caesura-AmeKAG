# Milestone R1 Implementation Changes: IME Virtual Keyboard & Text Input Component (Track IME)

## Overview
Milestone R1 delivers end-to-end Virtual Keyboard and IME Text Input support for Caesura (AmeKAG) across Desktop, Mobile (Android/iOS), Headless/Testing, and the KAG Scripting layer.

---

## 1. Files Modified

### C++ Platform & Engine Subsystem
1. **`src/platform/api/IPlatformBackend.h`**:
   - Added 4 pure virtual methods:
     - `virtual bool startTextInput() = 0;`
     - `virtual bool stopTextInput() = 0;`
     - `virtual bool setTextInputRect(int x, int y, int w, int h, int cursor = 0) = 0;`
     - `virtual bool isTextInputActive() const = 0;`
   - Opaque primitive arguments keep interface completely decoupled from third-party types (conforming to `AGENTS.md`).

2. **`src/platform/SDL3PlatformBackend.h` & `src/platform/SDL3PlatformBackend.cpp`**:
   - Implemented `startTextInput()`, `stopTextInput()`, `setTextInputRect()`, and `isTextInputActive()` via SDL3 APIs:
     - `SDL_StartTextInput(m_window)`
     - `SDL_StopTextInput(m_window)`
     - `SDL_SetTextInputArea(m_window, &rect, cursor)`
     - `SDL_TextInputActive(m_window)`
   - Safeguarded with `if (!m_window) return false;` for pre-initialization safety.

3. **`src/platform/NullPlatformBackend.h` & `src/platform/NullPlatformBackend.cpp`**:
   - Implemented headless state tracking with internal tracking fields: `m_textInputActive`, `m_textInputX`, `m_textInputY`, `m_textInputW`, `m_textInputH`, `m_textInputCursor`.
   - Updated `shutdown()` to reset `m_textInputActive = false`.

4. **`tests/cpp/EntryLifecycleBackends.h`**:
   - Implemented stub methods on test probe `PlatformBackend` (`startTextInput()`, `stopTextInput()`, `setTextInputRect()`, `isTextInputActive()`).

5. **`src/entry/Engine.cpp`**:
   - In `Engine::processEvents()`, added event dispatch for:
     - `SDL_EVENT_TEXT_INPUT`: Invokes global Lua function `_KAG_onTextInput(text)`.
     - `SDL_EVENT_TEXT_EDITING`: Invokes global Lua function `_KAG_onTextEditing(text, start, length)`.
   - In `SDL_EVENT_KEY_DOWN` / `SDL_EVENT_KEY_UP`: Added `SDLK_BACKSPACE` state tracking (`_GAME_KEY_BACKSPACE`) and `_KAG_onKeyDown` event dispatch for backspace, return, and escape keys.

6. **`src/input/InputRouter.cpp`**:
   - Confirmed `InputRouter` preserves non-advancing behavior in KAG focus mode while properly delivering UTF-8 text events to GAME focus callbacks.

7. **`src/script/bindings/DevCoreBinding.cpp`**:
   - Implemented `lua_DevCore_start_text_input`, `lua_DevCore_stop_text_input`, `lua_DevCore_set_text_input_rect`, and `lua_DevCore_is_text_input_active`.
   - Registered them in `devcore_functions[]` table.

---

### Lua Framework & KAG Runtime
8. **`scripts/backend.lua`**:
   - Added proxy methods to `Backend`:
     - `Backend.start_text_input()`
     - `Backend.stop_text_input()`
     - `Backend.set_text_input_rect(x, y, w, h, cursor)`
     - `Backend.is_text_input_active()`

9. **`scripts/backend_factory.lua`**:
   - Added command routing to `backend.platform` dispatch for `"start_text_input"`, `"stop_text_input"`, `"set_text_input_rect"`, and `"is_text_input_active"`.

10. **`scripts/sandbox.lua`**:
    - Whitelisted `start_text_input`, `stop_text_input`, `set_text_input_rect`, and `is_text_input_active` in `DEVCORE_WHITELIST`.

11. **`scripts/kag/schema.lua`**:
    - Defined declarative contracts for `[input]` and `[edit]` (KAG3 alias), supporting:
      - `name` (required target variable, e.g. `f.player_name` or `tf.password`)
      - `prompt`, `default`, `maxlen` / `max_length`, `x`, `y`, `width`, `height`, `font_size`, `color`, `bg_color`, `password`, `cond`, `btn_ok`, `btn_cancel`.

12. **`scripts/kag/commands/text.lua`**:
    - Implemented `TextCommands.input(ctx, params)` and `TextCommands.edit = TextCommands.input`:
      - Evaluates optional condition `cond`.
      - Computes UI bounding box with virtual keyboard occlusion prevention (`y <= 0.45 * height` upper viewport bound).
      - Calls `backend.set_text_input_rect` and `backend.start_text_input()`.
      - Renders prompt, password-masked/plain text with composition indicator, and OK/Cancel buttons in `TextScene` under `"text_input"` group.
      - Intercepts `_G._KAG_onTextInput`, `_G._KAG_onTextEditing`, `_G._KAG_onKeyDown`, and `_G._KAG_onClick`.
      - Yields execution via `coroutine.yield()` until committed or canceled.
      - On finish: saves variable to scoped target (`ctx.f` or specified scope), stops text input, cleans up scene elements, and restores previous hooks.

---

### Tests Added & Updated
13. **`tests/cpp/test_platform.cpp`**:
    - Added tests for `NullPlatformBackend text input lifecycle`.
    - Added tests for `SDL3PlatformBackend text input pre-init safety`.
    - Added tests for `IPlatformBackend text input polymorphism`.

14. **`tests/cpp/test_input.cpp`**:
    - Added tests for `SDL_EVENT_TEXT_INPUT is non-advancing in KAG focus`.
    - Added tests for `SDL_EVENT_TEXT_EDITING is non-advancing in KAG focus`.
    - Added tests for `text events route to GAME callback in GAME focus with UTF-8 preservation`.

15. **`tests/scripts/test_input_cmd.lua`**:
    - Comprehensive unit test suite covering:
      - Schema definition and coercion for `input` and `edit`.
      - Platform backend text input mock tracking.
      - Adaptive upper viewport placement (`y <= 0.45 * height`).
      - Interactive typing and backspace deletion.
      - Enter key commit and scoped variable writing.
      - Password masking in UI (`****`).
      - Mouse click OK / Cancel hit testing.

16. **`tests/scripts/run_lua_tests.lua`**:
    - Registered `test_input_cmd` in the automated test runner.

---

## 2. Verification Results
- **Coupling CI**: `python scripts/count_coupling.py --ci` -> PASS (all 16 modules within limits).
- **C++ Build**: `cmake --build build --config Debug` -> 0 errors.
- **C++ Doctests**: `CaesuraTests.exe` -> 1034 / 1034 passed (0 failed, 0 skipped, 315,959 assertions passed).
- **Lua Unit Test**: `test_input_cmd.lua` -> 100% checks passed.
