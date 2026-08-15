-- test_macro_nested.lua — nested macro expansion (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local tokens = {
    { "macro", { name = "inner", args = "who" } },
    { "ch", { name = "%who%", text = "inner-line" } },
    { "endmacro" },
    { "macro", { name = "outer", args = "who" } },
    { "inner", { who = "%who%" } },   -- nested call INSIDE the outer body
    { "ch", { name = "N", text = "outer-line" } },
    { "endmacro" },
    { "outer", { who = "Sakura" } },
}
local dispatched = {}
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) dispatched[#dispatched + 1] = { k, p2 } end
end})
local ctx = { macros = nil, macro_args = nil, f = {}, tf = {}, sf = {}, mp = {},
    current_scene = "t.ks", token_index = 1 }
local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
package.loaded["kag"] = kag_orig

check("nested macro dispatches inner", dispatched[1] and dispatched[1][2].name == "Sakura")
check("inner text", dispatched[1] and dispatched[1][2].text == "inner-line")
check("outer continues", dispatched[2] and dispatched[2][2].text == "outer-line")
check("inner dispatches as ch", dispatched[1] and dispatched[1][1] == "ch")

if failed > 0 then os.exit(1) end
-- expansion budget (audit): a self-recursive macro must fail fast
-- (not grow the token array until the C++ instruction budget)
local tokenizer2 = require("tokenizer")
local scheduler2 = require("scheduler")
local toksR = tokenizer2.parse("[macro m][m][endmacro][m]")
local ctxR = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = toksR, token_index = 1, current_scene = "t.ks",
    label_index = {}, _whileIterByScene = { ["t.ks"] = 0 } }
local coR = coroutine.create(function() scheduler2.run(ctxR, toksR, 1) end)
-- The scheduler yields between commands; run the coroutine to completion
-- (a single resume only executes up to the first yield and would miss the
-- budget error -- the self-recursive macro needs many expansion steps).
local okR, errR = true, nil
while coroutine.status(coR) ~= "dead" do
    local okStep, errStep = coroutine.resume(coR)
    if not okStep then
        okR, errR = false, errStep
        break
    end
end
-- Round 75: the expansion guard is depth-based now; the message says
-- "expansion depth exceeded" (the old per-context counter message was
-- "expansion budget exceeded").
check("recursive macro budgeted", not okR
      and tostring(errR):find("expansion depth") ~= nil)

print("NESTED MACRO TESTS DONE")
if failed > 0 then os.exit(1) end
