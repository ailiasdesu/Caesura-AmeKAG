-- test_delay.lua — [delay] KAG3 blocking-delay semantics (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- [delay] is owned by kag.lua (KAG.delay -> wait); the REAL audit gap
-- was missing schema coercion: [delay ms=500] fed the STRING "500"
-- into wait's ms<=0 comparison -> runtime error (pcall'd silently).
-- Its own schema now coerces ms/time/duration to numbers.
local KAG = require("kag")
check("KAG.delay exists", type(KAG.delay) == "function")
check("delay delegates to wait",
      type(package.loaded["kag.commands.system"].wait) == "function")

-- schema coercion: [delay ms="500"] -> numeric 500 (no comparison error)
local schema = require("kag.schema")
local coerced = schema.coerce("delay", { ms = "500" })
check("ms coerced to number", type(coerced.ms) == "number" and coerced.ms == 500)

-- behavior: KAG.delay(500) yields until elapsed >= 500ms (direct path)
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 }, current_scene = "t.ks", token_index = 1 }
local co = coroutine.create(function()
    KAG.delay(ctx, { ms = 500 })
end)
local ok1 = coroutine.resume(co)
check("delay yields first", ok1 and coroutine.status(co) == "suspended")
local ok2 = coroutine.resume(co, 100)
check("delay continues", ok2 and coroutine.status(co) == "suspended")
local ok3 = coroutine.resume(co, 450)
check("delay completes after 550ms", ok3 and coroutine.status(co) == "dead")

-- zero delay returns immediately
local ctx0 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 }, current_scene = "t.ks", token_index = 1 }
local co0 = coroutine.create(function()
    KAG.delay(ctx0, { ms = 0 })
end)
local ok0 = coroutine.resume(co0)
check("zero delay immediate", ok0 and coroutine.status(co0) == "dead")

-- source: delay schema exists (ms coercion contract)
local f = assert(io.open("scripts/kag/commands/system.lua", "r"))
local src = f:read("*a")
f:close()
check("delay schema defined", src:find('define("delay"', 1, true) ~= nil)

-- REGRESSION (security LOW): an explicit alias must NOT be shadowed by
-- the injected time default. coerce([delay ms=500]) then wait must wait
-- 500ms, not 1000ms. End-to-end through the coerce + handler path.
local full = schema.coerce("delay", { ms = "500" })
local ctxS = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 }, current_scene = "t.ks", token_index = 1 }
local coS = coroutine.create(function()
    KAG.delay(ctxS, full)
end)
local o1 = coroutine.resume(coS)
local o2 = coroutine.resume(coS, 100)   -- 100 < 500: still waiting
local o3 = coroutine.resume(coS, 450)   -- 100+450 = 550 >= 500: done
check("coerced ms=500 waits 500 (no default shadow)",
      o1 and o2 and o3 and coroutine.status(coS) == "dead")

-- same guard for duration= (review warn: it was still shadowed)
local fullD = schema.coerce("delay", { duration = "2000" })
local ctxD = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 }, current_scene = "t.ks", token_index = 1 }
local coD = coroutine.create(function()
    KAG.delay(ctxD, fullD)
end)
local d1 = coroutine.resume(coD)
local d2 = coroutine.resume(coD, 1500)  -- 1500 < 2000: still waiting
local d3 = coroutine.resume(coD, 600)   -- 2100 >= 2000: done
check("coerced duration=2000 waits 2000", d1 and d2 and d3
      and coroutine.status(coD) == "dead")

if failed > 0 then os.exit(1) end
print("DELAY TESTS DONE")
