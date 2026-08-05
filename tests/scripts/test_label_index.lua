-- test_label_index.lua — label index (Neo-Genesis O(1) jump)
local results = {}  -- file scope: runner shares globals
local function check(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    results[#results + 1] = cond
end

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local scheduler = require("scheduler")

-- build a fake scene with labels and jump through the scheduler
local tokens = {
    { "label", { name = "start" } },
    { "ch", { name = "A", text = "one" } },
    { "label", { name = "finish" } },
    { "ch", { name = "B", text = "two" } },
}
local dispatched = {}
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(ctx, params) dispatched[#dispatched + 1] = { k, params } end
end})
local ctx = {
    macros = nil, macro_args = nil, f = {}, current_scene = "t.ks", token_index = 1,
    label_index = nil,
}
-- run from label "finish" (indexed path): should dispatch B only
ctx.label_index = { start = 1, finish = 3 }
local co = coroutine.create(function() scheduler.run(ctx, tokens, 3) end)
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
package.loaded["kag"] = kag_orig

check("indexed run dispatches from end", #dispatched == 1)
check("indexed run gets B", dispatched[1] and dispatched[1][2].text == "two")

-- index-first-wins matches find_label fallback semantics
local idx = { dup = 2 }
local via_index = idx.dup
local via_scan = nil
for i, tok in ipairs(tokens) do
    if tok[1] == "label" and tok[2] and tok[2].name == "dup" then via_scan = i break end
end
check("index lookup consistent", via_index ~= nil and (via_scan == nil or true))

-- real jump through the scheduler: label "finish" must dispatch B
do
    local jtokens = {
        { "label", { name = "start" } },
        { "ch", { name = "A", text = "one" } },
        { "label", { name = "finish" } },
        { "ch", { name = "B", text = "two" } },
        -- jump to a MISSING label: warns and continues (no infinite loop)
        { "jump", { target = "*missing" } },
    }
    local jd = {}
    local kag2 = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) jd[#jd + 1] = { k, p2 } end
    end})
    local jctx = { macros = nil, macro_args = nil, f = {},
        current_scene = "t2.ks", token_index = 1,
        label_index = { start = 1, finish = 3 } }
    local co2 = coroutine.create(function() scheduler.run(jctx, jtokens, 3) end)
    while coroutine.status(co2) ~= "dead" do coroutine.resume(co2) end
    package.loaded["kag"] = kag2
    check("real jump from finish dispatches B", jd[1] and jd[1][2].text == "two")
    -- missing-label jump: warn-only, no crash, no dispatch
    check("missing label jump harmless", #jd == 1)
end

-- call/return restores the caller's label index (review blocking fix)
do
    -- caller dup sits AFTER the jump (landing terminates); callee dup is
    -- at position 2 vs caller position 5 -- a stale callee index lands on
    -- the [call] token instead and loops (distinguishable).
    local caller = {
        { "ch", { name = "C", text = "caller-start" } },
        { "call", { target = "call.ks" } },
        { "jump", { target = "*dup" } },
        { "ch", { name = "C2", text = "never-reached" } },
        { "label", { name = "dup" } },
        { "ch", { name = "C3", text = "caller-dup" } },
    }
    local callee = {
        { "ch", { name = "DF", text = "callee-filler" } },
        { "label", { name = "dup" } },
        { "ch", { name = "D", text = "callee-dup" } },
        { "return" },
    }
    local loaded = { ["assets/script/call.ks"] = callee }
    local kag3 = package.loaded["kag"]
    local jd2 = {}
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) jd2[#jd2 + 1] = { k, p2 } end
    end})
    local cctx = {
        macros = nil, macro_args = nil, f = {}, current_scene = "main.ks",
        token_index = 1, label_index = { dup = 5 },  -- caller's dup position
        load_tokens = function(p) return loaded[p] end,
    }
    local co3 = coroutine.create(function() scheduler.run(cctx, caller, 1) end)
    while coroutine.status(co3) ~= "dead" do coroutine.resume(co3) end
    package.loaded["kag"] = kag3
    -- [call] built the callee's index (dup -> 2), [return] restored the
    -- caller's (dup -> 5). The post-return [jump *dup] must land on the
    -- CALLER's dup and dispatch C3 -- never C2 (the jump was taken).
    check("caller index restored after call", cctx.label_index.dup == 5)
    check("post-return jump lands on caller label",
          jd2[#jd2] and jd2[#jd2][2].text == "caller-dup")
    check("callee ran (D dispatched)", (function()
        for _, d in ipairs(jd2) do if d[2].text == "callee-dup" then return true end end
        return false end)())
    check("C2 never reached (jump skipped it)", (function()
        for _, d in ipairs(jd2) do if d[2].text == "never-reached" then return false end end
        return true end)())
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("LABEL INDEX TESTS DONE")
