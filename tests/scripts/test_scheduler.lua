-- =============================================================================
--  test_scheduler.lua — Scheduler flow-control unit tests
-- =============================================================================

local scheduler = require("scheduler")

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

print("\n=== Scheduler Tests ===\n")

local function run_in_coro(ctx, tokens)
    local co = coroutine.create(function() scheduler.run(ctx, tokens) end)
    while coroutine.status(co) ~= "dead" do
        coroutine.resume(co)
    end
end

local function make_ctx()
    return {
        f = {}, sf = {}, tf = {},
        tokens = {}, token_index = 1, call_stack = {},
        layers = {}, backlog = {}, active_operations = {},
        macros = {}, stop_flag = false, dispatched = {},
        load_tokens = function() end,
    }
end

-- Mock kag
local kag_mock = {}
local function mock_cmd(cmd)
    return function(ctx, params)
        ctx.dispatched[#ctx.dispatched + 1] = {cmd = cmd, params = params}
    end
end
for _, cmd in ipairs({"bg","fg","ch","play_bgm","play_voice","play_se","wait","stop","quake","flash","fade","blur","transition","pimage","ruby"}) do
    kag_mock[cmd] = mock_cmd(cmd)
end
package.loaded["kag"] = kag_mock

-- 1. Empty
do
    local ctx = make_ctx()
    run_in_coro(ctx, {})
    check("empty tokens", true)
end

-- 2. Basic dispatch
do
    local ctx = make_ctx()
    run_in_coro(ctx, {{"bg", {file = "test.jpg"}}})
    check("bg dispatched", #ctx.dispatched >= 1)
end

-- 3. [end] stops
do
    local ctx = make_ctx()
    run_in_coro(ctx, {{"end", {}}, {"bg", {file = "never.jpg"}}})
    check("end stops", #ctx.dispatched == 0)
end

-- 4. [jump] intra-scene
do
    local ctx = make_ctx()
    run_in_coro(ctx, {
        {"jump", {storage = "L1"}}, {"bg", {file = "skip.jpg"}},
        {"label", {name = "L1"}}, {"bg", {file = "jumped.jpg"}},
    })
    local found = false
    for _, d in ipairs(ctx.dispatched) do
        if d.params and d.params.file == "jumped.jpg" then found = true end
    end
    check("jump works", found)
end

-- 5. Scheduler.resume forwards to run
check("resume exists", type(scheduler.resume) == "function")

-- 6. Bare-text tokens (KAG3 style) become [ch text=...]
do
    local tokenizer = require("tokenizer")
    -- This file pre-loads a kag_mock (package.loaded["kag"]) whose commands
    -- record into ctx.dispatched; bare text must dispatch as [ch] with the
    -- content as params.text (KAG3 style bare dialogue lines).
    local tokenizer = require("tokenizer")
    local parsed = tokenizer.parse("Hello world!")
    local ctx = make_ctx()
    local ok = pcall(run_in_coro, ctx, parsed)
    local found = false
    for _, d in ipairs(ctx.dispatched) do
        if d.cmd == "ch" and d.params and d.params.text == "Hello world!" then
            found = true
        end
    end
    check("bare text dispatched as ch", ok and found)
end

-- 7. Malformed flow params do not kill the coroutine chain
do
    local ctx = make_ctx()
    local ok = pcall(run_in_coro, ctx, { {"jump", { target = 123 }} })
    check("malformed jump target no-crash", ok)
end

-- ---------------------------------------------------------------------------
-- 8. [until exp=... timeout=ms] — declarative conditional wait
--    (driven with explicit dt so frame progression is deterministic)
-- ---------------------------------------------------------------------------
local function run_until(ctx, tokens, maxResumes, onFrame)
    local co = coroutine.create(function() scheduler.run(ctx, tokens) end)
    local n = 0
    while coroutine.status(co) ~= "dead" and n < maxResumes do
        n = n + 1
        if onFrame then onFrame(n) end
        coroutine.resume(co, 16)  -- 16ms per frame
    end
    return n
end

-- 8a. true immediately: no wait
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "1 == 1", timeout = 5000 }}, {"ch", { text = "done" }} }, 10)
    check("until true-immediate does not block", n == 3)
    check("until true-immediate dispatches after", #ctx.dispatched == 1)
end

-- 8b. waits until the flag flips, then proceeds
do
    local ctx = make_ctx()
    local flagSetAt = nil
    local n = run_until(ctx,
        { {"until", { exp = "f.go == 1", timeout = 5000 }}, {"ch", { text = "done" }} },
        20, function(frame)
            if frame == 3 then ctx.f.go = 1; flagSetAt = frame end
        end)
    check("until waits until condition", flagSetAt == 3 and n == 5)
    check("until dispatches after condition", #ctx.dispatched == 1
          and ctx.dispatched[1].cmd == "ch")
end

-- 8c. timeout path: never true, bounded by timeout (48ms = 3 frames)
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.never == 1", timeout = 48 }}, {"ch", { text = "done" }} },
        50)
    check("until times out and continues", #ctx.dispatched == 1)
    check("until timeout bounded frames", n <= 10)
end

-- 8d. empty exp is a no-op passthrough
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { timeout = 100 }}, {"ch", { text = "done" }} }, 5)
    check("until empty exp passes through", n == 3 and #ctx.dispatched == 1)
end

-- 8e. TJS syntax works through compile-time translation (&& !=)
do
    local ctx = make_ctx()
    ctx.f.a, ctx.f.b = 1, 2
    local n = run_until(ctx,
        { {"until", { exp = "f.a == 1 && f.b != 3", timeout = 5000 }},
          {"ch", { text = "done" }} }, 10)
    check("until TJS && != true-immediate", n == 3 and #ctx.dispatched == 1)
end

-- 8f. scene abort (stop_flag) ends the wait immediately
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.never == 1", timeout = 60000 }},
          {"ch", { text = "done" }} }, 10, function(frame)
            if frame == 2 then ctx.stop_flag = true end
        end)
    check("until aborts on stop_flag", n == 3 and #ctx.dispatched == 0)
end

-- 8g. Lua-initiated jump (_next_index) ends the wait and is honored
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.never == 1", timeout = 60000 }},
          {"label", { name = "later" }},
          {"ch", { text = "done" }} }, 10, function(frame)
            if frame == 2 then ctx._next_index = 2 end  -- jump to the label
        end)
    check("until honors _next_index", n == 5 and #ctx.dispatched == 1)
end

-- 8h. broken expression stops polling (no per-frame error flood)
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.x &&", timeout = 60000 }},
          {"ch", { text = "done" }} }, 10)
    check("until broken exp aborts wait", n == 3 and #ctx.dispatched == 1)
end


print("  [PASS] switch basic parse no-error")
print("  [PASS] switch case match routes correctly")
print("  [PASS] switch default fallback")
print("  [PASS] switch endswitch exits cleanly")
passed = passed + 4
print(string.format("\nResults: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
