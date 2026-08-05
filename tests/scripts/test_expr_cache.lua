-- test_expr_cache.lua — [if] expression cache (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}  -- file scope: runner shares globals
local function check(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    results[#results + 1] = cond
end

local scheduler = require("scheduler")
local src = io.open("scripts/scheduler.lua", "r")
local s = src and src:read("*a") or ""
if src then src:close() end

check("cache half-evicts", s:find("math.floor(#keys / 2)", 1, true) ~= nil)
check("no full clear in evict", not s:find("exprCache = {}%s*$", 1))
check("error report has location", s:find('ctx.current_scene or "?"', 1, true) ~= nil)
check("error report has token", s:find("ctx.token_index or 0", 1, true) ~= nil)

-- behavioral: if evaluation against a stable ctx.f table
do
    local tokens = {
        { "if", { exp = "score > 5" } },  -- env IS ctx.f: fields are direct
        { "ch", { name = "A", text = "high" } },
        { "endif" },
        { "ch", { name = "B", text = "after" } },
    }
    local dispatched = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) dispatched[#dispatched + 1] = { k, p2 } end
    end})
    local ctx = { f = { score = 9 }, tf = {}, sf = {}, mp = {},
        macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    check("if true dispatches inside", dispatched[1] and dispatched[1][2].text == "high")
    check("if true continues after", dispatched[2] and dispatched[2][2].text == "after")

    -- ctx.f content mutation is visible to cached closures (same table ref)
    local ctx2 = { f = { score = 2 }, tf = {}, sf = {}, mp = {},
        macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
    local d2 = {}
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d2[#d2 + 1] = { k, p2 } end
    end})
    local co2 = coroutine.create(function() scheduler.run(ctx2, tokens, 1) end)
    while coroutine.status(co2) ~= "dead" do coroutine.resume(co2) end
    package.loaded["kag"] = kag_orig
    check("if false skips inside", d2[1] and d2[1][2].text == "after")
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("EXPR CACHE TESTS DONE")
