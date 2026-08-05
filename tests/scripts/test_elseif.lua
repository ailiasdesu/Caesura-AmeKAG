-- test_elseif.lua — [elseif] chain evaluation (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local function runWith(score)
    local tokens = {
        { "if", { exp = "score > 90" } },
        { "ch", { name = "A", text = "great" } },
        { "elseif", { exp = "score > 70" } },
        { "ch", { name = "B", text = "good" } },
        { "elseif", { exp = "score > 50" } },
        { "ch", { name = "C", text = "ok" } },
        { "else" },
        { "ch", { name = "D", text = "low" } },
        { "endif" },
        { "ch", { name = "E", text = "after" } },
    }
    local d = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d[#d + 1] = { k, p2 } end
    end})
    local ctx = { f = { score = score }, tf = {}, sf = {}, mp = {},
        macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    return d
end

local d95 = runWith(95)
check("score 95 -> great", d95[1] and d95[1][2].text == "great")
check("score 95 skips rest", d95[2] and d95[2][2].text == "after")
local d80 = runWith(80)
check("score 80 -> elseif good", d80[1] and d80[1][2].text == "good")
check("score 80 continues", d80[2] and d80[2][2].text == "after")
local d60 = runWith(60)
check("score 60 -> second elseif ok", d60[1] and d60[1][2].text == "ok")
local d30 = runWith(30)
check("score 30 -> else low", d30[1] and d30[1][2].text == "low")
check("all chains terminate", d30[2] and d30[2][2].text == "after")

if failed > 0 then os.exit(1) end
print("ELSEIF TESTS DONE")
