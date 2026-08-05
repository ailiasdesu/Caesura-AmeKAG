-- test_switch_taken_nested.lua — taken-case skip with nested switch (review nit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
-- mode=fast: outer case fast TAKEN. Its body holds a nested switch whose
-- case slow does NOT match (mode=fast) -- the nested switch skips its own
-- body. Then a LATER outer case (slow) must be skipped by the taken-case
-- skip -- and the skip must stop at the OUTER endswitch, not the nested
-- one (which was already consumed by the nested switch's own skip).
local tokens = {
    { "switch", { "mode" } },
    { "case", { "fast" } },
    { "ch", { name = "A", text = "outer-fast" } },
    { "switch", { "mode" } },
    { "case", { "slow" } },
    { "ch", { name = "B", text = "nested-slow" } },
    { "endswitch" },
    { "ch", { name = "C", text = "outer-after-nested" } },
    { "case", { "slow" } },
    { "ch", { name = "D", text = "outer-slow" } },
    { "endswitch" },
    { "ch", { name = "E", text = "final" } },
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
check("nested slow skipped", (function()
    for _, x in ipairs(d) do if x[2].text == "nested-slow" then return false end end
    return true end)())
check("outer body continues", d[2] and d[2][2].text == "outer-after-nested")
check("later outer case skipped", (function()
    for _, x in ipairs(d) do if x[2].text == "outer-slow" then return false end end
    return true end)())
check("final runs", d[3] and d[3][2].text == "final")

if failed > 0 then os.exit(1) end
print("SWITCH TAKEN NESTED TESTS DONE")
