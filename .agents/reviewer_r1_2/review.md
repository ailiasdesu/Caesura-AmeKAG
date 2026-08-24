# Quality & Adversarial Review Report: Milestone R1 (Track IME)

## Review Summary

**Verdict**: REQUEST_CHANGES
**Overall Risk Assessment**: MEDIUM-HIGH (Critical runtime defect under strict sandbox mode; core feature implementation is solid)

---

## Findings

### [Critical] Finding 1: Strict Sandbox Crash on `_KAG_onTextInput` / `_KAG_onTextEditing` / `_KAG_onKeyDown` / `_GAME_KEY_BACKSPACE`
- **What**: When the Lua environment is locked in strict mode (`_SANDBOX_MODE == "strict"`, which is the default in release builds), calling `TextCommands.input(ctx, params)` throws a fatal error: `Sandbox: cannot create global '_KAG_onTextInput'`.
- **Where**:
  - `scripts/sandbox.lua:135-208` (`_G_whitelist`)
  - `scripts/kag/commands/text.lua:1689-1780`
  - `src/entry/Engine.cpp:1223`
- **Why**: `scripts/sandbox.lua` protects `_G` from unauthorized global creation via a metatable `__newindex` hook. The `_G_whitelist` table includes `_KAG_onClick`, `_KAG_onKey`, and `_KAG_onScroll`, but fails to include the newly introduced event hooks:
  - `_KAG_onTextInput`
  - `_KAG_onTextEditing`
  - `_KAG_onKeyDown`
  - `_GAME_KEY_BACKSPACE`
  When `text.lua` registers global handlers or `Engine.cpp` pushes `_GAME_KEY_BACKSPACE`, the assignment fails in strict sandbox mode.
- **Reproduction**:
  ```lua
  local text = require("kag.commands.text")
  require("sandbox") -- locks down _G in strict mode
  local ctx = { f = {}, sf = {}, tf = {}, mp = {}, text_state = { draws = {} } }
  local co = coroutine.create(function() text.input(ctx, { name = 'f.name' }) end)
  local ok, err = coroutine.resume(co)
  -- Result: ok = false, err = "scripts/kag/commands/text.lua:1750: Sandbox: cannot create global '_KAG_onTextInput'"
  ```
- **Suggested Fix**:
  Add the missing global identifiers to `_G_whitelist` in `scripts/sandbox.lua`:
  ```lua
  _KAG_onTextInput   = true,
  _KAG_onTextEditing = true,
  _KAG_onKeyDown     = true,
  _GAME_KEY_BACKSPACE = true,
  ```

---

### [Major] Finding 2: Default `y` Viewport Occlusion Edge Case
- **What**: When `y` is not specified (`y <= 0`), `box_y` is initialized to `math.floor(vh * 0.22)`. For standard galgame 180px-tall boxes at 720p resolution, `158 + 180 = 338px`, which is `46.9% * vh` (slightly exceeding the `45% * vh = 324px` limit).
- **Where**: `scripts/kag/commands/text.lua:1604-1614`
- **Why**: The adaptive clamping logic (`max_allowed_y = math.floor(vh * 0.45 - box_h)`) is placed in the `else` branch of `if box_y <= 0`, meaning default `y` calculations bypass the clamp check.
- **Suggested Fix**: Apply the `max_allowed_y` upper bound clamp unconditionally after setting default `box_y`:
  ```lua
  local box_y = tonumber(params.y) or 0
  if box_y <= 0 then
      box_y = math.floor(vh * 0.22)
  end
  local max_allowed_y = math.floor(vh * 0.45 - box_h)
  if max_allowed_y < 20 then max_allowed_y = 20 end
  if box_y > max_allowed_y then
      box_y = max_allowed_y
  end
  ```

---

### [Minor] Finding 3: Multibyte Truncation Fallback in Max Length Truncation
- **What**: In `_G._KAG_onTextInput` in `text.lua`, if `utf8` library is unavailable or disabled, the fallback branch performs byte-level string slicing: `buffer = buffer .. text:sub(1, allowed)`.
- **Where**: `scripts/kag/commands/text.lua:1743`
- **Why**: If a multibyte character (e.g. 3-byte CJK or 4-byte Emoji) is partially sliced by byte offset, it can produce invalid UTF-8 sequences.
- **Suggested Fix**: Use safe UTF-8 slicing or ensure UTF-8 boundary alignment when `utf8.codes` is not used.

---

## Verified Claims

- **C++ Interface & Decoupling**: `IPlatformBackend.h` defines pure virtual methods (`startTextInput`, `stopTextInput`, `setTextInputRect`, `isTextInputActive`) with primitive types only, satisfying `AGENTS.md` interface rules -> **PASS**
- **C++ Implementations**: `SDL3PlatformBackend`, `NullPlatformBackend`, and `EntryLifecycleBackends.h` implement all 4 virtual methods with null safety guards -> **PASS**
- **InputRouter Non-Advancing Behavior**: In `InputFocus::KAG` mode, `SDL_EVENT_TEXT_INPUT` and `SDL_EVENT_TEXT_EDITING` do not trigger `m_kagClickPending` -> **PASS**
- **DevCore & Backend Delegation**: `DevCoreBinding.cpp`, `scripts/backend.lua`, and `scripts/backend_factory.lua` properly route all 4 IME methods -> **PASS**
- **KAG Schema Contract**: `[input]` and `[edit]` registered in `scripts/kag/schema.lua` with typed coercions -> **PASS**
- **UI & Variable Assignment**: Scoped assignments (`f.*`, `tf.*`, `sf.*`, `mp.*`) and password masking (`****`) function as specified in headless tests -> **PASS**
- **Architecture Coupling**: `python scripts/count_coupling.py --ci` passes all 16 module limits -> **PASS**
- **C++ Doctest Suite**: 1034 / 1034 test cases pass -> **PASS**
- **Lua Unit Test**: `test_input_cmd.lua` passes all 23 assertions -> **PASS**

---

## Adversarial Stress Test Results

| Scenario | Expected Behavior | Actual Behavior | Result |
|---|---|---|---|
| Strict Sandbox Mode Activation | `TextCommands.input` runs without global error | Crashes: `Sandbox: cannot create global '_KAG_onTextInput'` | **FAIL (Finding 1)** |
| Backspace with Multibyte CJK Characters | Deletes 1 UTF-8 codepoint without corruption | `utf8_pop` pops full codepoint via reverse scan | **PASS** |
| Password Masking in UI | UI displays `****`, stores plaintext | `display_buf` masked, `targetTbl[key]` plaintext | **PASS** |
| Virtual Keyboard Height Clamping (explicit `y=800`) | Clamps `y` so `box_y + box_h <= 0.45 * vh` | Clamped to `144` (for 720p, 180h) | **PASS** |
| Variable Scoping (`tf.pwd`, `f.name`, `sf.slot`) | Writes to appropriate context table | Properly routes to `ctx.tf`, `ctx.f`, `ctx.sf` | **PASS** |
| Mouse Hit Testing on OK / Cancel | Clicks outside buttons ignored; OK saves; Cancel discards | Correct boundary checks with padding | **PASS** |
| Re-entrancy / Hook Cleanup | Restores `oldTextInput`, `oldKeyDown`, `oldClick` on exit | Full cleanup in `cleanup_and_finish` | **PASS** |

---

## Coverage Gaps

- Sandbox regression tests (`test_sandbox.lua`) should be updated to assert that `_KAG_onTextInput`, `_KAG_onTextEditing`, `_KAG_onKeyDown`, and `_GAME_KEY_BACKSPACE` can be set without sandbox violations.

---

## Unverified Items
- Physical touchscreen keyboard opening on physical Android / iOS devices (requires physical device deployment). Headless and mock pipelines are verified.
