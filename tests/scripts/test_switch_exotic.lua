-- test_switch_exotic.lua — unmatched-outer + nested-switch shape (security info)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local tokens = {
    -- outer switch MATCHES case fast; its body holds an inner switch that
    -- MATCHES case fast, whose body holds a nested switch that does NOT
    -- match (mode=fast ~= x) -- the exact shape from the security info:
    -- a skipped nested switch must pop ITS OWN entry, not the outer's.
    { "switch", { "mode" } },
    { "case", { "fast" } },
    { "ch", { name = "A", text = "outer-fast" } },
    { "switch", { "mode" } },
    { "case", { "fast" } },
    { "ch", { name = "B", text = "inner-fast" } },
    { "switch", { "mode" } },
    { "case", { "x" } },
    { "ch", { name = "C", text = "nested-x" } },
    { "endswitch" },
    { "ch", { name = "D", text = "inner-after-nested" } },
    { "endswitch" },
    { "ch", { name = "E", text = "outer-after-inner" } },
    { "case", { "slow" } },
    { "ch", { name = "F", text = "outer-slow" } },
    { "endswitch" },
    { "ch", { name = "G", text = "final" } },
}
local d = {}
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) d[#d + 1] = { k, p2 } end
end})
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = { mode = "fast" },
    macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
package.loaded["kag"] = kag_orig

check("outer fast runs", d[1] and d[1][2].text == "outer-fast")
check("inner fast runs", d[2] and d[2][2].text == "inner-fast")
check("nested x skipped", (function()
    for _, x in ipairs(d) do if x[2].text == "nested-x" then return false end end
    return true end)())
check("inner body continues", d[3] and d[3][2].text == "inner-after-nested")
check("outer body continues", d[4] and d[4][2].text == "outer-after-inner")
check("outer slow skipped", (function()
    for _, x in ipairs(d) do if x[2].text == "outer-slow" then return false end end
    return true end)())
check("final runs", d[5] and d[5][2].text == "final")

if failed > 0 then os.exit(1) end
print("SWITCH EXOTIC TESTS DONE")
