-- test_switch_scan.lua — depth-aware case scan (review note shape)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
-- mode="fast": the OUTER switch (switch fast) matches its own case fast;
-- the nested switch (inside outer's taken body) also matches case fast.
-- The review-note shape: an outer switch whose body holds a nested switch
-- with a matching case -- the scan must NOT land on the nested case.
local tokens = {
    { "switch", { "mode" } },
    { "case", { "slow" } },
    { "ch", { name = "A", text = "outer-slow" } },
    { "endswitch" },
    { "switch", { "mode" } },
    { "case", { "x" } },
    { "ch", { name = "B", text = "nested-x" } },
    { "endswitch" },
}
-- scan-only check via a no-match outer whose body holds a nested switch
-- with a MATCHING case: the outer must skip to its own endswitch, never
-- dispatching the nested case body.
local tokens2 = {
    { "switch", { "mode" } },      -- mode=slow: outer case fast NO match
    { "case", { "fast" } },
    { "switch", { "mode" } },      -- nested: case slow MATCHES
    { "case", { "slow" } },
    { "ch", { name = "C", text = "nested-slow" } },
    { "endswitch" },
    { "ch", { name = "D", text = "outer-body-after" } },
    { "endswitch" },
    { "ch", { name = "E", text = "after-all" } },
}
local d = {}
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) d[#d + 1] = { k, p2 } end
end})
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = { mode = "slow" },
    macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
local co = coroutine.create(function() scheduler.run(ctx, tokens2, 1) end)
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
package.loaded["kag"] = kag_orig

check("nested matching case NOT taken by outer scan",
    (function()
        for _, x in ipairs(d) do if x[2].text == "nested-slow" then return false end end
        return true end)())
check("outer body skipped (no match)", (function()
    for _, x in ipairs(d) do if x[2].text == "outer-body-after" then return false end end
    return true end)())
check("after-all runs", d[1] and d[1][2].text == "after-all")

if failed > 0 then os.exit(1) end
print("SWITCH SCAN TESTS DONE")
