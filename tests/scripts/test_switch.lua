-- test_switch.lua — switch/case no-fallthrough (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local function runWith(v)
    local tokens = {
        { "switch", { "mode" } },
        { "case", { "fast" } },
        { "ch", { name = "A", text = "fast-case" } },
        { "case", { "slow" } },
        { "ch", { name = "B", text = "slow-case" } },
        { "default" },
        { "ch", { name = "C", text = "default-case" } },
        { "endswitch" },
        { "ch", { name = "D", text = "after" } },
    }
    local d = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d[#d + 1] = { k, p2 } end
    end})
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = { mode = v },
        macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    return d
end

local d = runWith("fast")
check("fast case runs", d[1] and d[1][2].text == "fast-case")
check("no fallthrough into slow", (function()
    for _, x in ipairs(d) do if x[2].text == "slow-case" then return false end end
    return true end)())
check("default skipped", (function()
    for _, x in ipairs(d) do if x[2].text == "default-case" then return false end end
    return true end)())
check("after runs", d[2] and d[2][2].text == "after")
local d2 = runWith("other")
check("no match -> default", d2[1] and d2[1][2].text == "default-case")
check("default then after", d2[2] and d2[2][2].text == "after")
local d3 = runWith("slow")
check("slow case independent", d3[1] and d3[1][2].text == "slow-case")

-- ---- [switch exp="..."] expression selector (round 55) ----
-- Named exp= is a TJS expression (same machinery as [if]); the KAG3
-- positional form above stays a bare variable name. Comparison is
-- tostring equality for both forms.
local function runWithExp(exp, vars, cases, hasDefault)
    local tokens = {
        { "switch", { exp = exp } },
    }
    for _, cv in ipairs(cases) do
        tokens[#tokens + 1] = { "case", { cv } }
        tokens[#tokens + 1] = { "ch", { name = "A", text = "case-" .. cv } }
    end
    if hasDefault then
        tokens[#tokens + 1] = { "default" }
        tokens[#tokens + 1] = { "ch", { name = "C", text = "default-case" } }
    end
    tokens[#tokens + 1] = { "endswitch" }
    tokens[#tokens + 1] = { "ch", { name = "D", text = "after" } }
    local d = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d[#d + 1] = { k, p2 } end
    end})
    local ctx = { f = vars.f or {}, tf = {}, sf = {}, mp = {},
        variables = vars.variables or {}, macros = nil, macro_args = nil,
        current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    return d
end

local de = runWithExp("f.mode", { f = { mode = "fast" } },
    { "fast", "slow" }, false)
check("exp= variable parity with bare form", de[1] and de[1][2].text == "case-fast")
local de2 = runWithExp("f.gold", { f = { gold = 150 } }, { "150", "100" }, true)
check("exp= numeric tostring match", de2[1] and de2[1][2].text == "case-150")
local de2b = runWithExp("f.gold", { f = { gold = 200 } }, { "150", "100" }, true)
check("exp= non-matching case falls to default", de2b[1] and de2b[1][2].text == "default-case")
local de3 = runWithExp("f.gold >= 100", { f = { gold = 150 } }, { "true", "false" }, true)
check("exp= boolean expression case true", de3[1] and de3[1][2].text == "case-true")
local de4 = runWithExp("f.gold >= 100", { f = { gold = 50 } }, { "true" }, true)
check("exp= false expression -> default", de4[1] and de4[1][2].text == "default-case")
local de5 = runWithExp("f.flag && f.hp > 10", { f = { flag = true, hp = 30 } },
    { "true", "false" }, true)
check("exp= TJS && translated", de5[1] and de5[1][2].text == "case-true")
local de6 = runWithExp("f.nope", { f = {} }, { "x" }, true)
check("exp= missing var -> default, no error", de6[1] and de6[1][2].text == "default-case")

-- e2e through the tokenizer: full .ks source with exp=
do
    local tokenizer = require("tokenizer")
    local toks = tokenizer.parse(
        "[switch exp=\"f.tier\"]\n[case 2]\n[ch name=\"A\" text=\"two\"]\n"
        .. "[case 1]\n[ch name=\"B\" text=\"one\"]\n[endswitch]\n"
        .. "[ch name=\"D\" text=\"after\"]")
    local d = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d[#d + 1] = { k, p2 } end
    end})
    local ctx = { f = { tier = 2 }, tf = {}, sf = {}, mp = {},
        variables = {}, macros = nil, macro_args = nil,
        current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function() scheduler.run(ctx, toks, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    check("e2e .ks switch exp= selects case 2", d[1] and d[1][2].text == "two")
    check("e2e continues after endswitch", d[2] and d[2][2].text == "after")
end

-- ---- [switch] semantic boundaries (round 73) ----
-- tostring-equality reduces numeric/float/boolean selectors to string keys
-- ([case 1] and [case "1"] are the SAME key "1"; [case 1.0] is "1.0", distinct
-- from "1"). Case values are LITERALS -- a ${f.x} interpolation is not
-- evaluated. break/continue are loop-only: inside a switch with no loop they
-- throw loudly. Every run uses a FRESH token table: the compiler binds the
-- kag handler once on first compile, so reusing one token array while swapping
-- package.loaded["kag"] (the harness mock) would pin the FIRST run mock and
-- silently drop later runs (the real engine installs kag once, so this is a
-- test-harness rule, not an engine limit).
local function runSwitchTok(builder, vars)
    local tokens = builder()
    local d = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d[#d + 1] = { k, p2 } end
    end})
    local ctx = { f = vars.f or {}, tf = {}, sf = {}, mp = {},
        variables = vars.variables or {}, macros = nil, macro_args = nil,
        current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    local ok, err = true, nil
    while coroutine.status(co) ~= "dead" do
        local r, e = coroutine.resume(co)
        if not r then ok, err = false, e break end
    end
    package.loaded["kag"] = kag_orig
    return d, ok, err
end
local function textsOf(d)
    local out = {}
    for _, x in ipairs(d) do out[#out + 1] = x[2].text end
    return out
end
-- mkc(caseVals, hasDefault) -> a FRESH switch exp=f.x stream, one ch per case.
local function mkc(caseVals, hasDefault)
    return function()
        local t = { { "switch", { exp = "f.x" } } }
        for _, cv in ipairs(caseVals) do
            t[#t + 1] = { "case", { cv } }
            t[#t + 1] = { "ch", { name = "A", text = "case-" .. tostring(cv) } }
        end
        if hasDefault then
            t[#t + 1] = { "default" }
            t[#t + 1] = { "ch", { name = "C", text = "default-case" } }
        end
        t[#t + 1] = { "endswitch" }
        t[#t + 1] = { "ch", { name = "D", text = "after" } }
        return t
    end
end

-- 1) numeric/float/boolean selector typing:
do
    -- 1 and "1" are the SAME key; "1" and true/1.0 are distinct keys.
    local dA = runSwitchTok(mkc({ 1, "1" }), { f = { x = 1 } })
    check("exp selector numeric 1 collapses to key 1", dA[1] and dA[1][2].text == "case-1")
    local dF = runSwitchTok(mkc({ 1, "1", 1.0 }), { f = { x = 1.0 } })
    check("exp selector float 1.0 distinct key", dF[1] and dF[1][2].text == "case-1.0")
    local dB = runSwitchTok(mkc({ 1, "1", true }), { f = { x = true } })
    check("exp selector boolean true -> key true", dB[1] and dB[1][2].text == "case-true")
    local dInt = runSwitchTok(mkc({ 1.0 }), { f = { x = 1 } })
    check("exp selector integer 1 does NOT match float-only case", dInt[1] and dInt[1][2].text == "after")
    -- both key 1: LAST source case body wins (deterministic, documented).
    local dDup = runSwitchTok(function() return {
        { "switch", { exp = "f.x" } },
        { "case", { 1 } },   { "ch", { name = "A", text = "int-first" } },
        { "case", { "1" } }, { "ch", { name = "B", text = "str-last" } },
        { "endswitch" }, { "ch", { name = "C", text = "after" } },
    } end, { f = { x = 1 } })
    check("duplicate-equivalent key: last source case body wins", dDup[1] and dDup[1][2].text == "str-last")
end

-- 2) case values are literals: an interpolation string is NOT evaluated.
do
    local interp = "${f.one}"  -- literal case header text
    local d = runSwitchTok(function() return {
        { "switch", { exp = "f.n" } },
        { "case", { interp } }, { "ch", { name = "A", text = "interp-case" } },
        { "case", { "1" } },     { "ch", { name = "B", text = "lit-case" } },
        { "default" },            { "ch", { name = "C", text = "default-case" } },
        { "endswitch" },
        { "ch", { name = "D", text = "after" } },
    } end, { f = { n = 1, one = 1 } })
    check("case value literal (no interpolation)", d[1] and d[1][2].text == "lit-case")
    check("literal non-match keeps default unreached", (function()
        for _, t in ipairs(textsOf(d)) do
            if t == "interp-case" or t == "default-case" then return false end
        end
        return true end)())
end

-- 3) default position (middle vs end):
do
    local mkMid = function() return {
        { "switch", { exp = "f.m" } },
        { "case", { "a" } }, { "ch", { name = "A", text = "case-a" } },
        { "default" },        { "ch", { name = "C", text = "default-case" } },
        { "case", { "b" } }, { "ch", { name = "B", text = "case-b" } },
        { "endswitch" },
        { "ch", { name = "D", text = "after" } },
    } end
    local dMid = runSwitchTok(mkMid, { f = { m = "b" } })
    check("default in middle: later case still matches", dMid[1] and dMid[1][2].text == "case-b")
    check("default in middle: matched case skips default", (function()
        for _, t in ipairs(textsOf(dMid)) do if t == "default-case" then return false end end
        return true end)())
    local dDef = runSwitchTok(mkMid, { f = { m = "zz" } })
    check("default in middle: no match falls to default", dDef[1] and dDef[1][2].text == "default-case")
    check("default in middle: continues after", (function()
        local T = textsOf(dDef)
        return T[#T] == "after" end)())
    local dEnd = runSwitchTok(function() return {
        { "switch", { exp = "f.m" } },
        { "case", { "a" } }, { "ch", { name = "A", text = "case-a" } },
        { "default" },        { "ch", { name = "C", text = "default-case" } },
        { "endswitch" },
        { "ch", { name = "D", text = "after" } },
    } end, { f = { m = "other" } })
    check("default at end: no match falls to default", dEnd[1] and dEnd[1][2].text == "default-case")
end

-- 4) no matching case AND no default:
do
    local d = runSwitchTok(mkc({ 1 }), { f = { x = 99 } })
    check("no match + no default: body skipped", (function()
        for _, t in ipairs(textsOf(d)) do if t == "case-1" then return false end end
        return true end)())
    check("no match + no default: continues after", d[1] and d[1][2].text == "after")
end

-- 5) nested switch + [if] mixing (both nesting directions):
do
    local mkSInIf = function() return {
        { "if", { exp = "f.flag" } },
        { "ch", { name = "Q", text = "if-true" } },
        { "switch", { exp = "f.t" } },
        { "case", { "x" } }, { "ch", { name = "A", text = "switch-x" } },
        { "case", { "y" } }, { "ch", { name = "B", text = "switch-y" } },
        { "endswitch" },
        { "ch", { name = "R", text = "after-switch" } },
        { "endif" },
        { "ch", { name = "S", text = "after-if" } },
    } end
    local d = runSwitchTok(mkSInIf, { f = { flag = true, t = "x" } })
    check("switch inside #[if]# body runs", d[1] and d[1][2].text == "if-true")
    check("switch inside #[if]# dispatches case", d[2] and d[2][2].text == "switch-x")
    check("switch inside #[if]# continues in branch", d[3] and d[3][2].text == "after-switch")
    check("switch inside #[if]# resumes after endif", d[4] and d[4][2].text == "after-if")

    local dF = runSwitchTok(mkSInIf, { f = { flag = false, t = "x" } })
    check("switch inside false #[if]# skipped", (function()
        for _, t in ipairs(textsOf(dF)) do
            if t == "if-true" or t == "switch-x" or t == "after-switch" then return false end
        end
        return true end)())
    check("after false #[if]# runs", dF[1] and dF[1][2].text == "after-if")

    local dI = runSwitchTok(function() return {
        { "switch", { exp = "f.m" } },
        { "case", { "a" } },
        { "ch", { name = "A", text = "case-a" } },
        { "if", { exp = "f.sub" } }, { "ch", { name = "B", text = "sub-true" } }, { "endif" },
        { "endswitch" },
        { "ch", { name = "C", text = "final" } },
    } end, { f = { m = "a", sub = true } })
    check("#[if]# inside taken case runs", dI[2] and dI[2][2].text == "sub-true")
    check("#[if]# inside taken case then endswitch", dI[3] and dI[3][2].text == "final")

    local dS = runSwitchTok(function() return {
        { "switch", { exp = "f.m" } },
        { "case", { "a" } },
        { "ch", { name = "A", text = "case-a" } },
        { "case", { "b" } },
        { "if", { exp = "f.sub" } }, { "ch", { name = "B", text = "skipped-if" } }, { "endif" },
        { "endswitch" },
        { "ch", { name = "C", text = "final" } },
    } end, { f = { m = "a", sub = true } })
    check("#[if]# inside skipped case not run", (function()
        for _, t in ipairs(textsOf(dS)) do if t == "skipped-if" then return false end end
        return true end)())
    check("after skipped-case #[if]# runs", dS[#dS] and dS[#dS][2].text == "final")
end

-- 6) break/continue are loop-only: inside a switch with no loop -> loud error:
do
    local _, okB, errB = runSwitchTok(function() return {
        { "switch", { exp = "f.m" } },
        { "case", { "a" } }, { "ch", { name = "A", text = "case-a" } },
        { "break" },
        { "endswitch" },
    } end, { f = { m = "a" } })
    check("break inside switch (no loop) throws", (not okB) and type(errB) == "string"
        and errB:find("[Bb]reak") ~= nil)
    local _, okC, errC = runSwitchTok(function() return {
        { "switch", { exp = "f.m" } },
        { "case", { "a" } }, { "ch", { name = "A", text = "case-a" } },
        { "continue" },
        { "endswitch" },
    } end, { f = { m = "a" } })
    check("continue inside switch (no loop) throws", (not okC) and type(errC) == "string"
        and errC:find("[Cc]ontinue") ~= nil)
end

if failed > 0 then os.exit(1) end
print("SWITCH TESTS DONE")
