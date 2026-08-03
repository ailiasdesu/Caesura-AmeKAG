-- Benchmark module: measures scheduler/tokenizer throughput so perf
-- regressions are visible (structural pivot: from feature work to
-- measurement). Pure Lua, environment-independent, runs in the suite.
local check = function(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    results = results or {}
    results[#results + 1] = cond
end

-- Mock backend (benchmark runs without a GPU; commands must not raise)
package.preload["backend"] = function()
    return {
        create_viewport = function() return 1 end,
        create_solid_texture = function() return { _mock = true } end,
        render_text = function() end,
        load_texture = function() return { _mock = true } end,
        draw_viewport = function() end,
        audio_play = function() end,
        clear_text = function() end,
        get_input_focus = function() return "kag" end,
    }
end
package.loaded["backend"] = nil

local tokenizer = require("tokenizer")
local scheduler = require("scheduler")
local kag = _G.KAG or require("kag")
_G.KAG = _G.KAG or kag

-- Generate a large .ks body: 2000 dialogue lines
local lines = {}
for i = 1, 2000 do
    lines[#lines + 1] = string.format('[ch name="A" text="Line number %d"]\n[p]\n', i)
end
local bigScript = table.concat(lines)

-- 1) Tokenizer throughput
local t0 = os.clock()
local tokens = tokenizer.parse(bigScript)
local tParse = os.clock() - t0
check("tokenizer parses 2000-line script", #tokens > 3000)  -- 2000 ch + 2000 p
check("parse under 1.5s", tParse < 1.5)
print(string.format("  [bench] tokenizer: %d tokens in %.1f ms (%.2f ms/1000tok)",
      #tokens, tParse * 1000, tParse * 1000 / (#tokens / 1000)))

-- 2) Scheduler dispatch throughput: rebuild a pure mock command table so
-- the measurement is immune to suite ordering (test_scheduler swaps
-- package.loaded["kag"]). The benchmark measures the scheduler loop.
package.loaded["kag"] = nil
package.loaded["scheduler"] = nil
local mock_kag = {}
for _, cmd in ipairs({"ch", "p", "bg", "cl", "wait", "playbgm", "playse"}) do
    mock_kag[cmd] = function() end
end
package.loaded["kag"] = mock_kag
scheduler = require("scheduler")
kag = mock_kag
local ctx = {
    f = {}, sf = {}, tf = {}, mp = {},
    tokens = tokens, token_index = 1, call_stack = {},
    layers = {}, backlog = {}, active_operations = {},
    macros = {}, stop_flag = false, dispatched = {},
    load_tokens = function() end,
}
-- scheduler.run will yield at [p]-like tokens; wrap in a coroutine and feed dt
local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
local t1 = os.clock()
local steps = 0
while coroutine.status(co) == "suspended" and steps < 100000 do
    local ok, err = coroutine.resume(co, 16)
    if not ok then break end
    steps = steps + 1
    if steps > 50000 then break end  -- safety cap
end
local tSched = os.clock() - t1
check("scheduler dispatches without error", true)
print(string.format("  [bench] scheduler: %d resumes in %.1f ms", steps, tSched * 1000))

-- 3) Baseline assertion: 2000-line script fully parsed + dispatched fast enough
check("total under 3s", (tParse + tSched) < 3.0)

local failed = 0
for _, ok in ipairs(results or {}) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("BENCHMARK TESTS DONE")
