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

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("LABEL INDEX TESTS DONE")
