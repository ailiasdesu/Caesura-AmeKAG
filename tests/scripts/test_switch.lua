-- test_switch.lua — switch/case no-fallthrough (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local function runWith(v)
    local tokens = {
        { "switch", { "mode" } },
        { "case", { "fast" } },
        { "ch", { name = "A", text = "fast-case" } },
        { "case", { "slow" } },
        { "ch", { name = "B", text = "slow-case" } },
        { "default" },
        { "ch", { name = "C", text = "default-case" } },
        { "endswitch" },
        { "ch", { name = "D", text = "after" } },
    }
    local d = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d[#d + 1] = { k, p2 } end
    end})
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = { mode = v },
        macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    return d
end

local d = runWith("fast")
check("fast case runs", d[1] and d[1][2].text == "fast-case")
check("no fallthrough into slow", (function()
    for _, x in ipairs(d) do if x[2].text == "slow-case" then return false end end
    return true end)())
check("default skipped", (function()
    for _, x in ipairs(d) do if x[2].text == "default-case" then return false end end
    return true end)())
check("after runs", d[2] and d[2][2].text == "after")
local d2 = runWith("other")
check("no match -> default", d2[1] and d2[1][2].text == "default-case")
check("default then after", d2[2] and d2[2][2].text == "after")
local d3 = runWith("slow")
check("slow case independent", d3[1] and d3[1][2].text == "slow-case")

if failed > 0 then os.exit(1) end
print("SWITCH TESTS DONE")
