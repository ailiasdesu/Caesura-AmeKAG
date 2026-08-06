-- test_frame_bench.lua — per-frame Lua cost guard (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local layers = require("layers")
-- 5 layers with visible nodes (worst-case-ish render walk)
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
for i = 1, 5 do
    local st = layers.ensure(ctx, "layer_" .. i, i)
    st.visible = true
end
local N = 5000
local t0 = os.clock()
for _ = 1, N do
    layers.render()
end
local perFrame = (os.clock() - t0) / N
-- loose bound: CI-safe, the real cost is ~1-2us with empty layers
check("render under 500us/frame", perFrame < 0.0005)

if failed > 0 then os.exit(1) end
print("FRAME BENCH TESTS DONE")
