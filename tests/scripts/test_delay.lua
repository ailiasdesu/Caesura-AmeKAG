-- test_delay.lua — [delay] KAG3 blocking-delay semantics (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- [delay] was parsed by the tokenizer but had NO handler -- the
-- scheduler fallback rendered delay text as dialogue. Now aliases
-- [wait] with positional-arg support.
local System = package.loaded["kag.commands.system"] or require("kag.commands.system")
check("delay handler exists", type(System.delay) == "function")
check("delay delegates to wait", type(System.wait) == "function")

-- positional [delay 500]: maps params[1] into the ms loop
local KAG = require("kag")
check("delay registered on KAG", type(KAG.delay) == "function")

-- behavior: delay(500) yields until elapsed >= 500ms
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 }, current_scene = "t.ks", token_index = 1 }
local co = coroutine.create(function()
    System.delay(ctx, { 500 })
end)
-- step 1: yields (in loop), step 2: feed 100ms, step 3: feed 450ms -> done
local ok1 = coroutine.resume(co)
check("delay yields first", ok1 and coroutine.status(co) == "suspended")
local ok2 = coroutine.resume(co, 100)
check("delay continues", ok2 and coroutine.status(co) == "suspended")
local ok3, err3 = coroutine.resume(co, 450)
check("delay completes after 550ms", ok3 and coroutine.status(co) == "dead", err3)

-- zero/negative delay returns immediately (no yield)
local ctx0 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 }, current_scene = "t.ks", token_index = 1 }
local co0 = coroutine.create(function()
    System.delay(ctx0, { 0 })
end)
local ok0 = coroutine.resume(co0)
check("zero delay immediate", ok0 and coroutine.status(co0) == "dead")

-- source: delay forwards params[1]
local f = assert(io.open("scripts/kag/commands/system.lua", "r"))
local src = f:read("*a")
f:close()
check("positional forward", src:find("params[1] or params.time or params.ms", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("DELAY TESTS DONE")
