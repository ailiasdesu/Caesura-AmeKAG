-- =============================================================================
--  test_macro_deep2.lua -- [macro] second-round deep boundaries.
--  Complements test_macro.lua / test_macro_nested.lua / test_macro_deep.lua
--  (round 75 recursion + nested-def) and test_macro_bare.lua.
--
--  NEW angles covered here:
--    1. Macro ARG depth:
--       - a param name with a dot / non-word char is never matched by the
--         %[%w_]+% placeholder regex -- the literal stays, no crash.
--       - a named call that omits a declared arg keeps the placeholder.
--       - 3-level nested macro CALL chain forwarding one arg (depth 3+).
--    2. Macro x VARIABLE:
--       - [eval] inside a macro body mutates the shared ctx.f scope and the
--         effect persists across multiple expansions (expands-into-scope).
--       - [if] inside a macro body whose condition is built from a filled
--         %param% placeholder branches correctly per call.
--    3. Macro x SWITCH:
--       - a macro body that is a complete [switch]...[/endswitch] executes in
--         place when expanded.
--       - a [switch] case whose body contains a macro call expands inside the
--         taken case (and is skipped when the case is not taken).
--    4. erasemacro of a macro mid-chain: later call does not expand.
--
--  Run: external/lua/lua.exe tests/scripts/test_macro_deep2.lua
--  Runtime macro-splice path with hand-built token arrays (fresh per run),
--  independent of the C++ instruction budget.
-- =============================================================================
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, extra)
    if cond then
        print("PASS " .. name)
        passed = passed + 1
    else
        print("FAIL " .. name .. (extra and (" -- " .. tostring(extra)) or ""))
        failed = failed + 1
    end
end

local dispatched = {}
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) dispatched[#dispatched + 1] = { k, p2 } end
end})
local scheduler = require("scheduler")

local function clear_dispatched() dispatched = {} end

-- Run a hand-built token stream (fresh) to completion. Returns (ctx, ok, err).
local function run_tokens(builder, vars)
    local tokens = builder()
    local ctx = { f = vars and vars.f or {}, tf = {}, sf = {}, mp = {},
        variables = vars and vars.variables or {}, macros = nil, macro_args = nil,
        current_scene = "t.ks", token_index = 1, label_index = {} }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    local ok, err = true, nil
    local steps = 0
    while coroutine.status(co) ~= "dead" do
        local rok, rerr = coroutine.resume(co)
        steps = steps + 1
        if steps > 100000 then ok, err = false, "STEP-LIMIT"; break end
        if not rok then ok, err = false, rerr; break end
    end
    return ctx, ok, err
end

local function texts()
    local out = {}
    for _, d in ipairs(dispatched) do
        if d[1] == "ch" and d[2] and d[2].text then out[#out + 1] = d[2].text end
    end
    return out
end
local function hasText(v)
    for _, t in ipairs(texts()) do if t == v then return true end end
    return false
end

-- =============================================================================
-- 1. Macro ARG: a param name with a dot (non-word char) is not matched by the
--    %[%w_]+% placeholder fill -- the literal survives, no crash.
-- =============================================================================
do
    clear_dispatched()
    local ctx, ok, err = run_tokens(function() return {
        { "macro", { name = "dotarg", args = "a.b" } },
        { "ch", { text = "has [%a.b%] and [%x_y%] tail" } },
        { "endmacro" },
        -- redefine to make it dynamic (runtime path)
        { "macro", { name = "dotarg", args = "a.b" } },
        { "ch", { text = "has [%a.b%] and [%x_y%] tail" } },
        { "endmacro" },
        { "dotarg", { ["a.b"] = "V1", x_y = "W" } },
    } end)
    local t = texts()
    local all = table.concat(t or {}, "|")
    -- Key insight (order-robust): the macro-arg fill regex %[%w_]+% never
    -- matches a dot, so the value V1 supplied for the dot-named arg "a.b" is
    -- NEVER substituted (V1 is absent). A plain word placeholder %x_y%
    -- supplied in the same call IS filled (W appears). This pins that macro
    -- args whose names contain a dot (or other non-word char) can never be
    -- filled.
    local v1_absent = true
    for _, tx in ipairs(t or {}) do if tx:find("V1", 1, true) then v1_absent = false end end
    check("dot-named macro arg never substituted (V1 absent)", ok and v1_absent, all)
    check("word-char placeholder from call params still fills", ok and all:find("W", 1, true) ~= nil, all)
end

-- =============================================================================
-- 2. Named call omitting a declared arg keeps the placeholder.
-- =============================================================================
do
    clear_dispatched()
    local _, ok, err = run_tokens(function() return {
        { "macro", { name = "om", args = "who" } },
        { "ch", { text = "hello %who%" } },
        { "endmacro" },
        { "macro", { name = "om", args = "who" } },
        { "ch", { text = "hello %who%" } },
        { "endmacro" },
        { "om", { name = "x" } },   -- wrong key: who not provided
    } end)
    local t = texts()
    check("omitted declared arg keeps %who% literal (no crash)", ok and t[1] == "hello %who%", table.concat(t or {}, "|"))
end

-- =============================================================================
-- 3. 3-level nested macro CALL chain forwarding one arg (depth 3+).
--    outer(who) -> middle(who) -> inner(who); inner renders %who%.
-- =============================================================================
do
    clear_dispatched()
    local _, ok, err = run_tokens(function() return {
        { "macro", { name = "inner", args = "who" } },
        { "ch", { text = "leaf %who%" } },
        { "endmacro" },
        { "macro", { name = "middle", args = "who" } },
        { "inner", { who = "%who%" } },
        { "endmacro" },
        { "macro", { name = "outer", args = "who" } },
        { "middle", { who = "%who%" } },
        { "endmacro" },
        { "macro", { name = "inner", args = "who" } },
        { "ch", { text = "leaf2 %who%" } },
        { "endmacro" },
        { "macro", { name = "middle", args = "who" } },
        { "inner", { who = "%who%" } },
        { "endmacro" },
        { "macro", { name = "outer", args = "who" } },
        { "middle", { who = "%who%" } },
        { "endmacro" },
        { "outer", { who = "Sakura" } },
        { "outer", { who = "Kaito" } },
    } end)
    local t = texts()
    check("3-level macro call chain forwards arg", ok and t[1] == "leaf2 Sakura" and t[2] == "leaf2 Kaito", table.concat(t or {}, "|"))
end

-- =============================================================================
-- 4. Macro x VARIABLE: [eval] inside a macro body mutates the shared ctx.f
--    scope; the value persists across multiple expansions.
-- =============================================================================
do
    clear_dispatched()
    local ctx, ok, err = run_tokens(function() return {
        { "macro", { name = "cnt" } },
        { "eval", { exp = "f.count = (f.count or 0) + 1" } },
        { "ch", { text = "tick" } },
        { "endmacro" },
        { "macro", { name = "cnt" } },
        { "eval", { exp = "f.count = (f.count or 0) + 1" } },
        { "ch", { text = "tick2" } },
        { "endmacro" },
        { "cnt" },
        { "cnt" },
        { "cnt" },
    } end)
    check("macro [eval] increments shared f.count across 3 expansions", ok and (ctx.f.count or 0) == 3, "count=" .. tostring(ctx.f and ctx.f.count))
end

-- =============================================================================
-- 5. Macro x VARIABLE: [if] inside a macro body whose condition is built from
--    a filled %w% placeholder. Branching depends on the per-call value.
-- =============================================================================
do
    local mk = function(expTxt, bodyTxt)
        return {
            { "macro", { name = "mi", args = "w" } },
            { "if", { exp = expTxt } },
            { "ch", { text = bodyTxt } },
            { "endif" },
            { "endmacro" },
            { "macro", { name = "mi", args = "w" } },
            { "if", { exp = expTxt } },
            { "ch", { text = bodyTxt .. "2" } },
            { "endif" },
            { "endmacro" },
        }
    end
    -- %w% is filled (f.flag / f.flag) at splice; the branch runs per call value.
    clear_dispatched()
    local _, ok, err = run_tokens(function()
        local t = mk("f.%w% == 1", "flag-on")
        t[#t + 1] = { "mi", { w = "flag" }, { __callsite = true } }
        t[#t + 1] = { "mi", { w = "flag" }, { __callsite = true } }
        t[#t + 1] = { "ch", { text = "end" } }
        return t
    end, { f = { flag = 1 } })
    local t = texts()
    check("macro body [if] with %w% param: true branch runs", ok and hasText("flag-on2"), table.concat(t or {}, "|"))
    check("still dispatches after macro-if", ok and hasText("end"))
    -- false flag: the if body is skipped
    clear_dispatched()
    local _, ok2 = run_tokens(function()
        local t = mk("f.%w% == 1", "flag-on")
        t[#t + 1] = { "mi", { w = "flag" } }
        return t
    end, { f = { flag = 0 } })
    check("macro body [if] false branch skipped", ok2 and not hasText("flag-on2") and not hasText("flag-on"))
end

-- =============================================================================
-- 6. Macro x SWITCH: a macro body that is a complete [switch] executes in place.
-- =============================================================================
do
    clear_dispatched()
    local _, ok, err = run_tokens(function() return {
        { "macro", { name = "modeSw" } },
        { "switch", { "mode" } },
        { "case", { "fast" } }, { "ch", { text = "M-fast" } },
        { "case", { "slow" } }, { "ch", { text = "M-slow" } },
        { "default" },            { "ch", { text = "M-default" } },
        { "endswitch" },
        { "endmacro" },
        { "macro", { name = "modeSw" } },
        { "switch", { "mode" } },
        { "case", { "fast" } }, { "ch", { text = "M2-fast" } },
        { "case", { "slow" } }, { "ch", { text = "M2-slow" } },
        { "default" },            { "ch", { text = "M2-default" } },
        { "endswitch" },
        { "endmacro" },
        { "modeSw" },
        { "ch", { text = "after-macro-switch" } },
    } end, { variables = { mode = "fast" } })
    check("macro body switch dispatches case (fast)", ok and hasText("M2-fast"), texts() and table.concat(texts(), "|"))
    check("macro body switch skips other cases", not hasText("M2-slow") and not hasText("M2-default"))
    check("switch macro resumes after body", ok and hasText("after-macro-switch"))
    check("no expansion depth error", ok, err)
end

-- =============================================================================
-- 7. Macro x SWITCH: a [switch] case containing a macro call expands inside
--    the taken case; a non-taken case's macro call never expands.
-- =============================================================================
do
    clear_dispatched()
    local _, ok, err = run_tokens(function() return {
        { "macro", { name = "tiny" } },
        { "ch", { text = "TINY-BODY" } },
        { "endmacro" },
        { "macro", { name = "tiny" } },
        { "ch", { text = "TINY-BODY2" } },
        { "endmacro" },
        { "switch", { "mode" } },
        { "case", { "fast" } },
        { "tiny" },
        { "ch", { text = "after-tiny-in-case" } },
        { "case", { "slow" } },
        { "tiny" },
        { "ch", { text = "slow-branch" } },
        { "endswitch" },
        { "ch", { text = "finale" } },
    } end, { variables = { mode = "fast" } })
    check("macro expands inside taken case", ok and hasText("TINY-BODY2"))
    check("taken case continues after macro", ok and hasText("after-tiny-in-case"))
    check("non-taken case macro never expands", not hasText("slow-branch"))
    check("finale runs", ok and hasText("finale"))
end

-- =============================================================================
-- 8. erasemacro mid-chain: after erase, a later call does not expand the body.
-- =============================================================================
do
    clear_dispatched()
    local ctx, ok, err = run_tokens(function() return {
        { "macro", { name = "m" } },
        { "ch", { text = "BODY" } },
        { "endmacro" },
        { "macro", { name = "m" } },
        { "ch", { text = "BODY" } },
        { "endmacro" },
        { "m" },
        { "erasemacro", { name = "m" } },
        { "m" },
    } end)
    -- first m expands (BODY ran once), second m after erase does not.
    local bodyCount = 0
    for _, t in ipairs(texts()) do if t == "BODY" then bodyCount = bodyCount + 1 end end
    check("first call expands, erasing then suppresses later call", bodyCount == 1, "bodyCount=" .. bodyCount)
end

package.loaded["kag"] = kag_orig
print(string.format("\nMACRO DEEP2: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end