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

if failed > 0 then os.exit(1) end
print("SWITCH TESTS DONE")
