-- test_input_stress.lua — Adversarial Stress Test Suite for KAG [input] / [edit] command
-- Covers UTF-8 multi-byte chars, emojis, maxlen clipping, backspace boundary conditions,
-- password masking, viewport bounds, button hit-testing, composition pre-edit, and coroutine resumption.

local results = {}
local total_assertions = 0

local function check(name, cond, detail)
    total_assertions = total_assertions + 1
    if cond then
        print(string.format("  [PASS] %s", name))
        results[#results + 1] = true
    else
        print(string.format("  [FAIL] %s - %s", name, tostring(detail or "assertion failed")))
        results[#results + 1] = false
    end
end

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local Schema = require("kag.schema")
local TextCommands = require("kag.commands.text")
local TextScene = require("kag.text_scene")
local backend = require("backend")
local viewport = require("viewport")

-- Track backend mock calls
local ime_active = false
local ime_rect = nil
local original_start = backend.start_text_input
local original_stop = backend.stop_text_input
local original_set_rect = backend.set_text_input_rect

backend.start_text_input = function() ime_active = true return true end
backend.stop_text_input = function() ime_active = false return true end
backend.set_text_input_rect = function(x, y, w, h, c) ime_rect = { x = x, y = y, w = w, h = h, c = c } return true end

local function make_ctx()
    return {
        f = {}, sf = {}, tf = {}, mp = {},
        text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
        textCursorX = 32, textCursorY = 580,
        backlog = {}, layers = {},
    }
end

print("===============================================================================")
print("RUNNING ADVERSARIAL STRESS TESTS FOR KAG [INPUT] COMMAND")
print("===============================================================================")

-- -----------------------------------------------------------------------------
-- 1. Empty Default & Excessive Backspaces on Empty Buffer
-- -----------------------------------------------------------------------------
print("\n--- TEST 1: Empty Default & Excessive Backspaces ---")
do
    local ctx = make_ctx()
    local co = coroutine.create(function()
        TextCommands.input(ctx, { name = "f.empty_test", default = "", maxlen = 16 })
    end)
    coroutine.resume(co)
    check("input mode started with empty default", ctx._inputMode == true)
    check("IME active on empty default", ime_active == true)

    -- 50 consecutive backspaces on empty buffer
    for i = 1, 50 do
        _G._KAG_onKeyDown(8, "backspace")
    end
    check("input mode still active after 50 empty backspaces", ctx._inputMode == true)

    -- Enter commit
    _G._KAG_onKeyDown(13, "return")
    check("input committed after empty backspaces", ctx._inputMode == false)
    check("empty string assigned to target variable", ctx.f.empty_test == "")
    check("IME stopped", ime_active == false)
end

-- -----------------------------------------------------------------------------
-- 2. Multi-byte UTF-8: Japanese, Chinese, and Emojis
-- -----------------------------------------------------------------------------
print("\n--- TEST 2: Multi-byte UTF-8 Handling (JP / ZH / Emojis) ---")
do
    local ctx = make_ctx()
    local co = coroutine.create(function()
        TextCommands.input(ctx, { name = "f.utf8_test", default = "初", maxlen = 32 })
    end)
    coroutine.resume(co)

    -- Type Japanese Kana and Kanji
    _G._KAG_onTextInput("期設定") -- "初期設定" (4 kanji)
    -- Type Chinese
    _G._KAG_onTextInput("・角色名")
    -- Type Emojis (4-byte sequences)
    _G._KAG_onTextInput("🌸✨🎮")

    -- Check display in TextScene
    local state = TextScene.get_state(ctx)
    local found_text = false
    for _, d in ipairs(state.draws) do
        if d.text and d.text:find("初期設定・角色名🌸✨🎮") then
            found_text = true
            break
        end
    end
    check("rendered UI includes multi-byte UTF-8 string intact", found_text)

    -- Pop 3 emojis with backspace
    _G._KAG_onKeyDown(8, "backspace") -- pop 🎮
    _G._KAG_onKeyDown(8, "backspace") -- pop ✨
    _G._KAG_onKeyDown(8, "backspace") -- pop 🌸

    -- Pop 5 chinese/symbol chars
    _G._KAG_onKeyDown(8, "backspace") -- pop 名
    _G._KAG_onKeyDown(8, "backspace") -- pop 色
    _G._KAG_onKeyDown(8, "backspace") -- pop 角
    _G._KAG_onKeyDown(8, "backspace") -- pop ・
    _G._KAG_onKeyDown(8, "backspace") -- pop 定

    _G._KAG_onKeyDown(13, "return")
    check("correctly popped multi-byte chars leaving 初期設", ctx.f.utf8_test == "初期設", ctx.f.utf8_test)
end

-- -----------------------------------------------------------------------------
-- 3. Maxlen Truncation & Boundary Clipping
-- -----------------------------------------------------------------------------
print("\n--- TEST 3: Maxlen Truncation & Boundary Clipping ---")
do
    -- Case 3a: ASCII maxlen
    local ctx_a = make_ctx()
    local co_a = coroutine.create(function()
        TextCommands.input(ctx_a, { name = "f.max_a", default = "123", maxlen = 5 })
    end)
    coroutine.resume(co_a)
    _G._KAG_onTextInput("45678")
    _G._KAG_onKeyDown(13, "return")
    check("ASCII clipped strictly at maxlen=5", ctx_a.f.max_a == "12345", ctx_a.f.max_a)

    -- Case 3b: Multi-byte Kanji maxlen
    local ctx_b = make_ctx()
    local co_b = coroutine.create(function()
        TextCommands.input(ctx_b, { name = "f.max_b", default = "雨宮", maxlen = 4 })
    end)
    coroutine.resume(co_b)
    _G._KAG_onTextInput("蓮華桜花")
    _G._KAG_onKeyDown(13, "return")
    check("Multi-byte Kanji clipped at 4 codepoints (雨宮蓮華)", ctx_b.f.max_b == "雨宮蓮華", ctx_b.f.max_b)

    -- Case 3c: Emoji maxlen
    local ctx_c = make_ctx()
    local co_c = coroutine.create(function()
        TextCommands.input(ctx_c, { name = "f.max_c", default = "🌸", maxlen = 3 })
    end)
    coroutine.resume(co_c)
    _G._KAG_onTextInput("🌟🔥✨🎮")
    _G._KAG_onKeyDown(13, "return")
    check("Emoji clipped at 3 codepoints (🌸🌟🔥)", ctx_c.f.max_c == "🌸🌟🔥", ctx_c.f.max_c)

    -- Case 3d: max_length alias parameter
    local ctx_d = make_ctx()
    local co_d = coroutine.create(function()
        TextCommands.input(ctx_d, { name = "f.max_d", default = "", max_length = 3 })
    end)
    coroutine.resume(co_d)
    _G._KAG_onTextInput("ABCDEFG")
    _G._KAG_onKeyDown(13, "return")
    check("max_length alias parameter honored (ABC)", ctx_d.f.max_d == "ABC", ctx_d.f.max_d)
end

-- -----------------------------------------------------------------------------
-- 4. Password Masking
-- -----------------------------------------------------------------------------
print("\n--- TEST 4: Password Masking ---")
do
    local ctx = make_ctx()
    local co = coroutine.create(function()
        TextCommands.input(ctx, { name = "tf.secret", default = "桜🌸", password = true, maxlen = 10 })
    end)
    coroutine.resume(co)

    -- Check that UI shows exactly 2 asterisks for 2 codepoints
    local state = TextScene.get_state(ctx)
    local mask_matched = false
    for _, d in ipairs(state.draws) do
        if d.text and d.text:match("^%*%*|") then
            mask_matched = true
            break
        end
    end
    check("password masked as 2 asterisks for 2 UTF-8 characters", mask_matched)

    -- Append 3 more characters
    _G._KAG_onTextInput("Key")
    state = TextScene.get_state(ctx)
    mask_matched = false
    for _, d in ipairs(state.draws) do
        if d.text and d.text:match("^%*%*%*%*%*|") then
            mask_matched = true
            break
        end
    end
    check("password masked as 5 asterisks for 5 characters", mask_matched)

    -- Commit
    _G._KAG_onKeyDown(13, "return")
    check("underlying plaintext preserved in tf.secret", ctx.tf.secret == "桜🌸Key", ctx.tf.secret)
end

-- -----------------------------------------------------------------------------
-- 5. Viewport Placement & Occlusion Prevention Bounds Check
-- -----------------------------------------------------------------------------
print("\n--- TEST 5: Viewport Placement & Occlusion Bounds ---")
do
    local vw, vh = viewport.wh()
    
    -- Lower-half y placement (y=800) should be clamped to upper viewport (y <= 0.45 * vh - box_h)
    local ctx = make_ctx()
    local box_w, box_h = 500, 150
    local co = coroutine.create(function()
        TextCommands.input(ctx, { name = "f.pos_check", x = 100, y = 800, width = box_w, height = box_h })
    end)
    coroutine.resume(co)

    local max_expected_y = math.floor(vh * 0.45 - box_h)
    check("box y clamped to <= 0.45 * vh - height", ime_rect.y <= max_expected_y, string.format("ime_rect.y=%d, max=%d", ime_rect.y, max_expected_y))
    check("box y + height does not occlude lower 55% screen", (ime_rect.y + ime_rect.h) <= math.floor(vh * 0.45) + 1)
    check("box x set correctly", ime_rect.x == 100)
    check("box w set correctly", ime_rect.w == 500)
    check("box h set correctly", ime_rect.h == 150)
    _G._KAG_onKeyDown(13, "return")

    -- Automatic centering when x=0 and default y
    local co2 = coroutine.create(function()
        TextCommands.input(ctx, { name = "f.center_check", width = 600, height = 180 })
    end)
    coroutine.resume(co2)
    local expected_center_x = math.floor((vw - 600) / 2)
    local expected_default_y = math.floor(vh * 0.22)
    check("box x auto-centered", ime_rect.x == expected_center_x, string.format("x=%d, expected=%d", ime_rect.x, expected_center_x))
    check("box y default in upper viewport (0.22 * vh)", ime_rect.y == expected_default_y, string.format("y=%d, expected=%d", ime_rect.y, expected_default_y))
    _G._KAG_onKeyDown(13, "return")
end

-- -----------------------------------------------------------------------------
-- 6. Cancel / OK Button Hits & Key Interception
-- -----------------------------------------------------------------------------
print("\n--- TEST 6: Buttons Hit-testing & Escape / Cancel ---")
do
    -- Escape Key Cancel
    local ctx_esc = make_ctx()
    local co_esc = coroutine.create(function()
        TextCommands.input(ctx_esc, { name = "f.esc_var", default = "Original" })
    end)
    coroutine.resume(co_esc)
    _G._KAG_onTextInput("_Mod")
    _G._KAG_onKeyDown(27, "escape")
    check("Escape key clears input mode", ctx_esc._inputMode == false)
    check("Escape key does not save modified value", ctx_esc.f.esc_var == nil)

    -- Mouse Hit OK
    local ctx_ok = make_ctx()
    local co_ok = coroutine.create(function()
        TextCommands.input(ctx_ok, { name = "f.ok_btn", default = "HitOK", btn_ok = "Confirm" })
    end)
    coroutine.resume(co_ok)
    -- Click inside OK button (btn_ok is at box_x + box_w - 120, box_y + box_h - 44, w=100, h=36)
    _G._GAME_MOUSE_X = ime_rect.x + ime_rect.w - 70
    _G._GAME_MOUSE_Y = ime_rect.y + ime_rect.h - 26
    _G._KAG_onClick()
    check("Click OK button commits value", ctx_ok.f.ok_btn == "HitOK")
    check("Input mode cleared on OK click", ctx_ok._inputMode == false)

    -- Mouse Hit Outside Button (No-Op)
    local ctx_miss = make_ctx()
    local co_miss = coroutine.create(function()
        TextCommands.input(ctx_miss, { name = "f.miss_btn", default = "KeepOpen", btn_ok = "OK", btn_cancel = "Cancel" })
    end)
    coroutine.resume(co_miss)
    _G._GAME_MOUSE_X = 10 -- outside
    _G._GAME_MOUSE_Y = 10
    _G._KAG_onClick()
    check("Click outside buttons keeps input mode active", ctx_miss._inputMode == true)

    -- Mouse Hit Cancel
    _G._GAME_MOUSE_X = ime_rect.x + ime_rect.w - 190
    _G._GAME_MOUSE_Y = ime_rect.y + ime_rect.h - 26
    _G._KAG_onClick()
    check("Click Cancel button discards and exits", ctx_miss.f.miss_btn == nil)
    check("Input mode cleared on Cancel click", ctx_miss._inputMode == false)
end

-- -----------------------------------------------------------------------------
-- 7. Composition Text (IME Pre-edit) & Backspacing
-- -----------------------------------------------------------------------------
print("\n--- TEST 7: Composition Editing (Pre-edit) & Backspace ---")
do
    local ctx = make_ctx()
    local co = coroutine.create(function()
        TextCommands.input(ctx, { name = "f.comp_test", default = "Prefix" })
    end)
    coroutine.resume(co)

    -- IME sends intermediate text editing event
    _G._KAG_onTextEditing("nihon", 0, 5)
    local state = TextScene.get_state(ctx)
    local comp_found = false
    for _, d in ipairs(state.draws) do
        if d.text and d.text:find("%[nihon%]") then
            comp_found = true
            break
        end
    end
    check("composition pre-edit displayed in brackets", comp_found)

    -- Backspace while composition is active should clear composition first
    _G._KAG_onKeyDown(8, "backspace")
    state = TextScene.get_state(ctx)
    local comp_cleared = true
    for _, d in ipairs(state.draws) do
        if d.text and d.text:find("%[nihon%]") then
            comp_cleared = false
            break
        end
    end
    check("backspace clears composition before buffer", comp_cleared)

    -- Now commit final converted text
    _G._KAG_onTextInput("日本")
    _G._KAG_onKeyDown(13, "return")
    check("composition replaced with committed text (Prefix日本)", ctx.f.comp_test == "Prefix日本", ctx.f.comp_test)
end

-- -----------------------------------------------------------------------------
-- 8. Coroutine Interruption & External Resumption Safety
-- -----------------------------------------------------------------------------
print("\n--- TEST 8: Coroutine Interruption & External Resumption Safety ---")
do
    local ctx = make_ctx()
    local co = coroutine.create(function()
        TextCommands.input(ctx, { name = "f.co_test", default = "Interrupted" })
    end)
    coroutine.resume(co)
    check("coroutine yielded and inputMode active", ctx._inputMode == true)

    -- If the runner resumes the coroutine externally (e.g. timeout, debug skip, or forced cycle)
    local ok, err = coroutine.resume(co)
    check("external resume succeeded without error", ok == true, err)
    check("external resume cleanly finishes inputMode", ctx._inputMode == false)
    check("external resume stops IME", ime_active == false)
    check("external resume saves current buffer", ctx.f.co_test == "Interrupted")
    check("UI text group cleaned up", #TextScene.get_state(ctx).draws == 0)
end

-- -----------------------------------------------------------------------------
-- 9. Variable Scope Target Mapping
-- -----------------------------------------------------------------------------
print("\n--- TEST 9: Variable Scope Targets (f / tf / sf / bare) ---")
do
    local ctx = make_ctx()
    local scopes = {
        { name = "f.foo", scope = "f", key = "foo", val = "v1" },
        { name = "tf.bar", scope = "tf", key = "bar", val = "v2" },
        { name = "sf.baz", scope = "sf", key = "baz", val = "v3" },
        { name = "qux",    scope = "f", key = "qux", val = "v4" },
    }
    for _, s in ipairs(scopes) do
        local co = coroutine.create(function()
            TextCommands.input(ctx, { name = s.name, default = s.val })
        end)
        coroutine.resume(co)
        _G._KAG_onKeyDown(13, "return")
        check(string.format("scope target %s writes to ctx.%s.%s", s.name, s.scope, s.key),
            ctx[s.scope][s.key] == s.val, tostring(ctx[s.scope][s.key]))
    end
end

-- -----------------------------------------------------------------------------
-- 10. Conditional Execution (cond parameter)
-- -----------------------------------------------------------------------------
print("\n--- TEST 10: Conditional Execution (cond parameter) ---")
do
    local ctx = make_ctx()
    ctx.f.flag = false
    local ran_true = false
    local co1 = coroutine.create(function()
        TextCommands.input(ctx, { name = "f.cond_false", cond = "f.flag == true" })
        ran_true = true
    end)
    coroutine.resume(co1)
    check("false condition skips input and does not yield", ctx._inputMode ~= true and ran_true == true)
    check("false condition does not write variable", ctx.f.cond_false == nil)

    ctx.f.flag = true
    local co2 = coroutine.create(function()
        TextCommands.input(ctx, { name = "f.cond_true", cond = "f.flag == true", default = "Passed" })
    end)
    coroutine.resume(co2)
    check("true condition activates input", ctx._inputMode == true)
    _G._KAG_onKeyDown(13, "return")
    check("true condition commits value", ctx.f.cond_true == "Passed")
end

-- -----------------------------------------------------------------------------
-- 11. Sequential Chained [input] Prompts in Single Coroutine
-- -----------------------------------------------------------------------------
print("\n--- TEST 11: Sequential Chained [input] Prompts ---")
do
    local ctx = make_ctx()
    local completed = false
    local co = coroutine.create(function()
        TextCommands.input(ctx, { name = "f.first_name", default = "Alice" })
        TextCommands.input(ctx, { name = "f.last_name", default = "Liddell" })
        completed = true
    end)
    coroutine.resume(co)
    check("prompt 1 active", ctx._inputMode == true and ctx.waiting_input == true)
    _G._KAG_onTextInput("a")
    _G._KAG_onKeyDown(13, "return") -- commit first prompt

    -- Advance coroutine (as scheduler does when waiting_input becomes false)
    coroutine.resume(co)

    check("prompt 1 saved Alicea", ctx.f.first_name == "Alicea")
    check("prompt 2 active", ctx._inputMode == true and ctx.waiting_input == true)
    _G._KAG_onTextInput("b")
    _G._KAG_onKeyDown(13, "return") -- commit second prompt

    coroutine.resume(co)

    check("prompt 2 saved Liddellb", ctx.f.last_name == "Liddellb")
    check("sequential script completed", completed == true)
    check("input mode cleared finally", ctx._inputMode == false)
end

-- Restore backend mocks
backend.start_text_input = original_start
backend.stop_text_input = original_stop
backend.set_text_input_rect = original_set_rect

-- Summary
local failed_count = 0
for _, r in ipairs(results) do
    if not r then failed_count = failed_count + 1 end
end

print("\n===============================================================================")
print(string.format("TEST RESULTS: %d Total Assertions, %d Passed, %d Failed",
    total_assertions, total_assertions - failed_count, failed_count))
print("===============================================================================")

if failed_count > 0 then
    error(string.format("[test_input_stress] Verification failure: %d tests failed.", failed_count))
end
print("[test_input_stress] All stress tests PASSED successfully.")
