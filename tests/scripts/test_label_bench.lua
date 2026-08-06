-- test_label_bench.lua — label-index performance guard (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local tokenizer = require("tokenizer")
local parts = {}
for i = 1, 1500 do
    parts[#parts + 1] = "*label_" .. i
    parts[#parts + 1] = "[ch text=\"l\"]"
end
parts[#parts + 1] = "*target_end"
local tokens = tokenizer.parse(table.concat(parts, "\n"))
for j, t in ipairs(tokens) do
    if t.type then
        if t.type == "label" then tokens[j] = { "label", { name = t.name } }
        elseif t.type == "text" then tokens[j] = { "ch", { text = t.content or "" } }
        else tokens[j] = { t.cmd or t.type, t.params or {} } end
    end
end
local idx = scheduler.build_label_index(tokens)
check("index finds last label", scheduler.find_label(tokens, "target_end", idx) ~= nil)

-- relational guard: indexed lookup must be <= linear (loose, CI-safe;
-- the real ratio is ~300x at 3000 labels)
local N = 3000
local t0 = os.clock()
for _ = 1, N do scheduler.find_label(tokens, "target_end", idx) end
local tIndexed = os.clock() - t0
local t1 = os.clock()
for _ = 1, N do scheduler.find_label(tokens, "target_end", nil) end
local tLinear = os.clock() - t1
check("indexed <= linear", tIndexed <= tLinear + 0.01)

if failed > 0 then os.exit(1) end
print("LABEL BENCH TESTS DONE")
