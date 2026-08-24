# Handoff Report: Reviewer 2 (Milestone R1 — Track IME)

## 1. Observation

1. **Strict Sandbox Mode Rejection**:
   - In `scripts/sandbox.lua:135-208`, `_G_whitelist` defines allowed global variables for write protection.
   - It includes `_KAG_onClick`, `_KAG_onKey`, `_KAG_onScroll`, but does **not** include:
     - `_KAG_onTextInput`
     - `_KAG_onTextEditing`
     - `_KAG_onKeyDown`
     - `_GAME_KEY_BACKSPACE`
   - Direct execution under strict sandbox mode:
     ```powershell
     .\external\lua\lua.exe -e "package.path = 'scripts/?.lua;scripts/kag/?.lua;' .. package.path; local text = require('kag.commands.text'); require('sandbox'); local ctx = { f = {}, sf = {}, tf = {}, mp = {}, text_state = { draws = {} } }; local co = coroutine.create(function() text.input(ctx, { name = 'f.name' }) end); local ok, err = coroutine.resume(co); print(ok, err)"
     ```
     Verbatim Output:
     `false   scripts/kag\commands\text.lua:1750: Sandbox: cannot create global '_KAG_onTextInput'`

2. **Viewport Occlusion Default Y Placement**:
   - In `scripts/kag/commands/text.lua:1604-1614`, when `params.y` is not supplied, `box_y = math.floor(vh * 0.22)`.
   - For a default 180px box at 720p resolution, `158 + 180 = 338px` (`46.9% * vh`), slightly exceeding the `45% * vh = 324px` limit.

3. **C++ Platform, Input, and Lua Binding Implementations**:
   - `IPlatformBackend.h` defines 4 pure virtual methods with 0 data members and 0 third-party headers.
   - `SDL3PlatformBackend`, `NullPlatformBackend`, and `EntryLifecycleBackends.h` implement all methods with pre-init null guards.
   - `InputRouter.cpp` preserves non-advancing behavior for `SDL_EVENT_TEXT_INPUT` and `SDL_EVENT_TEXT_EDITING` under `InputFocus::KAG`.
   - `DevCoreBinding.cpp`, `backend.lua`, and `backend_factory.lua` correctly expose and proxy all methods.
   - `[input]` and `[edit]` schema registered in `scripts/kag/schema.lua`.
   - `test_input_cmd.lua` passes all 23 assertions.
   - `python scripts/count_coupling.py --ci` passes with 0 violations across all 16 modules.
   - `cmake --build build --config Debug` compiles with 0 errors.

---

## 2. Logic Chain

1. `scripts/sandbox.lua` enforces strict protection over `_G` in release mode. Any assignment to an unwhitelisted key triggers an error.
2. `scripts/kag/commands/text.lua` lines 1689-1780 assign global callbacks `_G._KAG_onTextInput`, `_G._KAG_onTextEditing`, and `_G._KAG_onKeyDown` during `[input]` execution.
3. Because these names are not in `_G_whitelist`, invoking `[input]` in release/strict builds fails immediately with a fatal sandbox error.
4. Adding `_KAG_onTextInput`, `_KAG_onTextEditing`, `_KAG_onKeyDown`, and `_GAME_KEY_BACKSPACE` to `_G_whitelist` in `scripts/sandbox.lua` will resolve this crash completely.
5. In addition, unconditionally applying `max_allowed_y = math.floor(vh * 0.45 - box_h)` in `scripts/kag/commands/text.lua` guarantees strict compliance with the mobile occlusion requirement under all default and custom dimension parameters.

---

## 3. Caveats

- In dev mode (`_SANDBOX_MODE = "dev"`), `_G` write protection is inactive, which is why unit tests running in dev mode passed without triggering the sandbox error.
- No other caveats.

---

## 4. Conclusion

- **Verdict**: **REQUEST_CHANGES**
- **Action Required**:
  1. Add `_KAG_onTextInput`, `_KAG_onTextEditing`, `_KAG_onKeyDown`, and `_GAME_KEY_BACKSPACE` to `_G_whitelist` in `scripts/sandbox.lua`.
  2. Ensure default `y` in `scripts/kag/commands/text.lua` is clamped by `max_allowed_y`.
  3. Add a sandbox strict mode test in `tests/scripts/test_input_cmd.lua` to prevent future regressions.

---

## 5. Verification Method

To independently reproduce and verify the fix:

1. **Reproduction of Sandbox Crash**:
   ```powershell
   .\external\lua\lua.exe -e "package.path = 'scripts/?.lua;scripts/kag/?.lua;' .. package.path; local text = require('kag.commands.text'); require('sandbox'); local ctx = { f = {}, sf = {}, tf = {}, mp = {}, text_state = { draws = {} } }; local co = coroutine.create(function() text.input(ctx, { name = 'f.name' }) end); local ok, err = coroutine.resume(co); assert(ok, err)"
   ```

2. **Unit Test Suite**:
   ```powershell
   .\external\lua\lua.exe tests/scripts/test_input_cmd.lua
   ```

3. **Architecture Coupling**:
   ```powershell
   python scripts/count_coupling.py --ci
   ```
