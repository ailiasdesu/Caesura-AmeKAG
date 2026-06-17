-- =============================================================================
--  Caesura (AmeKAG) �?tests/scripts/test_macro.lua
--  U1.5: macro record, erase, and expansion tests
--  Run: external\lua\lua.exe tests/scripts/test_macro.lua
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;scripts/kag/commands/?.lua;" .. package.path

local scheduler = require("scheduler")
local tokenizer = require("tokenizer")
local passed, failed = 0, 0

local function assert_eq(actual, expected, msg)
    if actual == expected then
        passed = passed + 1
    else
        failed = failed + 1
        print(string.format("FAIL: %s (expected %s, got %s)", msg, tostring(expected), tostring(actual)))
    end
end

local function run_script(script_text)
    local tokens = tokenizer.parse(script_text)
    local ctx = {}
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do
        local ok, err = coroutine.resume(co)
        if not ok then
            print("  Scheduler error:", err)
            return nil
        end
    end
    return ctx
end

-- ══════════════════════════════════════════════════════════════════════════�?-- Test 1: macro records body
-- ══════════════════════════════════════════════════════════════════════════�?
do
    local ctx = run_script([=[
[macro name="greet"]
[text text="Hello!"]
[p]
[endmacro]
]=])
    if ctx then
        assert_eq(ctx.macros ~= nil, true, "macros should exist")
        if ctx.macros then
            assert_eq(ctx.macros["greet"] ~= nil, true, "macro greet should be recorded")
            if ctx.macros["greet"] then
                assert_eq(#ctx.macros["greet"], 2, "macro body should have 2 tokens")
                assert_eq(ctx.macros["greet"][1][1], "text", "first body token should be text")
                assert_eq(ctx.macros["greet"][2][1], "p", "second body token should be p")
            end
        end
    else
        failed = failed + 1
        print("FAIL: scheduler crashed on macro recording")
    end
end

-- ══════════════════════════════════════════════════════════════════════════�?-- Test 2: erasemacro removes macro
-- ══════════════════════════════════════════════════════════════════════════�?
do
    local ctx = run_script([=[
[macro name="temp"]
[text text="body"]
[endmacro]
[erasemacro name="temp"]
]=])
    if ctx then
        assert_eq(ctx.macros ~= nil, true, "macros should exist after recording")
        assert_eq(ctx.macros["temp"], nil, "macro temp should be erased")
    else
        failed = failed + 1
        print("FAIL: scheduler crashed on erasemacro")
    end
end

-- ══════════════════════════════════════════════════════════════════════════�?-- Test 3: macro expansion executes body
-- ══════════════════════════════════════════════════════════════════════════�?
do
    local ctx = run_script([=[
[macro name="doneflag"]
[eval exp="done = true"]
[endmacro]
[doneflag]
[setflag]
]=])
    if ctx then
        assert_eq(ctx.macros ~= nil, true, "macros should exist")
        if ctx.macros then
            assert_eq(ctx.macros["setflag"] ~= nil, true, "macro should be recorded")
        end
        assert_eq(ctx.backlog and #ctx.backlog == 1, true, "macro expansion should create backlog entry")
    else
        failed = failed + 1
        print("FAIL: scheduler crashed on macro expansion")
    end
end

-- ══════════════════════════════════════════════════════════════════════════�?-- Test 4: macro without name is not recorded
-- ══════════════════════════════════════════════════════════════════════════�?
do
    local ctx = run_script([=[
[macro]
[text text="no name"]
[endmacro]
]=])
    if ctx then
        local empty = (ctx.macros == nil) or (next(ctx.macros) == nil)
        assert_eq(empty, true, "no macro should be recorded without name")
    else
        failed = failed + 1
        print("FAIL: scheduler crashed on unnamed macro")
    end
end

-- ══════════════════════════════════════════════════════════════════════════�?-- Results
-- ══════════════════════════════════════════════════════════════════════════�?
print(string.format("\nMacro tests: %d passed, %d failed", passed, failed))
if failed > 0 then
    os.exit(1)
end
