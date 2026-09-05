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
--  via ctx.load_tokens.
--  When a cross-scene [jump] sets new tokens and token_index, the
--  scheduler returns (coroutine dies); we detect this via a
--  _scene_changed flag and re-spawn the coroutine on the next update.
-- =============================================================================

local flow = require("flow")
local scheduler = require("scheduler")

local kag_runner = {}

-- Resolve a "*label" choice target to a token index: label_index
-- (built by the scheduler) first, token-scan fallback. Both resume AT
-- the label token itself ([label] is a no-op, matching the [jump]
-- convention -- review nit: the two paths must agree).
function kag_runner.resolve_label_index(ctx, label)
    local idx = ctx.label_index and ctx.label_index[label]
    if not idx then
        for i, tok in ipairs(ctx.tokens) do
            if tok[1] == "label" and tok[2] and tok[2].name == label then
                idx = i
                break
            end
        end
    end
    return idx
end

--- Resolve a "*label" choice target and stage the resume on ctx. The
-- update() branch calls this; tests call the same function so a broken
-- call site cannot pass CI (review warn).
-- Returns true when the label was found and ctx.token_index was staged.
function kag_runner.stage_label_jump(ctx, path)
    if type(path) ~= "string" or path:sub(1, 1) ~= "*" then
        return false
    end
    local idx = kag_runner.resolve_label_index(ctx, path:sub(2))
    if not idx then
        return false
    end
    ctx.token_index = idx
    ctx.stop_flag = false
    return true
end
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

-- KAG scene debugger resume entry points (called from the RPC eval
-- channel / editor): clear the runner's pause flag and let the scheduler
-- continue. continue_run() just resumes; step() arms a one-token pause.
function kag_runner.debug_resume()
    if ctx then ctx._kag_debug_paused = false end
    require("kag_debug").continue_run()
    return true
end

function kag_runner.debug_step()
    if ctx then ctx._kag_debug_paused = false end
    require("kag_debug").step()
    return true
end

--- kag_runner.get_ctx() — expose the live KAG context (RPC inspection).
function kag_runner.get_ctx()
    return ctx
end

--- kag_runner.remap_token_index(old_tokens, old_index, new_tokens) → index
--  Best resume position in a re-parsed scene stream:
--  1. first token with the same cmd AND the same primary param
--     (text/name/storage/file -- the content-bearing slot)
--  2. nearest preceding *label* in the old stream, looked up by name in
--     the new stream (labels are content-stable anchors)
--  3. 1 (scene start)
function kag_runner.remap_token_index(old_tokens, old_index, new_tokens)
    local old = old_tokens and old_tokens[old_index]
    if old and new_tokens then
        local cmd = old[1]
        local params = old[2] or {}
        local prim = params.text or params.name or params.storage or params.file
        for i, t in ipairs(new_tokens) do
            if t[1] == cmd then
                local np = t[2] or {}
                local nprim = np.text or np.name or np.storage or np.file
                if prim ~= nil and prim == nprim then
                    return i
                end
            end
        end
        -- fallback: nearest preceding label in the OLD stream
        for j = old_index, 1, -1 do
            local t = old_tokens[j]
            if t[1] == "label" and t[2] and t[2].name then
                for i, nt in ipairs(new_tokens) do
                    if nt[1] == "label" and nt[2] and nt[2].name == t[2].name then
                        return i
                    end
                end
                break
            end
        end
    end
    return 1
end

--- kag_runner.reload_scene(path?) — hot-reload a .ks scene (editor
--  workflow). Re-parses through flow.reload_scene (mods-aware, cache-
--  busting), preserves game state (f/sf/tf/layers/backlog/undo stack),
--  remaps the execution position, and re-spawns the scheduler coroutine
--  at the new token. When `path` is not the running scene, only the
--  scene cache is invalidated. Returns true + status.
function kag_runner.reload_scene(path)
    if not ctx then return false, "no-context" end
    if ctx._kag_debug_paused then return false, "kag-paused" end
    local allowed, reason = can_resume()
    if not allowed then return false, reason end
    if kag_co then
        local status = coroutine.status(kag_co)
        if coroutine.running() == kag_co or status == "running" or status == "normal" then
            return false, "scheduler-running"
        end
    end
    local target = path
    if not target or #target == 0 then
        target = ctx.current_scene or ctx.currentScene
    end
    if not target or #target == 0 then return false, "no-scene" end

    local flow = require("flow")
    local scene = flow.reload_scene(target)
    if not scene then return false, "reload-failed" end

    local running = ctx.current_scene or ctx.currentScene
    if target ~= running then
        return true, "cached"  -- cache busted; not the live scene
    end

    local new_tokens = scene.tokens
    local new_index = kag_runner.remap_token_index(
        ctx.tokens or {}, ctx.token_index or 1, new_tokens)

    -- Close the old scheduler synchronously: its cleanup may enqueue requests
    -- that must belong to the cancellation below, never to the new scene.
    -- Retain the dead coroutine until update() consumes the pending reload.
    if kag_co then
        local closed, close_error = coroutine.close(kag_co)
        if not closed then print("[KAG Runner] Scene reload cleanup failed: " .. tostring(close_error)) end
    end
    require("backend").cancel_async_loads()

    -- Stage the swap; update() starts the replacement before processing old
    -- input waits. Failed/cache-only reloads never reach this boundary.
    ctx.tokens = new_tokens
    ctx.labelMap = scene.labels
    ctx.label_index = nil      -- raw tokens: next jump re-scans
    ctx.token_index = new_index
    ctx.current_scene = target
    ctx.currentScene = target
    ctx.call_stack = nil       -- stale frames point into the old stream
    ctx._pendingSceneReload = true
    ctx.stop_flag = true
    return true, "reloaded"
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
    -- KAG scene debugger pause: the scheduler yielded "__kag_pause"
    -- (breakpoint/step hit). Stop advancing until the editor resumes
    -- (KAGDebug.continue_run/step from RPC); update() checks the flag.
    if result == "__kag_pause" then
        ctx._kag_debug_paused = true
        local kagDebug = require("kag_debug")
        kagDebug.set_paused(true)
        return true, "kag-paused"
    end
    -- DebugProtocol resume notification: the anchored coroutine was
    -- advanced directly; report the same state the caller expects.
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
    ctx.label_index = nil  -- scene swap bypasses the scheduler: rebuild
    -- Coerce the restored token index: crafted saves may carry a string or
    -- table; scheduler.run compares `i <= #tokens` numerically.
    -- t62 (round-94 guard; web/bridge.js keeps TWO mirrored copies of this
    -- exact discriminator -- search "atLoadTag9" (t69) -- change all three
    -- sites together): a self-referential load -- [save] then [load] back-to-back
    -- in the SAME scene -- recorded a resume point at/before the consumed
    -- [load] token, so a plain restore re-executes the [save]/[load] block
    -- forever. When the reloaded scene is the current one AND the resume was
    -- triggered BY a [load] tag sitting exactly at the current cursor AND the
    -- saved resume point is at/before it, advance past the [load] so
    -- execution continues forward. (The cursor-at-load-tag check keeps a
    -- direct SaveCommands.load() issued from a pause point -- the Golden VN
    -- roundtrip driver -- on the exact saved token: there is no load tag to
    -- skip there.) Cross-scene loads (a genuine [load] into a different
    -- scene) are unchanged: the scene differs, so the saved token is used
    -- exactly.
    local cur = tonumber(ctx.token_index) or 1
    local curTok = ctx.tokens and ctx.tokens[cur]
    -- Shape-agnostic: flow.load_scene tokens carry the command at [1];
    -- the web bridge's raw tokenizer.parse tokens carry it in .cmd (t69:
    -- the [1]-only check never fired on web, so a self-referential
    -- [save]->[load] re-spawned forever).
    local atLoadTag = type(curTok) == "table" and (curTok[1] == "load" or curTok.cmd == "load")
    if ctx._pendingLoadOriginScene == path and atLoadTag
        and (tonumber(ctx._pendingLoadToken) or 1) <= cur then
        ctx.token_index = cur + 1
    else
        ctx.token_index = math.max(1, tonumber(ctx._pendingLoadToken) or 1)
    end
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
        -- Accessibility: cc_mode (closed captions) synced from config at
        -- startup (pcall: degraded contexts without the backend factory
        -- must not fail the runner); Settings._applyAll updates it live.
        cc_mode = (function()
            local okC, config = pcall(require, "config")
            if okC and config.accessibility then
                return config.accessibility.cc_mode == true
            end
            return false
        end)(),
        load_tokens = load_tokens,
        -- Token-level rollback stack (see kag/snapshot.lua); cleared on
        -- [load] and on scene changes. Bounded to cap memory.
        _undoStack = {},
        _undoLimit = 64,
    }
    rawset(_G, "_CAESURA_CTX", ctx)

    -- t212: first-definition-wins runtime-error handler (gesture-hook
    -- precedent). A pre-set ctx.handle_error (custom recovery policy) is
    -- kept; the default reports to the composition-root chain and keeps
    -- console + file-log (Debug.log) visibility, with G4 traceback.
    kag_runner.install_error_handler(ctx)

    -- Load initial scene
    local scene = flow.load_scene(scene_path)
    if not scene then
        print("[KAG Runner] Failed to load scene: " .. scene_path)
        return false
    end
    ctx.tokens = scene.tokens
    ctx.labelMap = scene.labels
    ctx.label_index = nil  -- resume path: rebuild for the restored scene
    ctx.current_scene = scene_path
    ctx.currentScene = scene_path  -- also set camelCase for text/save commands

    -- Fresh session: reset session-local control-flow stacks and the
    -- macro expansion depth tracker (round 75 -- they live on ctx now so
    -- [save]/[load] can snapshot/restore them; a new game must not
    -- inherit the previous game's [for]/[while]/[if]/[switch] entries).
    ctx._forStack = {}
    ctx._whileStack = {}
    ctx._ifStack = {}
    ctx._switchStack = {}
    ctx._macroStack = nil
    ctx._resumeLoopStacks = nil
    -- [round 98] Cross-scene switch budget: session-scoped so it bounds an
    -- A<->B [jump]/[call]/[link] ping-pong driven by the frame loop AND an
    -- unbounded cross-scene [call] nesting (grows ctx.call_stack) across
    -- re-spawns. Reset ONLY here on a fresh session -- NOT per scheduler.run
    -- (a per-run reset would clear it on every scene swap and defeat the
    -- guard). scheduler.budget_scene_switch increments it per allowed switch.
    ctx._sceneSwitches = 0

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
    if not kag_co and not (ctx and ctx._pendingSceneReload) then return false, "not-running" end
    -- KAG scene debugger pause: while a breakpoint/step holds the
    -- scheduler, do not advance. The editor resumes by calling
    -- KAGDebug.continue_run() (or KAGDebug.step()) via RPC; the next
    -- update() then resumes the coroutine normally.
    if ctx and ctx._kag_debug_paused then
        return false, "kag-paused"
    end
    if ctx and ctx._pendingSceneReload then
        local allowed, reason = can_resume()
        if not allowed then return false, reason end
        close_scheduler_coroutine()
        ctx._pendingSceneReload = nil
        ctx.stop_flag = false
        ctx.waiting_input = false
        ctx.reveal = nil
        auto_advance_ms = 0
        kag_co = coroutine.create(function()
            scheduler.run(ctx, ctx.tokens, ctx.token_index)
        end)
        return resume_scheduler("update", math.max(0, (tonumber(dt) or 0) * 1000))
    end
    -- Replay system: record mode advances the capture clock every frame;
    -- playback mode fires due clicks before normal input processing -- the
    -- recorded event drives the same on_click path the player used
    -- (coordinates restored so choice buttons resolve).
    local replay = require("replay")
    local replay_mode = replay.get_mode()
    if replay_mode ~= "off" then
        local delta_ms = math.max(0, (tonumber(dt) or 0) * 1000)
        if replay_mode == "record" then
            replay.tick(delta_ms, nil)
        elseif replay_mode == "playback" then
            replay.tick(delta_ms, function(x, y)
                if x ~= nil then _G._GAME_MOUSE_X = x end
                if y ~= nil then _G._GAME_MOUSE_Y = y end
                kag_runner.on_click()
            end)
        end
    end
    -- Engine frame delta is seconds; KAG command durations are milliseconds.
    local delta_ms = math.max(0, (tonumber(dt) or 0) * 1000)

    -- [tween] fire-and-forget (wait=false) tweens: advance every frame.
    -- Blocking [tween] (wait=true) advances itself inside its own
    -- operation loop; this hook only services the non-blocking queue
    -- (skipped entirely once empty). The module loads through kag.lua;
    -- the pcall guards a missing registration in dev tooling.
    if ctx and type(ctx.tweens) == "table" and #ctx.tweens > 0 then
        pcall(function()
            local tw = require("kag.commands.tween")
            if tw and tw.update then tw.update(ctx, delta_ms) end
        end)
    end

    -- [t127 M-F2] Overlay pump for the runtime default PAGEUP hook: the
    -- default in KAG.gesture_defaults parks its backlog coroutine in
    -- ctx._gesture_history_co. Entries that override the hook drive their
    -- own coroutine and never set this slot; without this per-frame driver
    -- a pump-less entry would freeze on the first frame with input_focus
    -- stuck on "history" (soft-lock). Body lives in pump_gesture_overlay
    -- (named export, t131) so tests can drive its paths directly.
    kag_runner.pump_gesture_overlay(ctx)

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
            -- [typewriter sound] per-character SE (t201): fire when the
            -- reveal crossed >= interval NEW characters since the LAST
            -- FIRE -- not since the last frame, or a 1-char-per-frame
            -- reveal with interval=3 would never fire at all (t201
            -- empirical trace). last_shown is therefore the count at the
            -- last fire; the skip/click instant writes mark
            -- last_shown = total OUTSIDE this block, so an instant-reveal
            -- line cannot fire a burst of SEs (t200 plan 2.3).
            local prev = ctx.reveal.last_shown or 0
            if shown > prev and type(ctx.typewriter_sound) == "string"
               and ctx.typewriter_sound ~= "" then
                local interval = tonumber(ctx.typewriter_sound_interval) or 1
                if (shown - prev) >= interval then
                    backend.audio_play("se", ctx.typewriter_sound)
                    ctx.reveal.last_shown = shown
                end
            end
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
                    -- Unseen text: with skip_auto ("force") keep advancing at
                    -- an accelerated rate (2x) so players can rush past new
                    -- content without mashing; without it, stop read-skipping
                    -- (fall back to manual) -- classic read-skip behavior.
                    if ctx.skip_auto then
                        auto_advance_ms = ctx.skip_rate or 60
                    else
                        auto_advance_ms = 0
                        return false, "waiting-input"
                    end
                end
            end
            -- Skip mode: advance immediately, no delay.
            auto_advance_ms = 0
            if ctx.reveal then
                local st = require("kag.text_scene").get_state(ctx)
                st.reveal_chars = ctx.reveal.total
                -- [typewriter sound] (t201): same boundary mark as the
                -- click-instant write -- skip shows the line instantly.
                ctx.reveal.last_shown = ctx.reveal.total
            end
            return kag_runner.on_click()
        end
        if ctx.auto_mode then
            -- Galgame-standard auto pacing: while a voice line is playing,
            -- hold the advance timer (players read along with the audio);
            -- the 1.5s countdown only runs once the voice finished (or none).
            local voicePlaying = false
            pcall(function()
                voicePlaying = backend.audio_is_playing and backend.audio_is_playing("voice")
            end)
            if voicePlaying then
                auto_advance_ms = 0
            else
                auto_advance_ms = auto_advance_ms + delta_ms
                if auto_advance_ms >= (ctx.auto_delay or 1500) then
                    auto_advance_ms = 0
                    return kag_runner.on_click()
                end
            end
        end
        return false, "waiting-input"
    end
    auto_advance_ms = 0

    local status = coroutine.status(kag_co)
    if status == "dead" then
        close_scheduler_coroutine()
        if ctx._pendingRollback then
            -- Rollback: a snapshot was already restored into ctx by
            -- rollback(); re-spawn the scheduler at the saved position.
            -- rollback() set stop_flag to end the old coroutine; clear it
            -- or scheduler.run returns immediately and the script halts.
            ctx._pendingRollback = nil
            ctx.stop_flag = false
            kag_co = coroutine.create(function()
                scheduler.run(ctx, ctx.tokens, ctx.token_index)
            end)
            return resume_scheduler("update", delta_ms)
        end
        if ctx._scene_changed then
            -- A cross-scene switch is processed BEFORE the deferred
            -- [select]/[button] pending-jump: the choice label target is
            -- SCENE-LOCAL, and a run-ending switch (cross-scene [jump]/[link])
            -- already swapped tokens/labelMap to the NEW scene where that
            -- label cannot exist -- resolving it there printed "Choice label
            -- not found" and left the run stalled not-running forever (t33 /
            -- Golden Project finding). The explicit [jump]/[link] is the new
            -- flow authority, so the stale redirect is dropped loudly here.
            -- (A cross-scene [call] is NOT an authority: [return] restores the
            -- caller's tokens/labels and clears _scene_changed, so a deferred
            -- choice jump made before the call still resolves in the caller
            -- scene -- t43. Caveat, t49: the choice label must belong to the
            -- RESTORED scene -- a pending jump created by a callee-internal
            -- [select] is dropped at the [return] restore point, not here.)
            ctx._scene_changed = false
            if ctx._pendingJump then
                print("[KAG Runner] Dropping deferred choice jump across scene"
                      .. " switch: " .. tostring(ctx._pendingJump)
                      .. " (current scene: "
                      .. tostring(ctx.current_scene or ctx.currentScene or "?")
                      .. ")")
                ctx._pendingJump = nil
            end
            -- Scene changes (jump/call/link) invalidate every snapshot.
            ctx._undoStack = {}
            kag_co = coroutine.create(function()
                scheduler.run(ctx, ctx.tokens, ctx.token_index)
            end)
            return resume_scheduler("update", delta_ms)
        end
        if ctx._pendingJump then
            -- History/choice jump: reload the target scene and resume at the
            -- requested token (cleared on failure so we don't loop). The
            -- scene comes from backlog entries (crafted saves can carry an
            -- arbitrary path); apply the same allowlist as [load] before
            -- touching the filesystem.
            local target = ctx._pendingJump
            ctx._pendingJump = nil
            local path = target.scene or target
            if type(path) == "string" and path:sub(1, 1) == "*" then
                -- Same-scene label jump (KAG3 [select]/[button] convention:
                -- target="*label" -- review should-fix: the scene-path
                -- allowlist rejected these, so classic choice scripts never
                -- resolved their targets). Delegates to the shared staging
                -- helper so tests exercise the SAME code path.
                if kag_runner.stage_label_jump(ctx, path) then
                    kag_co = coroutine.create(function()
                        scheduler.run(ctx, ctx.tokens, ctx.token_index)
                    end)
                    return resume_scheduler("update", delta_ms)
                end
                print("[KAG Runner] Choice label not found: " .. tostring(path:sub(2)))
                return false, "label-not-found"
            end
            if type(path) ~= "string" or not require("kag.commands.save")._safeScenePath(path) then
                print("[KAG Runner] Rejected unsafe jump target: " .. tostring(path))
                return false, "unsafe-jump-target"
            end
            local scene = flow.load_scene(path)
            if scene then
                ctx.tokens = scene.tokens
                ctx.labelMap = scene.labels
                ctx.label_index = nil  -- history/choice jump: rebuild
                ctx.current_scene = path
                ctx.currentScene = path
                ctx.token_index = math.max(1, tonumber(target.index) or 1)
                ctx.stop_flag = false
                ctx._undoStack = {}
                kag_co = coroutine.create(function()
                    scheduler.run(ctx, ctx.tokens, ctx.token_index)
                end)
                return resume_scheduler("update", delta_ms)
            end
            print("[KAG Runner] Failed to load jump target: " .. tostring(target.scene or target))
        end
        if resume_from_save() then
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
local cc_bar_tex = nil  -- cached solid texture for the CC backing bar
function kag_runner.render()
    if not ctx then return false, "no-context" end
    local config = require("config")
    local ok, n = true, require("kag.text_scene").render(ctx)
    -- Closed captions (accessibility): a voiced line is drawn at a fixed
    -- bottom position, independent of the textbox, while cc_mode is on.
    -- Single source of truth: ctx.cc_mode (set by Settings._applyAll,
    -- game scripts, or the runner's startup sync from config).
    if ctx.cc_mode == true and ctx.cc_text
       and #(ctx.cc_text.text or "") > 0 then
        local backend = require("backend")
        local vp = ctx.viewport or {}
        local ok2, rw, rh = pcall(require("backend").get_resolution)
        if not (ok2 and rw and rh and rw > 0 and rh > 0) then rw, rh = 1920, 1080 end
        local w = vp.width or rw
        local h = vp.height or rh
        local text = ctx.cc_text.text
        local speaker = ctx.cc_text.speaker or ""
        if #speaker > 0 then
            text = speaker .. "：" .. text
        end
        local x = math.floor(w / 2)
        local y = h - 60
        -- Black backing bar for readability over any scene content
        -- (texture cached: one solid texture, re-issued every frame).
        if not cc_bar_tex then
            pcall(function()
                cc_bar_tex = backend.create_solid_texture(0, 0, 0, 150)
            end)
        end
        if cc_bar_tex then
            pcall(backend.draw_viewport, cc_bar_tex, x - 620, y - 8, 1240, 44)
        end
        pcall(backend.render_text, text, x, y, 255, 255, 255, 255)
    end

    return ok, n
end

-- ── kag_runner.rollback() → boolean ─────────────────────────────────────────
-- Pop the newest snapshot and restore ctx to it. The running coroutine is
-- stopped via stop_flag; update() re-spawns it at the saved token.

function kag_runner.rollback()
    if not ctx then return false, "no-context" end
    if type(ctx._undoStack) ~= "table" or #ctx._undoStack == 0 then
        return false, "nothing-to-rollback"
    end
    -- Can't roll back while a choice menu is open (stack cleared on choice).
    if ctx._choiceMode then return false, "choice-open" end
    local snap = table.remove(ctx._undoStack)
    if not require("kag.snapshot").restore(ctx, snap) then
        return false, "restore-failed"
    end
    ctx.stop_flag = true
    ctx._pendingRollback = true
    ctx.waiting_input = false
    return true
end

-- ── kag_runner.on_click() ────────────────────────────────────────────────────
-- Called when the user clicks (KAG input focus). Resumes the coroutine to
-- advance past click-blocking operations like [p] (page break).

function kag_runner.on_click()
    -- History/backlog overlay owns the pointer while open: ignore clicks so
    -- the overlay coroutine is not batch-resumed underneath. (Checked first:
    -- the guard must hold even when no coroutine is running yet.)
    if ctx and ctx.input_focus == "history" then return false, "history-open" end

    -- Replay system (record mode): log the click with the current mouse
    -- position (choice coordinates included) before advancing.
    local replay = require("replay")
    if replay.get_mode() == "record" then
        replay.record("click",
            _G._GAME_MOUSE_X or 0, _G._GAME_MOUSE_Y or 0)
    end
    if not kag_co then print("[Click] no coroutine"); return false, "not-running" end
    if coroutine.status(kag_co) == "dead" then print("[Click] coroutine dead"); return false, "dead" end
    if not ctx then print("[Click] no ctx"); return false, "no-context" end

    -- Click during the typewriter animation reveals the rest instantly
    -- (standard VN behavior: first click completes the line, second advances).
    if ctx.reveal then
        local st = require("kag.text_scene").get_state(ctx)
        if st.reveal_chars < ctx.reveal.total then
            st.reveal_chars = ctx.reveal.total
            -- [typewriter sound] (t201): the click-instant reveal shows the
            -- whole remainder; mark the SE boundary complete so the
            -- elapsed-driven follow-through cannot fire a burst of SEs for
            -- characters that were never animated char-by-char.
            ctx.reveal.last_shown = ctx.reveal.total
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

    -- Push a rollback snapshot BEFORE advancing: restore returns to the
    -- line the player just finished (token_index = last completed token).
    -- The reveal-complete early return above already excluded the animating
    -- first click, so every click that reaches here actually advances and
    -- gets a snapshot. (Gating on ctx.reveal==nil would never fire: [ch]/
    -- [text] always set reveal.)
    local snap = require("kag.snapshot").capture(ctx)
    if snap then
        local stack = ctx._undoStack
        if type(stack) ~= "table" then stack = {}; ctx._undoStack = stack end
        stack[#stack + 1] = snap
        if #stack > (ctx._undoLimit or 64) then
            table.remove(stack, 1)
        end
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

-- [t131] Overlay pump for the runtime default PAGEUP hook, exported so the
-- lua suite can drive the dead/error/no-op paths directly. Normal close
-- restores input_focus inside the history command itself; only the error
-- path forces it back here.
function kag_runner.pump_gesture_overlay(c)
    if not (c and c._gesture_history_co) then return end
    local hco = c._gesture_history_co
    if coroutine.status(hco) == "dead" then
        c._gesture_history_co = nil
        return
    end
    local ok, err = coroutine.resume(hco)
    if not ok then
        print("[History] overlay error: " .. tostring(err))
        c._gesture_history_co = nil
        c.input_focus = "kag"
        pcall(function() require("history_ui")._hideAll(c) end)
    end
end

-- t212: runtime-error handler install helper (named export so the lua
-- suite can drive the default and first-definition-wins paths directly).
function kag_runner.install_error_handler(c)
    if c == nil then return nil end
    if c.handle_error == nil then
        c.handle_error = function(cmd, err, scene, line)
            -- G4: attach Lua stack traceback (level 2 = caller of the handler).
            local msg = debug.traceback(tostring(err), 2)
            if msg == "" then msg = tostring(err) end
            local label = string.format("[ErrorUI] %s @ %s:%s: %s",
                tostring(cmd), tostring(scene or "?"), tostring(line or 0), msg)
            -- File + console visibility first: Debug.log writes the engine's
            -- logs/caesura_*.log (headless tooling and windowed player alike);
            -- plain print covers console-only contexts.
            local dbg = rawget(_G, "Debug")
            if dbg and type(dbg.log) == "function" then
                pcall(dbg.log, "error", label)
            else
                print(label)
            end
            -- Composition-root chain: Engine.report_command_error -> di
            -- ErrorReporter -> ErrorUI (no-op when no reporter is installed).
            local eng = rawget(_G, "Engine")
            if eng and type(eng.report_command_error) == "function" then
                pcall(eng.report_command_error, cmd, msg,
                      tostring(scene or ""), tonumber(line) or 0)
            end
        end
    end
    return c.handle_error
end

return kag_runner
