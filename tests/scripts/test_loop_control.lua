-- test_loop_control.lua — [break]/[continue] (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local function run(tokens, f0)
    local d = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d[#d + 1] = { k, p2 } end
    end})
    local vars = f0 or {}
    local ctx = { f = vars, tf = {}, sf = {}, mp = {}, variables = {},
        _whileIterByScene = { ["t.ks"] = 0 },
        macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    local ok, err = true
    while coroutine.status(co) ~= "dead" do
        ok, err = coroutine.resume(co)
        if not ok then break end
    end
    package.loaded["kag"] = kag_orig
    return d, ok, err, ctx
end

-- break: while loop with an explicit early exit
local t1 = {
    { "for", { var = "i", start = "0", ["end"] = "9", step = "1" } },
    { "if", { exp = "i == 2" } },
    { "break" },
    { "endif" },
    { "ch", { name = "A", text = "body" } },
    { "endfor" },
    { "ch", { name = "B", text = "after" } },
}
-- i==2 -> break: bodies for i=0,1 run (2), then after
local d1, ok1, _, ctx1 = run(t1)
check("break exits early", #d1 == 3 and d1[1][2].text == "body"
      and d1[3][2].text == "after")
check("break counter stops at 2", ctx1.f.i == 2)

-- continue: skip bodies for even i (i=0,2 skipped; 1,3 run)
local t2 = {
    { "for", { var = "j", start = "0", ["end"] = "3", step = "1" } },
    { "if", { exp = "j % 2 == 0" } },
    { "continue" },
    { "endif" },
    { "ch", { name = "A", text = "body" } },
    { "endfor" },
}
local d2, ok2, _, ctx2 = run(t2)
check("continue skips evens", #d2 == 2 and d2[1][2].text == "body")
check("continue counter completes", ctx2.f.j == 4)

-- continue inside while: n decrements via eval; continue skips the ch
-- eval BEFORE the check so a skipped iteration still decrements
-- (continue jumping past the decrement would spin forever -- the
-- author's bug, correctly surfaced by the loop guard).
local t3 = {
    { "while", { exp = "n > 0" } },
    { "eval", { exp = "f.n = f.n - 1" } },
    { "if", { exp = "n == 1" } },
    { "continue" },
    { "endif" },
    { "ch", { name = "A", text = "body" } },
    { "endwhile" },
}
-- n: 3->2 body(2), 2->1 continue, 1->0 body(0): 2 bodies, n=0
local d3, ok3, _, ctx3 = run(t3, { n = 3 })
check("while continue works", #d3 == 2)
check("while continue terminates", ctx3.f.n == 0)

-- break outside a loop errors loudly
local t4 = {
    { "break" },
}
local d4, ok4, err4 = run(t4)
check("break outside errors", ok4 == false)
check("break error message", type(err4) == "string"
      and err4:find("outside a loop", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("LOOP CONTROL TESTS DONE")
