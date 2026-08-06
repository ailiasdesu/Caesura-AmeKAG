-- test_unlock_bare.lua — [unlock] bare-id consumption (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local tokenizer = require("tokenizer")
local toks = tokenizer.parse("[unlock cg1]")
check("unlock bare parsed", toks[1].cmd == "unlock" and toks[1].params[1][2] == "cg1")

-- end-to-end: [unlock cg1] records the CG
local scheduler = require("scheduler")
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = { { "ch", { text = "hi" } } }, token_index = 1,
    current_scene = "t.ks", label_index = {} }
local co = coroutine.create(function() scheduler.run(ctx, toks, 1) end)
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
check("unlock recorded", ctx.unlockedCG and ctx.unlockedCG.cg1 == true)

-- [unlock music 2] -- bare kind? no: kind stays cg (named type only)
local toks2 = tokenizer.parse("[unlock m2]")
local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = { { "ch", { text = "hi" } } }, token_index = 1,
    current_scene = "t.ks", label_index = {} }
local co2 = coroutine.create(function() scheduler.run(ctx2, toks2, 1) end)
while coroutine.status(co2) ~= "dead" do coroutine.resume(co2) end
check("unlock default cg", ctx2.unlockedCG and ctx2.unlockedCG.m2 == true)

-- direct caller with a pair-table params[1] must not store a table key
-- (security low nit: string-guard consistency with jump/call/link).
-- unlockedCG == nil discriminates: the guard returns BEFORE creating
-- the table, the buggy code created it and stored a table key.
local KAG = require("kag")
check("unlock handler exists", type(KAG.unlock) == "function")
local ctx3 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.unlock, ctx3, { { "cg9" } })
check("pair-table id rejected", ctx3.unlockedCG == nil)

-- named id still wins over bare
local ctx4 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.unlock, ctx4, { { "ignored" }, id = "real1" })
check("named id wins", ctx4.unlockedCG and ctx4.unlockedCG.real1 == true
      and ctx4.unlockedCG.ignored == nil)

if failed > 0 then os.exit(1) end
print("UNLOCK BARE TESTS DONE")
