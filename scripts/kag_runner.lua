-- =============================================================================
--  Caesura (AmeKAG) — kag_runner.lua
--  KAG coroutine bridge for the engine update loop.
--
--  The scheduler (scheduler.lua) is coroutine-based — it yields after each
--  token (line 327: coroutine.yield()). Resuming the coroutine advances one
--  token. This module wraps that coroutine and provides start/update/on_click
--  hooks that the engine loop calls each frame.
--
--  Cross-scene jumps ([jump], [call], [link]) are handled by the scheduler
--  via ctx.load_tokens. When a cross-scene [jump] sets new tokens and
--  token_index, the scheduler returns (coroutine dies); we detect this via
--  a _scene_changed flag and re-spawn the coroutine on the next update.
-- =============================================================================

local flow = require("flow")
local scheduler = require("scheduler")

local kag_runner = {}
local kag_co = nil
local ctx = nil

-- ── Internal: load_tokens wrapper for cross-scene navigation ─────────────────
-- Called by scheduler.run() when [jump]/[call]/[link] target another .ks file.
-- flow.load_scene returns {tokens, labels, path}; we extract tokens, store
-- labels on ctx, and set _scene_changed so update() knows to re-spawn the coro.

local function load_tokens(path)
    local scene = flow.load_scene(path)
    if scene then
        ctx.labelMap = scene.labels
        ctx._scene_changed = true  -- signal update() to re-spawn coroutine
        return scene.tokens
    end
    print("[KAG Runner] Failed to load scene: " .. path)
    return nil
end

-- ── kag_runner.start(scene_path) → boolean ───────────────────────────────────
-- Load a .ks scene and begin executing it. Returns true on success.

function kag_runner.start(scene_path)
    -- Build execution context
    ctx = {
        f = {}, sf = {}, tf = {},
        tokens = {}, token_index = 1,
        call_stack = {}, layers = {}, backlog = {},
        active_operations = {}, stop_flag = false,
        variables = {},
        characters = {},
        unlockedCG = {}, unlockedMusic = {},
        -- [R7-FIX] Seen-flag tracking for Read Skip feature
        seen_scenes = {},
        waiting_input = false,
        _scene_changed = false,
        load_tokens = load_tokens,
    }
    rawset(_G, "_CAESURA_CTX", ctx)

    -- Load initial scene
    local scene = flow.load_scene(scene_path)
    if not scene then
        print("[KAG Runner] Failed to load scene: " .. scene_path)
        return false
    end
    ctx.tokens = scene.tokens
    ctx.labelMap = scene.labels
    ctx.current_scene = scene_path
    ctx.currentScene = scene_path  -- also set camelCase for text/save commands

    -- Start coroutine
    kag_co = coroutine.create(function()
        scheduler.run(ctx, ctx.tokens, 1)
    end)

    -- Advance past any non-blocking initial tokens (font, pt, etc.)
    coroutine.resume(kag_co)

    print("[KAG Runner] Started: " .. scene_path)
    return true
end

-- ── kag_runner.update(dt) ────────────────────────────────────────────────────
-- Called each frame by engine_update. Passes dt for timed operations like
-- [wait] and [trans] which accumulate elapsed time from coroutine.yield()
-- return values.

function kag_runner.update(dt)
    if not kag_co then return end

    -- Honour [p] click-wait: don't auto-advance when waiting for user input
    if ctx and ctx.waiting_input then return end

    local status = coroutine.status(kag_co)
    if status == "dead" then
        if ctx._scene_changed then
            ctx._scene_changed = false
            kag_co = coroutine.create(function()
                scheduler.run(ctx, ctx.tokens, ctx.token_index)
            end)
            coroutine.resume(kag_co, dt)
        else
            print("[KAG Runner] Script ended")
            kag_co = nil
        end
        return
    end

    local ok, err = coroutine.resume(kag_co, dt)
    if not ok then
        print("[KAG Runner] Coroutine error: " .. tostring(err))
        kag_co = nil
    end
end

-- ── kag_runner.on_click() ────────────────────────────────────────────────────
-- Called when the user clicks (KAG input focus). Resumes the coroutine to
-- advance past click-blocking operations like [p] (page break).

function kag_runner.on_click()
    if not kag_co then print("[Click] no coroutine"); return end
    if coroutine.status(kag_co) == "dead" then print("[Click] coroutine dead"); return end
    if not ctx then print("[Click] no ctx"); return end
    ctx.waiting_input = false
    -- Batch resume through all non-blocking tokens until next [p]
    local count = 0
    while coroutine.status(kag_co) ~= "dead" and not ctx.waiting_input and count < 200 do
        coroutine.resume(kag_co)
        count = count + 1
    end
    print("[Click] resumed " .. count .. " tokens, waiting=" .. tostring(ctx.waiting_input))
end

return kag_runner
