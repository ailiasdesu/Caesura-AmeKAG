-- test_bareval.lua — KAG3 bare positional args (tokenizer upgrade)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local tokenizer = require("tokenizer")

-- parse level: bare value becomes params[1]
local toks = tokenizer.parse("[delay 500]")
local p = toks[1].params
check("bare delay parsed", toks[1].cmd == "delay" and #p == 1)
check("bare value captured", p[1][1] == "1" and p[1][2] == "500")

-- ident=value untouched
local toks2 = tokenizer.parse("[wait ms=300]")
check("named param kept", toks2[1].params[1][1] == "ms"
      and toks2[1].params[1][2] == "300")

-- mixed bare + named
local toks3 = tokenizer.parse("[gallery 2]")
check("bare gallery", toks3[1].cmd == "gallery" and toks3[1].params[1][2] == "2")

-- END-TO-END: [delay 500] blocks ~500ms through the scheduler
local scheduler = require("scheduler")
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 }, current_scene = "t.ks", token_index = 1 }
local co = coroutine.create(function() scheduler.run(ctx, toks, 1) end)
local r1 = coroutine.resume(co)
local r2 = coroutine.resume(co, 100)
check("still waiting at 100ms", r1 and r2 and coroutine.status(co) == "suspended")
local r3 = coroutine.resume(co, 450)
-- wait exits at 550ms; the scheduler loop tail yields once more per
-- token, so one more resume finishes the coroutine
local r4 = coroutine.resume(co)
check("done at 550ms", r3 and r4 and coroutine.status(co) == "dead")

-- [wait 200] (bare positional) works too
local toks4 = tokenizer.parse("[wait 200]")
local ctx4 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 }, current_scene = "t.ks", token_index = 1 }
local co4 = coroutine.create(function() scheduler.run(ctx4, toks4, 1) end)
local w1 = coroutine.resume(co4)
local w2 = coroutine.resume(co4, 250)
local w3 = coroutine.resume(co4)
check("wait bare done", w1 and w2 and w3 and coroutine.status(co4) == "dead")

-- NAMED [delay ms=N] still wins through the real parse+scheduler path
-- (review blocking regression: the bare-value wrapper clobbered ms)
local toksN = tokenizer.parse("[delay ms=500]")
local ctxN = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 }, current_scene = "t.ks", token_index = 1 }
local coN = coroutine.create(function() scheduler.run(ctxN, toksN, 1) end)
local n1 = coroutine.resume(coN)
local n2 = coroutine.resume(coN, 100)
check("named ms waiting at 100ms", n1 and n2 and coroutine.status(coN) == "suspended")
local n3 = coroutine.resume(coN, 450)
local n4 = coroutine.resume(coN)
check("named ms done at 550ms", n3 and n4 and coroutine.status(coN) == "dead")

if failed > 0 then os.exit(1) end
print("BAREVAL TESTS DONE")
