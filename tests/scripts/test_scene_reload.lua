-- test_scene_reload.lua — KAG scene hot reload:
-- cache busting, position remapping (token/label/start fallbacks),
-- non-current scene cache-only invalidation, failure handling.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local flow = require("flow")
local kag_runner = require("kag_runner")

local TMP = "tests/scripts/_reload_tmp.ks"

local function write_ks(content)
    local f = assert(io.open(TMP, "w"))
    f:write(content)
    f:close()
end

-- ---- remap: same token (cmd + primary param) -----------------------------
do
    local old = {
        { "ch", { text = "one" } },
        { "ch", { text = "two" } },
        { "ch", { text = "three" } },
    }
    local new = {
        { "ch", { text = "zero" } },   -- inserted line
        { "ch", { text = "two" } },    -- the live token, shifted
        { "ch", { text = "three" } },
    }
    local idx = kag_runner.remap_token_index(old, 2, new)
    check("remap finds same token by content", idx == 2, tostring(idx))
end

-- ---- remap: label fallback ------------------------------------------------
do
    local old = {
        { "label", { name = "start" } },
        { "ch", { text = "a" } },
        { "label", { name = "garden" } },
        { "ch", { text = "b" } },
        { "ch", { text = "c" } },
    }
    local new = {
        { "ch", { text = "intro" } },      -- rewritten prologue
        { "label", { name = "garden" } },  -- label kept, shifted
        { "ch", { text = "b2" } },         -- text edited
    }
    -- live position = token 5 (ch c); nearest preceding label = garden
    local idx = kag_runner.remap_token_index(old, 5, new)
    check("remap falls back to nearest label", idx == 2, tostring(idx))
end

-- ---- remap: deleted label -> scene start ----------------------------------
do
    local old = {
        { "label", { name = "gone" } },
        { "ch", { text = "x" } },
    }
    local new = {
        { "ch", { text = "brand new" } },
    }
    local idx = kag_runner.remap_token_index(old, 2, new)
    check("remap falls back to scene start", idx == 1, tostring(idx))
end

-- ---- flow cache busting through reload_scene ------------------------------
do
    write_ks("[ch text=\"v1\"]\n")
    flow.reload_scene(TMP)  -- ensure fresh
    local s1 = flow.load_scene(TMP)
    -- load_scene now returns the COMPILED stream: array format
    -- {cmd, params} with a named params table (compile-time front-end).
    local p1 = s1.tokens[1][2]
    check("initial parse v1",
        p1 and p1.text == "v1",
        p1 and tostring(p1.text))
    write_ks("[ch text=\"v2\"]\n")
    local s2 = flow.reload_scene(TMP)
    local p2 = s2 and s2.tokens[1][2]
    check("reload_scene re-parses changed file",
        s2 and p2 and p2.text == "v2",
        p2 and tostring(p2.text))
end

-- ---- non-current scene: cache-only invalidation ---------------------------
do
    -- No kag_runner ctx in this suite: reload_scene returns no-context.
    -- Simulate the cache-only branch via flow directly (the runner branch
    -- is covered by the integration test in the engine/editor).
    write_ks("[ch text=\"v3\"]\n")
    flow.reload_scene(TMP)
    local ok, err = kag_runner.reload_scene(TMP)
    -- Without a runner ctx the call must fail gracefully, not raise.
    check("reload_scene without runner ctx fails gracefully",
        ok == false and (err == "no-context" or err ~= nil),
        tostring(ok) .. "/" .. tostring(err))
end

-- ---- missing file: graceful failure ---------------------------------------
do
    local ok, err = kag_runner.reload_scene("tests/scripts/__nope__.ks")
    check("reload_scene missing file fails gracefully",
        ok == false and err ~= nil, tostring(err))
end

os.remove(TMP)

-- Exercise the live-context path in isolation so scheduler/global state from
-- the shared Lua suite cannot mask cancellation or failure behavior.
do
    local old_tokens = { { "ch", { text = "before" } } }
    local new_tokens = { { "ch", { text = "after" } } }
    local replacement = { tokens = new_tokens, labels = {} }
    local cancellation_calls = 0
    local cancelled_before_swap = false
    local cleanup_calls = 0
    local scheduler_starts = 0
    local pending_requests = { "old-request" }
    local reload_calls = 0
    local attempt_running_reload = false
    local running_reload_result
    local runner
    local env = setmetatable({}, { __index = _G })
    env._G = env
    local dependencies = {
        flow = {
            load_scene = function() return { tokens = old_tokens, labels = {} } end,
            reload_scene = function()
                reload_calls = reload_calls + 1
                return replacement
            end,
        },
        scheduler = { run = function(c, tokens)
            scheduler_starts = scheduler_starts + 1
            if tokens == new_tokens then pending_requests[#pending_requests + 1] = "new-request" end
            local cleanup <close> = setmetatable({}, { __close = function()
                cleanup_calls = cleanup_calls + 1
                pending_requests[#pending_requests + 1] = "cleanup-request"
            end })
            if attempt_running_reload then
                running_reload_result = { runner.reload_scene("live.ks") }
            end
            c.waiting_input = true
            coroutine.yield()
        end },
        config = {},
        replay = { get_mode = function() return "off" end },
        backend = { cancel_async_loads = function()
            cancellation_calls = cancellation_calls + 1
            cancelled_before_swap = env._CAESURA_CTX.tokens == old_tokens
            pending_requests = {}
        end },
    }
    env.require = function(name)
        return assert(dependencies[name], "unexpected dependency: " .. name)
    end
    local source = assert(io.open("scripts/kag_runner.lua", "r"))
    local runner_src = source:read("*a")
    source:close()
    runner = assert(load(runner_src, "@scripts/kag_runner.lua", "t", env))()
    assert(runner.start("live.ks"))
    local live_ctx = runner.get_ctx()

    local cached, cache_status = runner.reload_scene("other.ks")
    check("non-current reload only refreshes cache", cached and cache_status == "cached")
    check("non-current reload preserves pending async work", cancellation_calls == 0)

    replacement = nil
    local accepted, failure = runner.reload_scene("live.ks")
    check("failed parse rejects current scene reload", not accepted and failure == "reload-failed")
    check("failed parse preserves pending async work", cancellation_calls == 0)
    check("failed parse keeps old scheduler suspended", cleanup_calls == 0)
    check("failed parse preserves live context", live_ctx.tokens == old_tokens and not live_ctx.stop_flag)

    replacement = { tokens = new_tokens, labels = {} }
    local reloaded, status = runner.reload_scene()
    check("current scene reload succeeds", reloaded and status == "reloaded")
    check("current scene cancels async work exactly once", cancellation_calls == 1)
    check("current scene cancels before replacing tokens", cancelled_before_swap)
    check("current scene stages replacement", live_ctx.tokens == new_tokens and live_ctx.stop_flag)
    check("current scene closes old scheduler before cancellation",
        cleanup_calls == 1 and #pending_requests == 0)
    local resumed = runner.update(0.016)
    check("scene reload starts new scheduler despite old input wait",
        resumed and scheduler_starts == 2 and not live_ctx._pendingSceneReload)
    check("scene reload does not cancel new scheduler work",
        cancellation_calls == 1 and #pending_requests == 1 and pending_requests[1] == "new-request")

    -- Each unsafe entry point gets a fresh suspended scheduler and context.
    for _, paused in ipairs({ "native", "kag", "running" }) do
        runner = assert(load(runner_src, "@scripts/kag_runner.lua", "t", env))()
        attempt_running_reload = paused == "running"
        local cancels_before = cancellation_calls
        local parses_before = reload_calls
        local closes_before = cleanup_calls
        assert(runner.start("live.ks"))
        local current = runner.get_ctx()
        local ok, reason
        if paused == "native" then
            runner.set_resume_adapter({
                is_paused = function() return true end,
                resume = function(_, co, value) return coroutine.resume(co, value) end,
            })
            ok, reason = runner.reload_scene("live.ks")
        elseif paused == "kag" then
            current._kag_debug_paused = true
            ok, reason = runner.reload_scene("live.ks")
        else
            ok, reason = table.unpack(running_reload_result)
        end
        check(paused .. " scheduler rejects direct scene reload", ok == false and reason ~= nil)
        check(paused .. " reload leaves old context and async work intact",
            cancellation_calls == cancels_before and reload_calls == parses_before
            and cleanup_calls == closes_before and current.tokens == old_tokens
            and not current._pendingSceneReload and not current.stop_flag)
    end
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("SCENE RELOAD TESTS DONE")
