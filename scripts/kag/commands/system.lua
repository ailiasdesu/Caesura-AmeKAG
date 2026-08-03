-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/system.lua
--  Phase 4/9: KAG system tag handlers — [wait], [emb], [eval]
--  [wait]  — time-based blocking with CancelToken (spec [10.2.33])
--  [emb]   — embedded Lua execution within sandbox (spec [10.2.47])
--  [eval]  — expression evaluation with result storage
--  Phase 9: Integrated sandbox.lua for strict Release enforcement
-- =============================================================================

local Operation = require("kag.operation")
-- Sandbox is loaded by C++ lockdownScriptEnv() AFTER all modules are preloaded.
-- Do NOT require("sandbox") here ? it would activate lockdown too early.
local _sandbox_cache = nil
local function getSandbox()
    if not _sandbox_cache then
        _sandbox_cache = rawget(package.loaded, "sandbox")
    end
    return _sandbox_cache
end

local SystemCommands = {}

-- ═══════════════════════════════════════════════════════════════════════════
--  [wait time=1500]
--  Block for specified milliseconds. Cancelable via CancelToken.
--  Spec [10.2.33]: registers cancel callback, uses coroutine.yield.
-- ═══════════════════════════════════════════════════════════════════════════

function SystemCommands.wait(ctx, params)
    local ms = tonumber(params.time or params.ms or params.duration or 1000)
    if ms <= 0 then return end

    local operation <close> = Operation.start(ctx)
    local ct = operation.token

    local elapsed = 0
    local frameTime = 16  -- ~60fps default dt

    while elapsed < ms and not ct.cancelled do
        -- Yield each frame; scheduler feeds actual dt
        local dt = coroutine.yield() or frameTime
        if ct.cancelled then break end
        elapsed = elapsed + dt
    end

    if not ct.cancelled then
        operation:complete()
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [emb exp="ctx.f.score = ctx.f.score + 1"]
--  Execute embedded Lua within sandboxed environment.
--  Spec [10.2.47]: _ENV whitelist, Dev/Release dual mode.
--  Phase 9: Uses sandbox.execute() when sandbox.is_strict() is true.
-- ═══════════════════════════════════════════════════════════════════════════

function SystemCommands.emb(ctx, params)
    local exp = params.exp or params.code or params[1] or ""
    if #exp == 0 then return end

    -- Check if sandbox enforcement is active
    if getSandbox().is_strict() then
        -- Strict mode: use sandbox.execute()
        local env = {
            ctx  = ctx,
            tf   = ctx.tf or {},
            f    = ctx.f or {},
            sf   = ctx.sf or {},
            mp   = ctx.mp or {},
        }

        local ok, result, envOut = getSandbox().execute(exp, env)

        -- Sync back any mutations to the environment
        if envOut then
            ctx.tf = envOut.tf or ctx.tf
            ctx.f  = envOut.f  or ctx.f
            ctx.sf = envOut.sf or ctx.sf
            ctx.mp = envOut.mp or ctx.mp
        end

        if ok then
            ctx.tf = ctx.tf or {}
            ctx.tf.emb_result = result
        else
            ctx.tf = ctx.tf or {}
            ctx.tf.emb_result = nil
        end
        return
    end

    -- Dev mode (lax): original behavior — build sandbox with basic whitelist
    local sandbox = {
        ctx     = ctx,
        math    = math,
        string  = string,
        table   = table,
        os      = { clock = os.clock, date = os.date, time = os.time },
        tostring = tostring,
        tonumber = tonumber,
        type    = type,
        pairs   = pairs,
        ipairs  = ipairs,
        next    = next,
        print   = Sandbox.print_redirect,
        pcall   = pcall,
        select  = select,
        unpack  = unpack or table.unpack,
        error   = error,
    }

    -- Compile + execute in sandbox
    local fn, compileErr = load(exp, "=emb", "t", sandbox)
    if not fn then
        print("[SystemCmd] emb compile error: " .. tostring(compileErr))
        return
    end

    local ok, result = pcall(fn)
    if ok then
        ctx.tf = ctx.tf or {}
        ctx.tf.emb_result = result
    else
        print("[SystemCmd] emb runtime error: " .. tostring(result))
        ctx.tf = ctx.tf or {}
        ctx.tf.emb_result = nil
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [eval exp="1+2*3"]
--  Evaluate Lua expression and return result.
--  Phase 9: Uses sandbox.execute() when sandbox.is_strict() is true.
--  In dev mode, handled inline by scheduler.lua _execute loop.
-- ═══════════════════════════════════════════════════════════════════════════

function SystemCommands.eval(ctx, params)
    local exp = params.exp or params.code or params[1] or ""
    if #exp == 0 then return end

    -- If not strict, let scheduler handle it inline (backward compat)
    if not getSandbox().is_strict() then
        -- Scheduler intercepts before dispatch; this is a no-op
        return
    end

    -- Strict mode: evaluate through sandbox
    local env = {
        ctx  = ctx,
        tf   = ctx.tf or {},
        f    = ctx.f or {},
        sf   = ctx.sf or {},
        mp   = ctx.mp or {},
    }

    -- Wrap expression as a return statement to capture the result
    local code = "return " .. exp
    local ok, result, envOut = getSandbox().execute(code, env)

    -- Sync back mutations
    if envOut then
        ctx.tf = envOut.tf or ctx.tf
        ctx.f  = envOut.f  or ctx.f
        ctx.sf = envOut.sf or ctx.sf
        ctx.mp = envOut.mp or ctx.mp
    end

    if ok then
        ctx.tf = ctx.tf or {}
        ctx.tf.eval_result = result
    else
        print("[SystemCmd] eval error: " .. tostring(result))
        ctx.tf = ctx.tf or {}
        ctx.tf.eval_result = nil
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [history] — open backlog overlay UI
--  U3: Backlog display. Opens scrollable overlay with jump + voice replay.
-- ═══════════════════════════════════════════════════════════════════════════

function SystemCommands.history(ctx, params)
    local HistoryUI = require("history_ui")
    local result = HistoryUI.show(ctx)
    if type(result) == "table" and result.jump then
        -- The scheduler discards handler return values; signal the jump
        -- through ctx so the runner's dead-coroutine branch consumes it.
        ctx._pendingJump = { scene = result.scene, index = result.index }
        ctx.stop_flag = true
    end
    return result
end


-- ═══════════════════════════════════════════════════════════════════════════
--  [unlock type="cg" id="scene01"] — unlock a gallery/music item
--  Writes to ctx.unlockedCG (or ctx.unlockedMusic) for persistent tracking.
-- ═══════════════════════════════════════════════════════════════════════════

-- [rollback] — pop the newest token-level snapshot and re-run from there.
-- Returns false (no error) when there is nothing to roll back; the runner
-- surfaces that as a click with no effect.
function SystemCommands.rollback(ctx, params)
    if not ctx then return false end
    local ok, reason = require("kag_runner").rollback()
    if not ok then
        print("[rollback] unavailable: " .. tostring(reason))
        return false
    end
    return true
end

function SystemCommands.unlock(ctx, params)
    local kind = params.type or "cg"
    local id   = params.id or params.name or ""
    if id == "" then return end

    if kind == "cg" then
        ctx.unlockedCG = ctx.unlockedCG or {}
        ctx.unlockedCG[id] = true
    elseif kind == "music" then
        ctx.unlockedMusic = ctx.unlockedMusic or {}
        ctx.unlockedMusic[id] = true
    end
end
return SystemCommands
