# Adversarial Challenge Report — Milestone R1: IME Virtual Keyboard & Text Input Component

## Challenge Summary

**Overall risk assessment**: LOW
**Verdict**: APPROVE

All 11 stress dimensions and boundary edge cases passed empirical verification with zero failures, zero regressions, and full adherence to `AGENTS.md` modular isolation principles.

---

## Adversarial Challenges & Stress Scenarios

### Challenge 1: Empty String Default & Excessive Backspacing on Empty Buffer
- **Assumption Challenged**: Calling backspace on an empty buffer (`""`) repeatedly or starting with an empty default could trigger underflow, negative string indexing, or nil dereferencing in `utf8_pop()`.
- **Attack Scenario**: Initialized `[input]` with `default = ""` and dispatched 50 consecutive `_G._KAG_onKeyDown(8, "backspace")` calls.
- **Empirical Result**: `utf8_pop("")` safely returned `""` on empty strings and single-byte strings; buffer remained `""` without exception or state corruption.
- **Status**: PASSED (Test 1).

### Challenge 2: Multi-byte UTF-8 Encoding (Japanese Kana/Kanji, Chinese, Emojis)
- **Assumption Challenged**: Multi-byte UTF-8 sequences (3-byte CJK ideographs, 4-byte emojis) could be broken or truncated mid-byte by byte-based operations during typing or backspacing.
- **Attack Scenario**: Appended mixed CJK and 4-byte emojis (`"初期設定・角色名🌸✨🎮"`) and iteratively popped them with backspaces.
- **Empirical Result**: Multi-byte character boundaries were cleanly preserved by `utf8_pop()` (using `utf8.offset(s, -1)` with byte-continuation loop fallback); individual emojis and kanji were removed intact without leaving malformed trailing bytes.
- **Status**: PASSED (Test 2).

### Challenge 3: Maxlen Truncation Across Multi-byte and Emoji Boundaries
- **Assumption Challenged**: `maxlen` enforcement could count raw bytes instead of code points or split a multi-byte sequence when appending text near the limit.
- **Attack Scenario**:
  - `maxlen = 5` with ASCII (`"123" + "45678"` -> `"12345"`).
  - `maxlen = 4` with Kanji (`"雨宮" + "蓮華桜花"` -> `"雨宮蓮華"`).
  - `maxlen = 3` with Emojis (`"🌸" + "🌟🔥✨🎮"` -> `"🌸🌟🔥"`).
- **Empirical Result**: `utf8_length()` and code-point iteration correctly counted codepoints and clipped exactly at codepoint boundaries without corrupting multi-byte characters. Both `maxlen` and `max_length` alias parameters were verified.
- **Status**: PASSED (Test 3).

### Challenge 4: Password Masking Integrity and Plaintext Isolation
- **Assumption Challenged**: Password masking could leak plaintext into the UI text scene or store masked asterisks (`"****"`) into the target variable instead of plaintext.
- **Attack Scenario**: Initialized `[input]` with `password = true`, entered `"桜🌸Key"`, and inspected the rendered `TextScene` draws and target scope `tf.secret`.
- **Empirical Result**: UI text display contained strictly asterisks (`"*****|"`) corresponding to the codepoint count, while the actual underlying variable received uncorrupted plaintext `"桜🌸Key"`.
- **Status**: PASSED (Test 4).

### Challenge 5: Viewport Placement & Virtual Keyboard Occlusion Prevention
- **Assumption Challenged**: High `y` parameters (e.g. `y = 800` on 720p or 1080p screens) could place the text box in the lower half of the screen where an on-screen virtual keyboard would occlude it.
- **Attack Scenario**: Supplied out-of-bounds `y = 800` on a 1080p logical viewport (`vh = 1080`).
- **Empirical Result**: Clamped to `max_allowed_y = math.floor(1080 * 0.45 - 150) = 336`, ensuring the entire bounding box (`y + height = 486 <= 0.45 * 1080`) remains in the upper 45% viewport. Default centering (`x = (1920 - 600)/2 = 660`) and upper placement (`y = 0.22 * vh = 237`) were verified.
- **Status**: PASSED (Test 5).

### Challenge 6: Button Hit-Testing, Key Interceptions & Cancel Discard
- **Assumption Challenged**: Clicking outside button regions could erroneously trigger dismissal, or clicking Cancel/pressing Escape could save modified buffer contents.
- **Attack Scenario**: Dispatched clicks inside OK button, Cancel button, and outside all buttons; dispatched Return (13) and Escape (27) key events.
- **Empirical Result**: Outside clicks were ignored (input mode remained active); OK click and Return key cleanly committed values; Cancel click and Escape key safely discarded changes without mutating target variables.
- **Status**: PASSED (Test 6).

### Challenge 7: IME Composition Pre-edit & Composition Backspacing
- **Assumption Challenged**: Intermediate IME composition text (`SDL_EVENT_TEXT_EDITING`) could contaminate the permanent buffer or fail to clear upon backspace.
- **Attack Scenario**: Dispatched `_G._KAG_onTextEditing("nihon", 0, 5)`, verified `"[nihon]"` display, triggered backspace, and finally committed `"日本"`.
- **Empirical Result**: Pre-edit text displayed in brackets without altering `buffer`; backspacing cleared the pre-edit text before touching `buffer`; final text input seamlessly replaced composition text.
- **Status**: PASSED (Test 7).

### Challenge 8: Coroutine Interruption & External Resumption Safety
- **Assumption Challenged**: An external coroutine resume (or engine reset) while `[input]` is awaiting user confirmation could leak global hooks (`_G._KAG_onTextInput`) or leave IME active.
- **Attack Scenario**: Invoked `coroutine.resume(co)` directly while `ctx._inputMode == true`.
- **Empirical Result**: `cleanup_and_finish(true)` executed upon coroutine wakeup, stopping IME, clearing `TextScene` groups, and restoring previous global hooks without error.
- **Status**: PASSED (Test 8).

### Challenge 9: Variable Scope Routing
- **Assumption Challenged**: Scopes other than `f` (such as `tf`, `sf`, or bare variables) could be dropped or misrouted.
- **Attack Scenario**: Tested `f.foo`, `tf.bar`, `sf.baz`, and bare `qux`.
- **Empirical Result**: Correctly routed to `ctx.f`, `ctx.tf`, `ctx.sf`, and default `ctx.f` respectively.
- **Status**: PASSED (Test 9).

### Challenge 10: Conditional Execution
- **Assumption Challenged**: `cond` parameter could fail to evaluate expressions or stall the coroutine on false conditions.
- **Attack Scenario**: Tested `cond = "f.flag == true"` with `f.flag = false` and `f.flag = true`.
- **Empirical Result**: False condition exited immediately without yielding or starting IME; true condition entered interactive input.
- **Status**: PASSED (Test 10).

### Challenge 11: Sequential Chained [input] Commands
- **Assumption Challenged**: Consecutive `[input]` commands in the same script could conflict on hook restoration or leave stale state.
- **Attack Scenario**: Executed prompt 1 (`f.first_name`) followed by prompt 2 (`f.last_name`) in a single coroutine.
- **Empirical Result**: Prompt 1 committed cleanly; Prompt 2 initialized fresh IME hooks and committed second variable without interference.
- **Status**: PASSED (Test 11).

---

## Stress Test Results Summary

| # | Test Case | Assertions | Status |
|---|---|---|---|
| 1 | Empty default & 50 consecutive backspaces | 6 | PASS |
| 2 | Multi-byte UTF-8 (Kanji, Kana, Chinese, 4-byte Emojis) | 2 | PASS |
| 3 | Maxlen truncation (ASCII, Kanji, Emojis, max_length alias) | 4 | PASS |
| 4 | Password masking & plaintext retention | 3 | PASS |
| 5 | Viewport placement bounds & occlusion clamping | 7 | PASS |
| 6 | OK/Cancel button hit-testing, outside click rejection, Escape discard | 7 | PASS |
| 7 | Composition editing (pre-edit) & backspacing | 3 | PASS |
| 8 | Coroutine interruption & external resume cleanup | 6 | PASS |
| 9 | Scoped variable mapping (`f`, `tf`, `sf`, bare) | 4 | PASS |
| 10 | Conditional execution (`cond` parameter) | 4 | PASS |
| 11 | Sequential chained `[input]` prompts in coroutine | 6 | PASS |
| **Total** | **Adversarial Stress Suite** | **52** | **100% PASS** |

---

## Unchallenged Areas
- Physical hardware IME touch keyboards on iOS/Android device screens (verified via SDL3 unit tests, mock probes, and platform abstractions).
