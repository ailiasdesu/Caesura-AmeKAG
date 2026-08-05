-- test_if_nested.lua — nested if/elseif chains (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local function runWith(a, b)
    local tokens = {
        { "if", { exp = "a > 5" } },
        { "ch", { name = "A", text = "outer-true" } },
        { "if", { exp = "b > 5" } },
        { "ch", { name = "B", text = "inner-true" } },
        { "else" },
        { "ch", { name = "C", text = "inner-false" } },
        { "endif" },
        { "ch", { name = "D", text = "outer-after-inner" } },
        { "else" },
        { "ch", { name = "E", text = "outer-false" } },
        { "endif" },
        { "ch", { name = "F", text = "final" } },
    }
    local d = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d[#d + 1] = { k, p2 } end
    end})
    local ctx = { f = { a = a, b = b }, tf = {}, sf = {}, mp = {},
        macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    return d
end

-- outer true + inner true: outer-true, inner-true, outer-after-inner, final
local d = runWith(9, 9)
check("outer true + inner true seq",
      d[1] and d[1][2].text == "outer-true"
      and d[2] and d[2][2].text == "inner-true"
      and d[3] and d[3][2].text == "outer-after-inner"
      and d[4] and d[4][2].text == "final")
check("no double execution", #d == 4)
-- outer true + inner false: inner-false runs, outer else NOT
local d2 = runWith(9, 2)
check("inner false branch", d2[2] and d2[2][2].text == "inner-false")
check("outer else skipped", (function()
    for _, x in ipairs(d2) do if x[2].text == "outer-false" then return false end end
    return true end)())
-- outer false: inner chain entirely skipped, outer else runs once
local d3 = runWith(2, 9)
check("outer false -> else once", d3[1] and d3[1][2].text == "outer-false")
check("inner chain skipped", (function()
    for _, x in ipairs(d3) do
        if x[2].text == "inner-true" or x[2].text == "inner-false" then return false end
    end
    return true end)())
check("final after all", d3[2] and d3[2][2].text == "final")

if failed > 0 then os.exit(1) end
print("NESTED IF TESTS DONE")
