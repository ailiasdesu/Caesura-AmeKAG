-- test_waitclick.lua — [waitclick] + KAG.wait_click (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")

-- [waitclick] blocks until the click clears waiting_input
local scheduler = require("scheduler")
local tokens = {
    { "waitclick" },
    { "ch", { name = "A", text = "after" } },
}
local dispatched = {}
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = KAG
-- wrap ch to record (waitclick handled inline by KAG)
local real_ch = KAG.ch
KAG.ch = function(ctx, params) dispatched[#dispatched + 1] = params.text end
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 },
    macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
-- first resume: waitclick sets waiting_input and yields
coroutine.resume(co)
check("waitclick blocks scheduler", ctx.waiting_input == true)
check("no advance while blocked", #dispatched == 0)
-- simulate a click: clear the flag and resume
ctx.waiting_input = false
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
check("click resumes past waitclick", #dispatched == 1
      and dispatched[1] == "after")

-- KAG.wait_click() outside a coroutine errors loudly
local ok, err = pcall(KAG.wait_click)
check("wait_click outside coroutine errors", ok == false)
check("wait_click error message", type(err) == "string"
      and err:find("outside a coroutine", 1, true) ~= nil)

-- KAG.wait_click() inside a coroutine suspends until the flag clears
package.loaded["kag"] = kag_orig
local called = false
-- set the ctx BEFORE the coroutine starts so the flag path is real
rawset(_G, "_CAESURA_CTX", ctx)
ctx.waiting_input = false
local co2 = coroutine.create(function()
    local r = KAG.wait_click()
    called = (r == true)
end)
coroutine.resume(co2)
check("wait_click suspends", coroutine.status(co2) == "suspended")
check("wait_click set flag", ctx.waiting_input == true)
ctx.waiting_input = false
coroutine.resume(co2)
check("wait_click returns after resume", called)
rawset(_G, "_CAESURA_CTX", nil)

if failed > 0 then os.exit(1) end
print("WAITCLICK TESTS DONE")
