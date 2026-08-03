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

local default_resume_adapter = {
    is_paused = function()
        local probe = rawget(_G, "_CAESURA_DEBUG_IS_PAUSED")
        if type(probe) == "function" then
            local ok, paused = pcall(probe)
            if not ok then error(paused, 0) end
            return paused == true
        end
        return rawget(_G, "_CAESURA_DEBUG_PAUSED") == true
    end,
    resume = function(_, co, value)
        return coroutine.resume(co, value)
    end,
}

local resume_adapter = default_resume_adapter

local function debug_pause_state()
    local ok, paused = pcall(resume_adapter.is_paused)
    if not ok then
        print("[KAG Runner] Debug pause probe failed: " .. tostring(paused))
        return nil, "debug-adapter-error"
    end
    return paused == true
end

local function can_resume()
    local paused, err = debug_pause_state()
    if paused == nil then return false, err end
    if paused then return false, "debug-paused" end
    return true
end

local function close_scheduler_coroutine()
    if not kag_co then return true end
    local closed, close_error = coroutine.close(kag_co)
    kag_co = nil
    return closed, close_error
end

local function resume_scheduler(origin, value)
    if not kag_co then return false, "not-running" end
    if coroutine.status(kag_co) == "dead" then return false, "dead" end

    local allowed, reason = can_resume()
    if not allowed then return false, reason end

    -- DebugProtocol resumes its anchored coroutine directly on the Lua owner
    -- thread. This notification must never advance it a second time.
    if origin == "debug-resume" then
        return true, coroutine.status(kag_co)
    end

    local called, resumed, result = pcall(
        resume_adapter.resume, origin, kag_co, value)
    if not called then
        result = resumed
        resumed = false
    end
    if not resumed then
        print("[KAG Runner] " .. origin .. " resume failed: " .. tostring(result))
        if kag_co and coroutine.status(kag_co) == "dead" then
            close_scheduler_coroutine()
        end
        return false, result
    end
    return true, result
end

-- Production uses the default adapter: Engine installs a live pause probe and
-- performs DebugProtocol resume itself. The global flag is a compatibility
-- fallback; injection is kept for deterministic arbitration tests.
function kag_runner.set_resume_adapter(adapter)
    if adapter == nil then
        resume_adapter = default_resume_adapter
        return
    end
    assert(type(adapter) == "table", "resume adapter must be a table")
    assert(type(adapter.is_paused) == "function",
        "resume adapter must provide is_paused()")
    assert(type(adapter.resume) == "function",
        "resume adapter must provide resume(origin, coroutine, value)")
    resume_adapter = adapter
end

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

-- Resume a previously saved game: reload the saved scene and start from the
-- saved token index ([load] support; the pending fields are set by
-- SaveCommands.load and consumed here when the current script ends).
local function resume_from_save()
    local path = ctx._pendingLoadScene
    if not path or #path == 0 then return false end
    local scene = flow.load_scene(path)
    if not scene then
        print("[KAG Runner] Failed to load saved scene: " .. path)
        ctx._pendingLoadScene = nil
        return false
    end
    ctx.labelMap = scene.labels
    ctx.tokens = scene.tokens
    ctx.token_index = ctx._pendingLoadToken or 1
    ctx.currentScene = path
    -- The saved [load] set stop_flag to end the running script; clear it so
    -- the resumed coroutine actually executes tokens (scheduler returns
    -- immediately while stop_flag is set).
    ctx.stop_flag = false
    ctx.current_scene = path  -- snake_case variant read by system.lua
    -- A crafted save may carry non-table seen_scenes or a stale call_stack;
    -- validate the former and drop the latter so [return] cannot redirect
    -- into stale frames.
    if type(ctx.seen_scenes) ~= "table" then ctx.seen_scenes = {} end
    ctx.call_stack = nil
    ctx._pendingLoadScene = nil
    ctx._pendingLoadToken = nil
    return true
end

-- ── kag_runner.start(scene_path) → boolean ───────────────────────────────────
-- Load a .ks scene and begin executing it. Returns true on success.

function kag_runner.start(scene_path)
    local allowed, reason = can_resume()
    if not allowed then
        print("[KAG Runner] Start blocked: " .. reason)
        return false, reason
    end

    if kag_co then
        if coroutine.status(kag_co) ~= "dead" then
            return false, "already-running"
        end
        close_scheduler_coroutine()
    end

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
    local resumed, resume_reason = resume_scheduler("start")
    if not resumed then return false, resume_reason end

    print("[KAG Runner] Started: " .. scene_path)
    return true
end

-- ── kag_runner.update(dt) ────────────────────────────────────────────────────
-- Called each frame by engine_update. Passes dt for timed operations like
-- [wait] and [trans] which accumulate elapsed time from coroutine.yield()
-- return values.

local auto_advance_ms = 0  -- accumulated ms before auto-mode advances

function kag_runner.update(dt)
    if not kag_co then return false, "not-running" end
    -- Engine frame delta is seconds; KAG command durations are milliseconds.
    local delta_ms = math.max(0, (tonumber(dt) or 0) * 1000)

    -- Typewriter reveal: advance the visible character count by the
    -- configured text speed (ms per char). Skip/auto modes reveal instantly.
    if ctx and ctx.reveal and not ctx.skip_mode then
        local speed = tonumber(ctx.text_speed) or 50
        if speed > 0 then
            ctx.reveal.elapsed = ctx.reveal.elapsed + delta_ms
            local shown = math.min(ctx.reveal.total,
                math.floor(ctx.reveal.elapsed / speed))
            local st = require("kag.text_scene").get_state(ctx)
            st.reveal_chars = shown
        end
    end

    -- Honour [p] click-wait: don't auto-advance when waiting for user input,
    -- EXCEPT in auto mode, which advances after a short delay (like a
    -- visual-novel auto-play button).
    if ctx and ctx.waiting_input then
        if ctx.skip_mode then
            if ctx.skip_mode == "seen" then
                -- Read-skip: only advance past text this scene already saw.
                local scene = ctx.current_scene or ctx.currentScene or ""
                local seen = type(ctx.seen_scenes) == "table"
                             and ctx.seen_scenes[scene]
                local wasSeen = type(seen) == "table"
                             and seen[ctx.token_index or 0] == true
                if not wasSeen then
                    -- Unseen text: stop read-skipping (fall back to manual).
                    auto_advance_ms = 0
                    return false, "waiting-input"
                end
            end
            -- Skip mode: advance immediately, no delay.
            auto_advance_ms = 0
            if ctx.reveal then
                local st = require("kag.text_scene").get_state(ctx)
                st.reveal_chars = ctx.reveal.total
            end
            return kag_runner.on_click()
        end
        if ctx.auto_mode then
            auto_advance_ms = auto_advance_ms + delta_ms
            if auto_advance_ms >= 1500 then  -- ~1.5s regardless of fps
                auto_advance_ms = 0
                return kag_runner.on_click()
            end
        end
        return false, "waiting-input"
    end
    auto_advance_ms = 0

    local status = coroutine.status(kag_co)
    if status == "dead" then
        close_scheduler_coroutine()
        if ctx._scene_changed then
            ctx._scene_changed = false
            kag_co = coroutine.create(function()
                scheduler.run(ctx, ctx.tokens, ctx.token_index)
            end)
            return resume_scheduler("update", delta_ms)
        elseif resume_from_save() then
            -- [load]: restart the saved scene at the saved token.
            kag_co = coroutine.create(function()
                scheduler.run(ctx, ctx.tokens, ctx.token_index)
            end)
            return resume_scheduler("update", delta_ms)
        else
            print("[KAG Runner] Script ended")
        end
        return false, "ended"
    end

    return resume_scheduler("update", delta_ms)
end

-- Submit persistent KAG text after the layer tree so dialogue stays above
-- scene content and is re-issued on every frame.
function kag_runner.render()
    if not ctx then return false, "no-context" end
    return true, require("kag.text_scene").render(ctx)
end

-- ── kag_runner.on_click() ────────────────────────────────────────────────────
-- Called when the user clicks (KAG input focus). Resumes the coroutine to
-- advance past click-blocking operations like [p] (page break).

function kag_runner.on_click()
    if not kag_co then print("[Click] no coroutine"); return false, "not-running" end
    if coroutine.status(kag_co) == "dead" then print("[Click] coroutine dead"); return false, "dead" end
    if not ctx then print("[Click] no ctx"); return false, "no-context" end

    -- Click during the typewriter animation reveals the rest instantly
    -- (standard VN behavior: first click completes the line, second advances).
    if ctx.reveal then
        local st = require("kag.text_scene").get_state(ctx)
        if st.reveal_chars < ctx.reveal.total then
            st.reveal_chars = ctx.reveal.total
            return true, "revealed"
        end
    end

    local allowed, reason = can_resume()
    if not allowed then return false, reason end

    -- Mark the current line as seen for read-skip: only text the player
    -- actually clicked through counts as read.
    local scene = ctx.current_scene or ctx.currentScene or ""
    if scene ~= "" and type(ctx.token_index) == "number" then
        ctx.seen_scenes = ctx.seen_scenes or {}
        local st = ctx.seen_scenes[scene]
        if type(st) ~= "table" then st = {}; ctx.seen_scenes[scene] = st end
        st[ctx.token_index] = true
    end

    ctx.waiting_input = false
    -- Batch resume through all non-blocking tokens until next [p]
    local count = 0
    while coroutine.status(kag_co) ~= "dead" and not ctx.waiting_input and count < 200 do
        local resumed, resume_reason = resume_scheduler("click")
        if not resumed then
            return false, resume_reason
        end
        count = count + 1
    end
    print("[Click] resumed " .. count .. " tokens, waiting=" .. tostring(ctx.waiting_input))
    return true, count
end

-- Optional owner-thread notification after C++ has resumed DebugProtocol's
-- anchored coroutine. It observes state only; Engine skips normal update on
-- that frame so the scheduler cannot advance twice.
function kag_runner.debug_resume()
    return resume_scheduler("debug-resume")
end

return kag_runner
