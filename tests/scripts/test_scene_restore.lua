-- test_scene_restore.lua — [call]/[return] scene-name restore (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local caller = {
    { "ch", { name = "C", text = "caller-start" } },
    { "call", { target = "sub.ks" } },
    { "ch", { name = "C2", text = "caller-after" } },
}
local callee = {
    { "ch", { name = "D", text = "callee-line" } },
    { "return" },
}
local loaded = { ["assets/script/sub.ks"] = callee }
local dispatched = {}
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) dispatched[#dispatched + 1] = { k, p2 } end
end})
local ctx = { macros = nil, macro_args = nil, f = {}, tf = {}, sf = {}, mp = {},
    current_scene = "main.ks", token_index = 1, label_index = {},
    load_tokens = function(p) return loaded[p] end }
local co = coroutine.create(function() scheduler.run(ctx, caller, 1) end)
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
package.loaded["kag"] = kag_orig

check("callee dispatched", dispatched[2] and dispatched[2][2].text == "callee-line")
check("caller continues after return", dispatched[3] and dispatched[3][2].text == "caller-after")
check("scene name restored", ctx.current_scene == "main.ks")
check("call stack empty", not ctx.call_stack or #ctx.call_stack == 0)

if failed > 0 then os.exit(1) end
print("SCENE RESTORE TESTS DONE")
