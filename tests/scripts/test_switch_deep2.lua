-- =============================================================================
--  test_switch_deep2.lua -- [switch] second-round deep boundaries.
--  Complements test_switch.lua (round 73) and the nested-shape siblings
--  (test_switch_exotic / test_switch_scan / test_switch_taken_nested).
--
--  NEW angles covered here:
--    1. Expression-depth selectors: ternary '?' and '??' in [switch exp=...]
--       (integration with the round-68/round-53 expression machinery, pinned
--       to the scheduler's tostring-equality semantics).
--    2. Case values as literals with quotes / embedded escapes.
--    3. Control flow INSIDE a taken case body: [jump *label], [call]+[return],
--       and a bare [return] -- switch stack hygiene after each exit.
--    4. Nested switch with exp= selectors on both levels.
--
--  Run: external/lua/lua.exe tests/scripts/test_switch_deep2.lua
--  Hand-built token arrays (fresh per run), exactly like the sibling switch
--  tests: the compiler binds the kag mock once per token stream.
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

local scheduler = require("scheduler")

local dispatched = {}
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) dispatched[#dispatched + 1] = { k, p2 } end
end})
local function clear_dispatched() dispatched = {} end

-- Run a hand-built fresh token stream (built by builder) to completion.
-- Returns (ok, err). Captures resume errors so a broken exp or a control-flow
-- assertion surfacing as resume=false is reported, not swallowed.
local function run_tokens(builder, vars)
    local tokens = builder()
    local ctx = { f = vars.f or {}, tf = {}, sf = {}, mp = {},
        variables = vars.variables or {}, macros = nil, macro_args = nil,
        current_scene = "t.ks", token_index = 1, label_index = {} }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    local ok, err = true, nil
    local steps = 0
    while coroutine.status(co) ~= "dead" do
        local rok, rerr = coroutine.resume(co)
        steps = steps + 1
        if steps > 8000 then ok, err = false, "STEP-LIMIT"; break end
        if not rok then ok, err = false, rerr; break end
    end
    return ok, err
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

-- 1. Expression depth: ternary ?: selector (round 68 machinery).
do
    clear_dispatched()
    local ok, err = run_tokens(function() return {
        { "switch", { exp = "f.a > 0 ? 'pos' : 'neg'" } },
        { "case", { "pos" } }, { "ch", { text = "positive" } },
        { "case", { "neg" } }, { "ch", { text = "negative" } },
        { "default" },          { "ch", { text = "default-case" } },
        { "endswitch" },
        { "ch", { text = "after" } },
    } end, { f = { a = 5 } })
    local t = texts()
    check("switch exp ternary ?: picks pos branch", ok and t[1] == "positive", table.concat(t or {}, "|"))
    check("ternary switch not run neg/default", not hasText("negative") and not hasText("default-case"), err)

    clear_dispatched()
    local ok2 = run_tokens(function() return {
        { "switch", { exp = "f.a > 0 ? 'pos' : 'neg'" } },
        { "case", { "pos" } }, { "ch", { text = "positive" } },
        { "case", { "neg" } }, { "ch", { text = "negative" } },
        { "default" },          { "ch", { text = "default-case" } },
        { "endswitch" },
        { "ch", { text = "after" } },
    } end, { f = { a = -2 } })
    local t2 = texts()
    check("switch exp ternary ?: picks neg branch when falsy", ok2 and t2[1] == "negative", table.concat(t2 or {}, "|"))
end

-- 2. Expression depth: ?? nullish selector (round 53 machinery).
do
    clear_dispatched()
    local ok, err = run_tokens(function() return {
        { "switch", { exp = "f.missing ?? 'fallback'" } },
        { "case", { "fallback" } }, { "ch", { text = "used-fallback" } },
        { "case", { "x" } },         { "ch", { text = "case-x" } },
        { "default" },               { "ch", { text = "default-case" } },
        { "endswitch" },
        { "ch", { text = "after" } },
    } end, { f = {} })
    local t = texts()
    check("switch exp ?? nullish falls back to case key", ok and t[1] == "used-fallback", table.concat(t or {}, "|"))
    check("?? no empty key sentinel / no error", ok, err)

    clear_dispatched()
    local ok2 = run_tokens(function() return {
        { "switch", { exp = "f.gold ?? 'fallback'" } },
        { "case", { "150" } },      { "ch", { text = "gold-150" } },
        { "case", { "fallback" } }, { "ch", { text = "used-fallback" } },
        { "default" },              { "ch", { text = "default-case" } },
        { "endswitch" },
        { "ch", { text = "after" } },
    } end, { f = { gold = 150 } })
    local t2 = texts()
    check("switch exp ?? keeps present value", ok2 and t2[1] == "gold-150", table.concat(t2 or {}, "|"))
end

-- 3. Case values are literals: quotes / escapes stay part of the key; an
--    expression-looking header (e.g. "1 == 1") is NOT evaluated.
do
    clear_dispatched()
    local ok, err = run_tokens(function() return {
        { "switch", { exp = "f.k" } },
        { "case", { "'quoted'" } }, { "ch", { text = "single-quoted" } },
        { "case", { "it's" } },     { "ch", { text = "apostrophe" } },
        { "case", { "a\\nb" } },    { "ch", { text = "escaped-newline" } },
        { "default" },              { "ch", { text = "default-case" } },
        { "endswitch" },
        { "ch", { text = "after" } },
    } end, { f = { k = "it's" } })
    check("quoted case value matched literally", ok and hasText("apostrophe"), err)

    clear_dispatched()
    local ok2 = run_tokens(function() return {
        { "switch", { exp = "f.k" } },
        { "case", { "1 == 1" } }, { "ch", { text = "expr-literal" } },
        { "case", { "true" } },   { "ch", { text = "bool-key" } },
        { "default" },            { "ch", { text = "default-case" } },
        { "endswitch" },
        { "ch", { text = "after" } },
    } end, { f = { k = true } })
    check("expression-looking case header treated as literal key", ok2 and hasText("bool-key"))
end

-- 4. Control flow inside a TAKEN case: [jump *label] leaves the switch.
do
    clear_dispatched()
    local ok, err = run_tokens(function() return {
        { "switch", { exp = "f.m" } },
        { "case", { "a" } },
        { "ch", { text = "case-a" } },
        { "jump", { storage = "*leave" } },
        { "endswitch" },
        { "ch", { text = "unreachable-after-jump" } },
        { "label", { name = "leave" } },
        { "ch", { text = "after-jump" } },
        { "switch", { exp = "f.m" } },
        { "case", { "a" } }, { "ch", { text = "second-switch-b" } },
        { "default" },        { "ch", { text = "second-default" } },
        { "endswitch" },
        { "ch", { text = "finale" } },
    } end, { f = { m = "a" } })
    check("jump inside taken case exits switch", ok and not hasText("unreachable-after-jump"), err)
    check("jump lands on label after switch", ok and hasText("after-jump"))
    check("switch state clean after jump (new switch dispatches)", ok and hasText("second-switch-b"))
    check("finale runs after second switch", ok and hasText("finale"))
end

-- 5. [call *sub] + [return] inside a taken case (return pops the CALL frame,
--    not the switch entry; the case resumes and still reaches [endswitch]).
do
    clear_dispatched()
    local ok, err = run_tokens(function() return {
        { "switch", { exp = "f.m" } },
        { "case", { "a" } },
        { "ch", { text = "case-a" } },
        { "call", { storage = "*sub" } },
        { "ch", { text = "after-call-in-case" } },
        { "endswitch" },
        { "ch", { text = "after-switch" } },
        { "jump", { storage = "*end" } },
        { "label", { name = "sub" } },
        { "ch", { text = "in-sub" } },
        { "return" },
        { "ch", { text = "UNREACH-sub" } },
        { "label", { name = "end" } },
        { "ch", { text = "finale" } },
    } end, { f = { m = "a" } })
    check("call inside taken case runs sub", ok and hasText("in-sub"), err)
    check("return pops call frame, case continues", ok and hasText("after-call-in-case"))
    check("case reaches endswitch, after-switch runs", ok and hasText("after-switch"))
    check("sub body not re-entered", not hasText("UNREACH-sub"))
    check("finale runs", ok and hasText("finale"))
end

-- 6. [return] directly inside a taken case aborts the scene (KAG3 statement
--    return): tokens after it never run, a later switch is unaffected.
do
    clear_dispatched()
    local ok, err = run_tokens(function() return {
        { "switch", { exp = "f.m" } },
        { "case", { "a" } },
        { "ch", { text = "case-a" } },
        { "return" },
        { "endswitch" },
        { "ch", { text = "UNREACH-after-return" } },
        { "switch", { exp = "f.m" } },
        { "case", { "a" } }, { "ch", { text = "second-a" } },
        { "endswitch" },
        { "ch", { text = "finale" } },
    } end, { f = { m = "a" } })
    check("return inside case ran the case body", ok and hasText("case-a"))
    check("nothing after [return] runs", not hasText("UNREACH-after-return") and not hasText("second-a") and not hasText("finale"))
end

-- 7. Nested switch, both levels exp= selectors.
do
    clear_dispatched()
    local ok, err = run_tokens(function() return {
        { "switch", { exp = "f.mode" } },
        { "case", { "fast" } },
        { "ch", { text = "outer-fast" } },
        { "switch", { exp = "f.sub" } },
        { "case", { "x" } }, { "ch", { text = "inner-x" } },
        { "case", { "y" } }, { "ch", { text = "inner-y" } },
        { "endswitch" },
        { "ch", { text = "outer-after-inner" } },
        { "case", { "slow" } },
        { "ch", { text = "outer-slow" } },
        { "endswitch" },
        { "ch", { text = "final" } },
    } end, { f = { mode = "fast", sub = "y" } })
    check("nested exp-switch: outer matches", ok and hasText("outer-fast"), err)
    check("nested exp-switch: inner dispatches y", ok and hasText("inner-y"))
    check("outer resumes after inner endswitch", ok and hasText("outer-after-inner"))
    check("later outer case skipped", not hasText("outer-slow"))
    check("final runs", ok and hasText("final"))
end

package.loaded["kag"] = kag_orig
print(string.format("\nSWITCH DEEP2: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end