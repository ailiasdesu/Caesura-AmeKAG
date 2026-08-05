-- test_for.lua — numeric [for] loops (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local function runFor(tokens, f0, pre_iter)
    local d = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d[#d + 1] = { k, p2 } end
    end})
    local vars = f0 or {}
    local ctx = { f = vars, tf = {}, sf = {}, mp = {}, variables = {},
        _whileIterByScene = { ["t.ks"] = pre_iter or 0 },
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

-- basic: i 0..2 (3 iterations), counter visible in f, after runs
local t1 = {
    { "for", { var = "i", start = "0", ["end"] = "2", step = "1" } },
    { "ch", { name = "A", text = "body" } },
    { "endfor" },
    { "ch", { name = "B", text = "after" } },
}
local d1, ok1, _, ctx1 = runFor(t1)
check("for runs 3 times",
      #d1 == 4 and d1[1][2].text == "body" and d1[4][2].text == "after")
check("counter ends past end", ctx1.f.i == 3)
check("after runs", d1[#d1] and d1[#d1][2].text == "after")

-- descending: i 3..1 step -1 (3 iterations)
local t2 = {
    { "for", { var = "j", start = "3", ["end"] = "1", step = "-1" } },
    { "ch", { name = "A", text = "body" } },
    { "endfor" },
}
local d2, ok2, _, ctx2 = runFor(t2)
check("descending runs 3 times", #d2 == 3)
check("descending counter", ctx2.f.j == 0)

-- zero iterations: start past end (step +1)
local t3 = {
    { "for", { var = "k", start = "5", ["end"] = "2", step = "1" } },
    { "ch", { name = "A", text = "body" } },
    { "endfor" },
    { "ch", { name = "B", text = "after" } },
}
local d3, ok3 = runFor(t3)
check("empty for skips body", #d3 == 1 and d3[1][2].text == "after")

-- guard: step=0 degenerates to 1 (no hang); pre-seed near cap fires
local t4 = {
    { "for", { var = "z", start = "0", ["end"] = "999", step = "0" } },
    { "ch", { name = "A", text = "body" } },
    { "endfor" },
}
local d4, ok4, err4 = runFor(t4, {}, 65536)
check("for guard errors loudly", ok4 == false)
check("guard message mentions iterations",
      type(err4) == "string" and err4:find("iterations", 1, true) ~= nil)

-- nested for: 2x2 with independent counters
local t5 = {
    { "for", { var = "a", start = "0", ["end"] = "1", step = "1" } },
    { "for", { var = "b", start = "0", ["end"] = "1", step = "1" } },
    { "ch", { name = "A", text = "body" } },
    { "endfor" },
    { "endfor" },
    { "ch", { name = "B", text = "after" } },
}
local d5, ok5, _, ctx5 = runFor(t5)
check("nested for runs 4 times", #d5 == 5)  -- 4 bodies + after
check("nested counters independent", ctx5.f.a == 2 and ctx5.f.b == 2)
check("nested after runs", d5[#d5] and d5[#d5][2].text == "after")


-- Sequential same-name for after an EMPTY for: the mark must be cleared
-- (review should-fix) or the second loop reuses the stale counter.
do
    local scheduler = require("scheduler")
    local tokens = {
        { "for", { var = "i", start = "9", ["end"] = "1", step = "1" } },  -- empty
        { "ch", { name = "A", text = "skip" } },
        { "endfor" },
        { "for", { var = "i", start = "0", ["end"] = "1", step = "1" } },  -- 2 iters
        { "ch", { name = "B", text = "body" } },
        { "endfor" },
    }
    local d = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d[#d + 1] = { k, p2 } end
    end})
    local vars = {}
    local ctx = { f = vars, tf = {}, sf = {}, mp = {}, variables = {},
        _whileIterByScene = { ["t.ks"] = 0 },
        macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    local ok = true
    while coroutine.status(co) ~= "dead" do ok = coroutine.resume(co) end
    package.loaded["kag"] = kag_orig

    local bodyc = 0
    for _, x in ipairs(d) do if x[2].text == "body" then bodyc = bodyc + 1 end end
    local skipc = 0
    for _, x in ipairs(d) do if x[2].text == "skip" then skipc = skipc + 1 end end
    local okall = ok and bodyc == 2 and skipc == 0 and vars.i == 2
    if okall then print("PASS empty-for mark cleared; second for runs 2x")
        passed = passed + 1
    else print("FAIL empty-for mark cleared; second for runs 2x")
        failed = failed + 1 end
end

if failed > 0 then os.exit(1) end
print("FOR TESTS DONE")
