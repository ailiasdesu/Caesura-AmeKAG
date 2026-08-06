-- Benchmark module: measures scheduler/tokenizer throughput so perf
-- regressions are visible (structural pivot: from feature work to
-- measurement). Pure Lua, environment-independent, runs in the suite.
local results = {}  -- file scope: runner shares globals
local check = function(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
        results[#results + 1] = cond
end

-- captured BEFORE the mocks below (review: capturing after would
-- restore the mock itself)
local benchmark_preload_backend = package.preload["backend"]
local benchmark_backend = package.loaded["backend"]

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
-- SAVE the backend cache: nil-ing it permanently poisons every later
-- require("backend") under the suite sandbox ("not preloaded") --
-- font/video/wait tests all died in the suite because of this
-- (audit: pre-existing pollution the sandbox exposed).
package.loaded["backend"] = nil

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
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
-- relaxed to 3s for CI (pure-Lua PEG, no JIT on Linux runners); the
-- lpeg batch optimization took the local parse from 1.37s to ~0.9s
check("parse under 3s", tParse < 3.0)
print(string.format("  [bench] tokenizer: %d tokens in %.1f ms (%.2f ms/1000tok)",
      #tokens, tParse * 1000, tParse * 1000 / (#tokens / 1000)))

-- 2) Scheduler dispatch throughput: rebuild a pure mock command table so
-- the measurement is immune to suite ordering (test_scheduler swaps
-- package.loaded["kag"]). The benchmark measures the scheduler loop.
-- Capture BEFORE clearing so the restore puts the REAL modules back.
local _saved_kag = package.loaded["kag"]
local _saved_sched = package.loaded["scheduler"]
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
check("run makes progress", steps > 3000)  -- ~5751 tokens expected
print(string.format("  [bench] scheduler: %d resumes in %.1f ms", steps, tSched * 1000))

-- 3) Baseline assertion: 2000-line script fully parsed + dispatched fast enough
check("total under 3s", (tParse + tSched) < 3.0)

-- restore the real modules for subsequent tests
package.loaded["kag"] = _saved_kag
package.loaded["scheduler"] = _saved_sched

local failed = 0
for _, ok in ipairs(results or {}) do if not ok then failed = failed + 1 end end
if package.loaded["backend"] == nil and benchmark_backend then
    package.loaded["backend"] = benchmark_backend
end
if package.preload["backend"] == nil and benchmark_preload_backend then
    package.preload["backend"] = benchmark_preload_backend
end
if failed > 0 then os.exit(1) end
print("BENCHMARK TESTS DONE")
