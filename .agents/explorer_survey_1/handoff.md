# Handoff Report: Milestone R1 (Track IME Survey)

## 1. Observation
1. **Platform Backend Interface**:
   - In `src/platform/api/IPlatformBackend.h:14-56`, `IPlatformBackend` defines lifecycle (`init`, `shutdown`), `pollEvent`, `getMouseState`, `getTicksMs`, `getNativeWindowHandle`, `createGLContext`, `postFrame`, `getWindowWidth`, `getWindowHeight`, `setFullscreen`, `resizeWindow`, and `getBackendName`. There are zero text input or IME virtual keyboard methods defined.
   - In `external/SDL3/SDL3-3.2.0/include/SDL3/SDL_keyboard.h:354-550`, SDL3 defines `SDL_StartTextInput(SDL_Window*)`, `SDL_StopTextInput(SDL_Window*)`, `SDL_SetTextInputArea(SDL_Window*, const SDL_Rect*, int cursor)`, and `SDL_TextInputActive(SDL_Window*)`.
   - In `src/platform/SDL3PlatformBackend.h:12-50` and `NullPlatformBackend.h:8-29`, neither platform backend currently implements any text input methods.

2. **Input Routing & Event Dispatching**:
   - In `external/SDL3/SDL3-3.2.0/include/SDL3/SDL_events.h:375-425`, SDL3 defines `SDL_TextInputEvent` (with `const char* text`) and `SDL_TextEditingEvent` (with `const char* text`, `Sint32 start`, `Sint32 length`).
   - In `src/input/InputRouter.cpp:43-69`, `InputRouter::processEvent` only routes `SDL_EVENT_MOUSE_BUTTON_DOWN` and `SDL_EVENT_KEY_DOWN` to `m_kagCallbacks` in `InputFocus::KAG` mode, and routes all events to `m_gameCallbacks` in `InputFocus::GAME` mode.
   - In `src/entry/Engine.cpp:979-1278`, `Engine::processEvents` handles window resizing, mouse state, and key globals, but has no handlers for `SDL_EVENT_TEXT_INPUT` or `SDL_EVENT_TEXT_EDITING`.

3. **Scripting & Lua Bindings**:
   - In `src/script/bindings/DevCoreBinding.cpp:197-208`, `DevCore` registers functions for focus, logging, quitting, and resolution, but no text input APIs.
   - In `scripts/backend.lua:1-540`, `Backend` acts as a facade over `_CAESURA_BACKEND` and `DevCore`, but lacks `start_text_input`, `stop_text_input`, `set_text_input_rect`, and `is_text_input_active`.
   - In `scripts/sandbox.lua:295-375`, `DEVCORE_WHITELIST` filters callable `DevCore` APIs in sandboxed contexts.

4. **KAG Engine & Choice / Text Scene Components**:
   - In `scripts/kag/commands/text.lua:1248-1410`, interactive choice buttons (`[button]`, `[endbutton]`) set `ctx.waiting_input = true`, install a custom `_G._KAG_onClick` hook, block via `coroutine.yield()`, and resume upon user action.
   - In `scripts/kag/text_scene.lua:106-225`, text and UI elements are grouped via named groups (e.g. `"choices"`), allowing dynamic addition and cleanup (`remove_group`).
   - In `scripts/viewport.lua:1-50`, logical viewport resolution is retrieved via `viewport.wh()` (default 1920x1080).
   - In `scripts/kag/schema.lua:1-50`, KAG Neo-Genesis commands are defined with typed contracts, constraints, and metadata via `Schema.define(cmd, spec)`.

5. **Existing Test Harness**:
   - In `tests/cpp/test_platform.cpp:1-372`, doctest cases cover `SDL3PlatformBackend`, `NullPlatformBackend`, `IDisplayService`, and `ILifecycleService`.
   - In `tests/cpp/test_input.cpp:1-1113`, doctest cases verify `InputRouter` focus switching, KAG filtering of non-advancing events, and UTF-8 passthrough in `SDL_EVENT_TEXT_INPUT`.
   - In `tests/scripts/run_lua_tests.lua:30-184` and `tests/scripts/test_choice.lua:1-259`, unit tests verify interactive coroutine yielding, variable assignment, and event hook restoration.

---

## 2. Logic Chain
1. **Platform Layer (Observation 1)**:
   - Since `IPlatformBackend` lacks text input methods, adding `startTextInput()`, `stopTextInput()`, `setTextInputRect(x, y, w, h, cursor)`, and `isTextInputActive()` as pure virtual methods (`= 0`) in `src/platform/api/IPlatformBackend.h` without exposing third-party types adheres strictly to `AGENTS.md` rules.
   - Implementing these in `SDL3PlatformBackend` using `SDL_StartTextInput`, `SDL_StopTextInput`, `SDL_SetTextInputArea`, and `SDL_TextInputActive` provides native IME and virtual keyboard control across desktop and mobile.
   - Implementing corresponding state stubs in `NullPlatformBackend` ensures headless and CI test execution without requiring a display or GPU.

2. **Event Routing Layer (Observation 2)**:
   - In `Engine::processEvents()`, hooking `SDL_EVENT_TEXT_INPUT` to call `_KAG_onTextInput(text)` and `SDL_EVENT_TEXT_EDITING` to call `_KAG_onTextEditing(text, start, length)` delivers IME events to Lua cleanly without mutating `InputRouter`'s story-advance contract.
   - In `InputRouter.cpp`, ensuring text events are treated as non-advancing under `InputFocus::KAG` guarantees IME typing does not accidentally trigger page advances.

3. **Lua Binding Layer (Observation 3)**:
   - Exposing `start_text_input`, `stop_text_input`, `set_text_input_rect`, and `is_text_input_active` in `DevCoreBinding.cpp` allows Lua scripts to manage the platform virtual keyboard lifecycle.
   - Routing these through `scripts/backend.lua` and updating `DEVCORE_WHITELIST` in `scripts/sandbox.lua` guarantees sandboxed game scripts can invoke them safely.

4. **KAG UI Component & Viewport Offset (Observation 4)**:
   - Registering `Schema.define("input", ...)` in `scripts/kag/schema.lua` brings `[input]` into the KAG Neo-Genesis standard.
   - In `scripts/kag/commands/text.lua` (or a dedicated input handler), following the `endbutton` coroutine pattern (`ctx.waiting_input = true`, `coroutine.yield()`) allows asynchronous user typing and confirmation.
   - Placing the text box in the upper portion of the viewport (`y <= 0.45 * height`) and registering the input bounding area with `Backend.set_text_input_rect` ensures the virtual keyboard (which occupies the bottom ~40-50% on mobile) never occludes the input field.
   - Writing the committed text into `ctx[scope][key]` (e.g. `f.player_name`) satisfies the user data capture requirement.

5. **Testing & Validation (Observation 5)**:
   - Adding test cases in `tests/cpp/test_platform.cpp` (pre-init safety, null backend state) and `tests/cpp/test_input.cpp` (UTF-8 preservation, text event routing) guarantees C++ stability.
   - Adding `tests/scripts/test_input_cmd.lua` into `tests/scripts/run_lua_tests.lua` provides 100% headless mock validation of schema coercion, typing, backspacing, password masking, viewport offsets, and variable scoping.

---

## 3. Caveats
- **Physical Keyboard vs. Virtual Keyboard**: On desktop platforms without touch screens, `startTextInput` activates native IME candidate windows but does not pop up a visual touch keyboard. On mobile platforms (Android/iOS), SDL3 automatically raises the on-screen keyboard when text input is active.
- **Key Repeat / IME Composition**: During IME composition, raw key events may be consumed by the OS IME before reaching SDL; the component must rely on `_KAG_onTextEditing` and `_KAG_onTextInput` for composition rather than raw key codes.
- No other unexamined caveats.

---

## 4. Conclusion
The implementation architecture for Milestone R1 is fully analyzed, modular, and conformant to all engine boundaries. All 5 areas have concrete, isolated implementation paths:
1. Pure virtual methods on `IPlatformBackend` implemented by `SDL3PlatformBackend` and `NullPlatformBackend`.
2. Clean event dispatch in `Engine.cpp` (`_KAG_onTextInput`, `_KAG_onTextEditing`).
3. Platform bridge in `DevCoreBinding.cpp`, `backend.lua`, and `sandbox.lua`.
4. Declarative `[input]` command in `schema.lua` + text box UI in `text.lua` with adaptive upper viewport placement.
5. Full C++ doctest and standalone Lua test suites.

Detailed technical report is written at:
`d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_1\report.md`

---

## 5. Verification Method
1. **Architecture & Coupling Check**:
   ```powershell
   python scripts/count_coupling.py --ci
   ```
   *Expected*: Passes with 0 violations across all 16 modules.
2. **C++ Unit Tests**:
   ```powershell
   cmake --build build --config Debug
   ./build/tests/Debug/CaesuraTests.exe
   ```
   *Expected*: 100% doctest cases passed, 0 failed, 0 skipped.
3. **Lua Full Unit Test Suite**:
   ```powershell
   lua tests/scripts/run_lua_tests.lua
   ```
   *Expected*: All tests pass including `test_input_cmd`.
