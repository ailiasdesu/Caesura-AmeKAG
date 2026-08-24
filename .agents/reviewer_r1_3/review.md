# Review and Adversarial Stress-Test Report — Milestone R1 (Iteration 2)

## Review Summary

**Verdict**: **APPROVE**
**Overall Quality**: EXCELLENT
**Integrity Status**: VERIFIED (No hardcoded facades, bypasses, or integrity violations)
**Overall Risk Assessment**: LOW

---

## 1. Quality & Correctness Review

### 1.1 `scripts/sandbox.lua` Whitelist Conformance
- **Location**: `_G_whitelist` (lines 167-169, 201) and `DEVCORE_WHITELIST` (lines 306-309).
- **Verified Additions**:
  - `_KAG_onTextInput = true`
  - `_KAG_onTextEditing = true`
  - `_KAG_onKeyDown = true`
  - `_GAME_KEY_BACKSPACE = true`
  - `start_text_input = true`, `stop_text_input = true`, `set_text_input_rect = true`, `is_text_input_active = true` in `DEVCORE_WHITELIST`.
- **Sandbox Security Assessment**:
  - `_G_mt.__newindex` correctly checks `_G_whitelist[k]`.
  - Non-whitelisted globals (e.g. `_UNAUTHORIZED_TEST_VAR_FORBIDDEN`) remain strictly blocked with runtime errors.
  - No global namespace pollution or backdoor creation detected.

### 1.2 `scripts/kag/commands/text.lua` Viewport Clamping & Layout
- **Location**: `TextCommands.input` (lines 1604-1615).
- **Verified Implementation**:
  ```lua
  local max_allowed_y = math.floor(vh * 0.45 - box_h)
  if max_allowed_y < 20 then max_allowed_y = 20 end
  box_y = math.max(0, math.min(box_y, max_allowed_y))
  ```
- **Correctness Analysis**:
  - **720p Resolution** ($vw=1280, vh=720, box\_h=180$):
    - $vh \times 0.45 = 324\text{px}$.
    - $max\_allowed\_y = 324 - 180 = 144\text{px}$.
    - Default $y$ ($vh \times 0.22 = 158\text{px}$) is unconditionally clamped: $\min(158, 144) = 144\text{px}$.
    - Input box bounding box is $y + h = 144 + 180 = 324\text{px} = 45\% \times vh$.
    - Overflowing $y=500$ is clamped to $144\text{px}$.
    - Valid low $y=60$ is preserved at $60\text{px}$.
  - **1080p Resolution** ($vw=1920, vh=1080, box\_h=180$):
    - $vh \times 0.45 = 486\text{px}$.
    - $max\_allowed\_y = 486 - 180 = 306\text{px}$.
    - Default $y$ ($vh \times 0.22 = 237\text{px}$) is preserved: $\min(237, 306) = 237\text{px}$ ($237 + 180 = 417\text{px} \le 486\text{px}$).
    - Overflowing $y=800$ is clamped to $306\text{px}$.
    - Valid low $y=100$ is preserved at $100\text{px}$.
  - Lower bound guard `max_allowed_y < 20` prevents negative values on very compact or low-resolution viewports.
- **Multibyte UTF-8 Slicing & Backspace**:
  - `utf8_length` and `utf8_pop` safely handle multibyte UTF-8 codepoints without chopping intermediate continuation bytes (`0x80`..`0xBF`).
  - `_G._KAG_onTextInput` contains both `utf8.codes` iteration and safe byte-boundary fallback.
  - Event restoration in `cleanup_and_finish` properly restores prior hooks and clears input mode.

---

## 2. Adversarial Stress-Testing & Integrity Audit

### 2.1 Adversarial Dimension 1: Integrity & Facade Check
- **Check**: Are there any hardcoded returns, fake mock test results, or bypasses?
- **Result**: **PASS**. Real logic is implemented across C++ (`IPlatformBackend`, `SDL3PlatformBackend`, `DevCoreBinding`, `Engine.cpp`) and Lua (`sandbox.lua`, `text.lua`, `schema.lua`, `backend.lua`).

### 2.2 Adversarial Dimension 2: Boundary & Extreme Inputs
- **Attack Scenario**: Low resolution or small viewport where $vh \times 0.45 < box\_h$.
- **Result**: **PASS**. Handled by `if max_allowed_y < 20 then max_allowed_y = 20 end` and `box_y = math.max(0, math.min(box_y, max_allowed_y))`.
- **Attack Scenario**: Rapid typing exceeding `maxlen` or composition event flooding.
- **Result**: **PASS**. `maxlen` clamping works at both single-byte and multibyte UTF-8 boundaries.
- **Attack Scenario**: Invoking `TextCommands.input` in strict sandbox mode.
- **Result**: **PASS**. Verified by `test_input_cmd.lua` section 9 under `_SANDBOX_MODE == "strict"`.

---

## 3. Verified Claims & Test Results

| Test Suite / Tool | Command | Scope | Result | Details |
|---|---|---|---|---|
| Architecture Coupling | `python scripts/count_coupling.py --ci` | 16 Modules | **PASS** | 0 violations across all 16 modules |
| C++ Full Test Suite | `.\CaesuraTests.exe` (in `build/tests/Debug`) | Core Engine | **PASS** | 1041 / 1041 passed, 0 failed, 385,095 assertions passed |
| Standalone Input Suite | `lua tests/scripts/test_input_cmd.lua` | IME & Input | **PASS** | 42 / 42 assertions passed |
| Standalone Sandbox Suite | `lua -e "package.path='scripts/?.lua;'..package.path" tests/scripts/test_sandbox.lua` | Sandbox Sec | **PASS** | 15 / 15 assertions passed |
| Full Lua Test Suite | `lua tests/scripts/run_lua_tests.lua` | All Lua Subsystems | **PASS** | 134 / 134 test suites passed, 0 failed |

---

## 4. Findings & Observations

### Minor Observation (Informational Only)
- **What**: Running `tests/scripts/test_sandbox.lua` standalone directly without `run_lua_tests.lua` requires `package.path` to include `scripts/?.lua`.
- **Impact**: Zero impact on engine or test runner (`run_lua_tests.lua` sets `package.path` on line 8).
- **Suggestion**: For consistency with `test_input_cmd.lua`, `test_sandbox.lua` could optionally include `package.path = "scripts/?.lua;" .. package.path` at the top in future cleanup.

---

## 5. Final Recommendation
All acceptance criteria for Milestone R1 and Worker 2 remediation tasks have been met with complete integrity, rigorous testing, and zero architectural regressions. **APPROVE**.
