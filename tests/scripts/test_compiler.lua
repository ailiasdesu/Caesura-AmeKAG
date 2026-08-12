-- test_compiler.lua — compile-time front-end tests (scripts/kag/compiler.lua)
-- Phase A of the Neo-Genesis core rewrite: token streams compile once into
-- a side table (flow jump table, pre-translated expressions, normalized
-- params, handler bindings, label index) that the scheduler consumes.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local tokenizer = require("tokenizer")
local compiler = require("kag.compiler")

-- ---------------------------------------------------------------------------
-- 1. compile() produces the side table; idempotent recompile
-- ---------------------------------------------------------------------------
local toks = tokenizer.parse(
    "[ch text=\"a\"]\n*start\n[if exp=\"f.hp > 10 && f.flag != 0\"]\n"
    .. "[ch text=\"ok\"]\n[else]\n[ch text=\"no\"]\n[endif]\n[jump target=*start]")
compiler.compile(toks)
check("compiled side table present", toks._compiled ~= nil)
check("labels indexed at compile time", toks._compiled.labels.start == 2)
local flow = toks._compiled.flow
check("if jump_false compiled (first elseif/else/endif)",
      flow[3] and flow[3].kind == "if" and flow[3].jump_false == 5)
check("else jump_chain_end compiled", flow[5] and flow[5].kind == "else"
      and flow[5].jump_chain_end == 7)
check("jump target recorded", flow[8] and flow[8].kind == "jump"
      and flow[8].target == "*start")
check("TJS && translated at compile time",
      toks._compiled.exprs[3] == "f.hp > 10 and f.flag ~= 0")

-- idempotent: recompile keeps the same side table (no double work)
local first = toks._compiled
compiler.compile(toks)
check("recompile is a no-op", toks._compiled == first)

-- invalidate drops the table; recompile rebuilds
compiler.invalidate(toks)
check("invalidate drops side table", toks._compiled == nil)
compiler.compile(toks)
check("recompile after invalidate rebuilds", toks._compiled ~= nil)

-- ---------------------------------------------------------------------------
-- 2. params normalized: bare positional -> numeric key, pairs -> named
-- ---------------------------------------------------------------------------
local toks2 = tokenizer.parse("[delay 500][ch text=\"x\" speed=10]")
compiler.compile(toks2)
local p1 = toks2._compiled.params[1]
check("bare positional -> params[1] numeric", p1[1] == "500")
local p2 = toks2._compiled.params[2]
check("named params normalized", p2.text == "x" and p2.speed == "10")

-- 2b. compile-time positional -> named via contract positional_index
pcall(require, "kag.commands.system")
pcall(require, "kag.commands.video")
local toks2b = tokenizer.parse("[set f.hp 30][video test.mpg]")
compiler.compile(toks2b)
local ps = toks2b._compiled.params
check("set bare -> var mapped", ps[1].var == "f.hp" and ps[1][1] == "f.hp")
check("video bare -> file mapped", ps[2].file == "test.mpg")
check("unmigrated bare keeps numeric key only",
      toks2._compiled.params[1][1] == "500"
      and toks2._compiled.params[1].ms == nil)

-- ---------------------------------------------------------------------------
-- 3. while/for/break/continue jump targets compiled
-- ---------------------------------------------------------------------------
local toks3 = tokenizer.parse(
    "[while exp=\"f.i < 5\"]\n[ch text=\"loop\"]\n[if exp=\"f.i > 3\"]\n"
    .. "[break]\n[endif]\n[endwhile]\n[for var=i start=0 end=3]\n"
    .. "[continue]\n[endfor]")
compiler.compile(toks3)
local flow3 = toks3._compiled.flow
check("while skip_body_to compiled", flow3[1] and flow3[1].kind == "while"
      and flow3[1].skip_body_to == 6)
check("break loop_end compiled", flow3[4] and flow3[4].kind == "break"
      and flow3[4].loop_end == 6)
check("for skip_body_to compiled", flow3[7] and flow3[7].kind == "for"
      and flow3[7].skip_body_to == 9)

-- ---------------------------------------------------------------------------
-- 4. switch case table: depth-aware, O(1) by value
-- ---------------------------------------------------------------------------
local toks4 = tokenizer.parse(
    "[switch mode]\n[case fast]\n[ch text=\"F\"]\n[switch mode]\n"
    .. "[case slow]\n[ch text=\"NESTED\"]\n[endswitch]\n[default]\n"
    .. "[ch text=\"D\"]\n[endswitch]")
compiler.compile(toks4)
local f4 = toks4._compiled.flow[1]
check("switch case table built", f4.kind == "switch" and f4.cases ~= nil)
check("depth-1 case mapped", f4.cases.fast == 3)
check("nested case NOT in outer table", f4.cases.slow == nil)
check("default mapped", f4.default == 9)
check("endswitch index", f4.endswitch == 10)

-- ---------------------------------------------------------------------------
-- 5. handler bindings resolved at compile time
-- ---------------------------------------------------------------------------
local saved_kag = package.loaded["kag"]
package.loaded["kag"] = { ch = function() end, wait = function() end }
local toks5 = tokenizer.parse("[ch text=\"a\"][wait 100]")
compiler.compile(toks5)
local h = toks5._compiled.handlers
check("handler bound for ch", h[1] ~= nil)
check("handler bound for wait", h[2] ~= nil)
package.loaded["kag"] = saved_kag

-- ---------------------------------------------------------------------------
-- 6. scheduler fast path: compiled stream executes identically
-- ---------------------------------------------------------------------------
local scheduler = require("scheduler")
-- NOTE: the mock kag table must be installed BEFORE compile so handler
-- bindings capture the mock (compile binds kag[cmd] at compile time).
local kag_orig = package.loaded["kag"]
local d = {}
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) d[#d + 1] = { k, p2 } end
end})
local toks6 = tokenizer.parse(
    "[if exp=\"f.a > 1 && f.b != 2\"]\n[ch text=\"taken\"]\n[else]\n"
    .. "[ch text=\"not-taken\"]\n[endif]\n*end\n[stop]")
compiler.compile(toks6)
local ctx6 = { f = { a = 5, b = 1 }, tf = {}, sf = {}, mp = {},
    variables = {}, current_scene = "t.ks", token_index = 1,
    label_index = toks6._compiled.labels }
local co = coroutine.create(function() scheduler.run(ctx6, toks6, 1) end)
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
check("compiled stream dispatches if-taken", d[1] and d[1][2].text == "taken")

-- same scene with the OTHER branch taken (env differs, expr cache keyed by
-- env identity -- compiled source must not leak stale values)
-- NOTE: install a FRESH mock before compile -- handler bindings capture
-- the kag table present at compile time (compile binds kag[cmd] once).
local d2 = {}
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) d2[#d2 + 1] = { k, p2 } end
end})
local toks6b = tokenizer.parse(
    "[if exp=\"f.a > 1 && f.b != 2\"]\n[ch text=\"taken\"]\n[else]\n"
    .. "[ch text=\"not-taken\"]\n[endif]\n*end\n[stop]")
compiler.compile(toks6b)
local ctx6b = { f = { a = 0, b = 2 }, tf = {}, sf = {}, mp = {},
    variables = {}, current_scene = "t.ks", token_index = 1,
    label_index = toks6b._compiled.labels }
local co2 = coroutine.create(function() scheduler.run(ctx6b, toks6b, 1) end)
while coroutine.status(co2) ~= "dead" do coroutine.resume(co2) end
check("compiled stream dispatches else branch (env-identity cache)",
      d2[1] and d2[1][2].text == "not-taken")
package.loaded["kag"] = kag_orig

-- ---------------------------------------------------------------------------
-- 7. record-format input (tokenizer.parse output) compiles too
-- ---------------------------------------------------------------------------
local toks7 = tokenizer.parse("[ch text=\"plain\"]")
-- tokenizer returns record format; compiler accepts both
compiler.compile(toks7)
check("record-format stream compiled", toks7._compiled ~= nil)

if failed > 0 then
    print(string.format("COMPILER TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("COMPILER TESTS DONE (%d passed)", passed))
