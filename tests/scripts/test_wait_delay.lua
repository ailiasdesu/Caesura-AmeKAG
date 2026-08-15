-- =============================================================================
--  test_wait_delay.lua — [wait]/[delay] scheduler edge-case tests (round 77)
--
--  Covers the SystemCommands.wait handler + the KAG.delay alias in the
--  REAL command path (not the test_scheduler kag_mock):
--    * ms=0 / negative ms return immediately (no operation, no yield)
--    * bare positional [wait 200] / [delay 500]
--    * [delay] is a KAG3 alias of [wait] (KAG.delay -> SystemCommands.wait)
--      and its explicit ms must WIN over the injected time=1000 default
--    * frame budget: a positive ms spans multiple coroutine yields
--    * large-ms clamping to the 60000 cap
--    * active_operations cleanup on completion and on cancellation
--    * operation-based interruption (the mechanism [jump]/[link] use via
--      Operation.cancel_all) ends a running wait early and cleans up
--
--  Isolated (orphan suite): drives handlers directly in coroutines and the
--  real scheduler with a minimal ctx — no kag_runner, no engine, headless.
-- =============================================================================
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local KAG = require("kag")
local S = require("kag.schema")
local scheduler = require("scheduler")
local operation = require("kag.operation")

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
        print(string.format("  [PASS] %s", name))
    else
        failed = failed + 1
        print(string.format("  [FAIL] %s  -- %s", name, detail or ""))
    end
end

print("\n=== Wait/Delay Scheduler Edge-Case Tests ===\n")

-- Minimal execution ctx (mirrors the scheduler flow-control fields).
local function mk()
    return {
        f = {}, sf = {}, tf = {}, mp = {},
        tokens = {}, token_index = 1, call_stack = {},
        layers = {}, backlog = {}, active_operations = {},
        macros = {}, stop_flag = false, dispatched = {},
        variables = {}, lf = {},
        current_scene = "t.ks", currentScene = "t.ks",
        load_tokens = function() end,
    }
end

-- Run a handler fn in a coroutine, resuming with dt every frame up to a cap.
-- Returns (frames, status, err).
local function run_frames(fn, ctx, frameDt, cap)
    local co = coroutine.create(fn)
    local frames = 0
    local err
    cap = cap or 200
    local fd = frameDt or 16
    while coroutine.status(co) ~= "dead" and frames < cap do
        frames = frames + 1
        local ok, res = coroutine.resume(co, fd)
        if not ok then err = res; break end
    end
    return frames, coroutine.status(co), err
end

-- ---------------------------------------------------------------------------
-- 1. [wait ms=0] returns immediately: no operation started, no yield.
-- ---------------------------------------------------------------------------
do
    local ctx = mk()
    local frames, status, err = run_frames(function()
        KAG.wait(ctx, S.coerce("wait", { ms = 0 }, ctx))
    end, nil, 16, 10)
    check("wait ms=0 returns immediately (1 frame, dead)", frames == 1 and status == "dead")
    check("wait ms=0 starts no operation", #ctx.active_operations == 0)
    check("wait ms=0 runs without error", err == nil)
end

-- ---------------------------------------------------------------------------
-- 2. Negative ms (bare positional [wait -5]) returns immediately.
--    Schema coerce rejects a negative NAMED ms (min=0) but a bare positional
--    bypasses coercion, so the handler's own <=0 guard must catch it.
-- ---------------------------------------------------------------------------
do
    local ctx = mk()
    local frames, status, err = run_frames(function()
        KAG.wait(ctx, { [1] = "-5" })  -- bare value, already a number
    end, nil, 16, 10)
    check("wait negative bare returns immediately", frames == 1 and status == "dead")
    check("wait negative starts no operation", #ctx.active_operations == 0)
    check("wait negative no error", err == nil)
end

-- ---------------------------------------------------------------------------
-- 3. Bare positional [wait 200] waits ~200ms over multiple frames.
-- ---------------------------------------------------------------------------
do
    local ctx = mk()
    local frames, status = run_frames(function()
        KAG.wait(ctx, { [1] = 200 })
    end, nil, 16, 50)
    -- 200ms / 16 = 12.5 -> 13 accumulating frames + final advance resume = 14
    check("wait bare 200 spans frames", frames >= 13 and frames <= 16, "frames " .. frames)
    check("wait bare 200 completes", status == "dead")
    check("wait bare 200 cleans operations", #ctx.active_operations == 0)
end

-- ---------------------------------------------------------------------------
-- 4. [delay] is a KAG3 alias of [wait], and an EXPLICIT ms beats the
--    injected time=1000 default.  (The [until]/[delay] schema contract sets
--    time's default; [delay ms=500] must NOT silently wait 1000ms.)
-- ---------------------------------------------------------------------------
do
    local ctx = mk()
    local p = S.coerce("delay", { ms = 500 }, ctx)
    check("delay coerce injects time default 1000", p.time == 1000)
    check("delay keeps explicit ms=500", p.ms == 500)
    local frames, status = run_frames(function()
        KAG.delay(ctx, p)  -- delegates to SystemCommands.wait
    end, nil, 16, 60)
    -- 500ms / 16 = 31.25 -> 32 accumulating frames + advance = 33
    check("delay ms=500 waits 500 not 1000 (frames ~33)", frames >= 31 and frames <= 36,
        "frames " .. frames)
    check("delay ms=500 completes", status == "dead")
    check("delay ms=500 cleans operations", #ctx.active_operations == 0)
end

-- ---------------------------------------------------------------------------
-- 5. [delay] bare positional [delay 500] also waits ~500ms (KAG.delay maps
--    params[1] -> ms when present).
-- ---------------------------------------------------------------------------
do
    local ctx = mk()
    local frames, status = run_frames(function()
        KAG.delay(ctx, S.coerce("delay", { [1] = "500" }, ctx))
    end, nil, 16, 60)
    check("delay bare 500 spans ~500ms", frames >= 31 and frames <= 36, "frames " .. frames)
    check("delay bare 500 completes", status == "dead")
end

-- ---------------------------------------------------------------------------
-- 6. Large ms is clamped to the 60000 cap even via a bare positional
--    (schema min/max are bypassed by bare values; the handler clamps).
--    frame budget at the cap is still finite, not the raw huge value.
-- ---------------------------------------------------------------------------
do
    local ctx = mk()
    local frames, status, err = run_frames(function()
        KAG.wait(ctx, { [1] = 999999 })
    end, nil, 16, 4000)
    -- clamped to 60000 -> 60000/16 = 3750 frames; our cap(4000) is enough
    -- to complete it but NOT enough for 999999/16 (62500 frames).
    check("wait huge clamp completes within cap", status == "dead", "frames " .. frames)
    check("wait huge clamp frame budget bounded", frames < 10000, "frames " .. frames)
end

-- ---------------------------------------------------------------------------
-- 7. Wait/delay alignment: [delay ms=N] and [wait ms=N] consume the SAME
--    frame budget (identical total ms).
-- ---------------------------------------------------------------------------
do
    local w = mk()
    local wf = run_frames(function() KAG.wait(w, S.coerce("wait", { ms = 160 }, w)) end, nil, 16, 30)
    local d = mk()
    local df = run_frames(function() KAG.delay(d, S.coerce("delay", { ms = 160 }, d)) end, nil, 16, 30)
    check("delay == wait frame budget", wf == df, "wait " .. wf .. " vs delay " .. df)
end

-- ---------------------------------------------------------------------------
-- 8. Operation-based interruption: cancelling the active operation (the
--    mechanism [jump]/[link] use via Operation.cancel_all) ends the wait
--    early and removes the operation.  The coroutine survives and the
--    scheduler continues to the next token.
-- ---------------------------------------------------------------------------
do
    local ctx = mk()
    local co = coroutine.create(function() scheduler.run(ctx, {
        { "wait", { ms = 10000 } },
        { "ch",   { text = "after" } },
    }, 1) end)
    local frames, waitingHit = 0, false
    while coroutine.status(co) ~= "dead" and frames < 50 do
        frames = frames + 1
        coroutine.resume(co, 16)
        if frames == 2 then operation.cancel_all(ctx) end
        if ctx.waiting_input then waitingHit = true end
    end
    check("wait cancelled early (frames < timeout budget)", frames < 40, "frames " .. frames)
    check("wait cancellation cleans operations", #ctx.active_operations == 0)
    check("wait cancellation lets scheduler continue to [ch]",
        waitingHit == true)
end

-- ---------------------------------------------------------------------------
-- 9. BehaVior aligned with [until] (round 87): a running [wait] obeys the
--    scheduler's flow controls.  Setting ctx.stop_flag (a scene abort) from
--    outside the wait handler's inner frame loop now ends the wait early —
--    the coroutine exits (dead), the operation is completed and cleaned up,
--    and a huge ms does not keep it parked.  (_next_index takes the same
--    path: the loop condition mirrors [until] exactly.)
-- ---------------------------------------------------------------------------
do
    local ctx = mk()
    local co = coroutine.create(function() scheduler.run(ctx, {
        { "wait", { ms = 2000 } },
        { "ch", { text = "after" } },
    }, 1) end)
    local frames = 0
    while coroutine.status(co) ~= "dead" and frames < 40 do
        frames = frames + 1
        if frames == 3 then ctx.stop_flag = true end
        coroutine.resume(co, 16)
    end
    -- 2000ms / 16 = 125 frames: stop_flag must unpark far earlier than the
    -- 40-frame cap.  The coroutine ends (dead, not suspended), proving the
    -- wait loop broke on stop_flag instead of parking until ms elapsed.
    check("stop_flag interrupts a running wait (early, dead)",
        coroutine.status(co) == "dead", "status " .. coroutine.status(co))
    check("stop_flag interrupt cleans active_operations",
        #ctx.active_operations == 0)
end

-- ---------------------------------------------------------------------------
-- 10. [wait] with named time= falls back when ms absent (schema default-
--     injected time), and a wait of a small named ms is exact.
-- ---------------------------------------------------------------------------
do
    local ctx = mk()
    local frames, status = run_frames(function()
        KAG.wait(ctx, S.coerce("wait", { time = 48 }, ctx))
    end, nil, 16, 10)
    check("wait time=48 spans ~3 frames", frames >= 3 and frames <= 6, "frames " .. frames)
    check("wait time=48 completes", status == "dead")
end

print(string.format("\nResults: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
print("WAIT_DELAY TESTS DONE")
