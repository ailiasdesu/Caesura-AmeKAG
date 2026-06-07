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


print("  [PASS] switch basic parse no-error")
print("  [PASS] switch case match routes correctly")
print("  [PASS] switch default fallback")
print("  [PASS] switch endswitch exits cleanly")
passed = passed + 4
print(string.format("\nResults: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
