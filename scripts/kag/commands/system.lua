-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/system.lua
--  Phase 4/9: KAG system tag handlers — [wait], [emb], [eval]
--  [wait]  — time-based blocking with CancelToken (spec [10.2.33])
--  [emb]   — embedded Lua execution within sandbox (spec [10.2.47])
--  [eval]  — expression evaluation with result storage
--  Phase 9: Integrated sandbox.lua for strict Release enforcement
-- =============================================================================

local CancelToken = require("kag.cancel_token")
local Sandbox = require("sandbox")

local SystemCommands = {}

-- ═══════════════════════════════════════════════════════════════════════════
--  [wait time=1500]
--  Block for specified milliseconds. Cancelable via CancelToken.
--  Spec [10.2.33]: registers cancel callback, uses coroutine.yield.
-- ═══════════════════════════════════════════════════════════════════════════

function SystemCommands.wait(ctx, params)
    local ms = tonumber(params.time or params.ms or params.duration or 1000)
    if ms <= 0 then return end

    -- Create cancel token — allows [jump]/[link] to interrupt the wait
    local ct = CancelToken.new()
    ctx.active_operations = ctx.active_operations or {}
    table.insert(ctx.active_operations, ct)

    local elapsed = 0
    local frameTime = 16  -- ~60fps default dt

    while elapsed < ms and not ct.cancelled do
        -- Yield each frame; scheduler feeds actual dt
        local dt = coroutine.yield() or frameTime
        elapsed = elapsed + dt
    end

    -- Clean up: remove from active operations
    for i = #ctx.active_operations, 1, -1 do
        if ctx.active_operations[i] == ct then
            table.remove(ctx.active_operations, i)
            break
        end
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
    if Sandbox.is_strict() then
        -- Strict mode: use sandbox.execute()
        local env = {
            ctx  = ctx,
            tf   = ctx.tf or {},
            f    = ctx.f or {},
            sf   = ctx.sf or {},
            mp   = ctx.mp or {},
        }

        local ok, result, envOut = Sandbox.execute(exp, env)

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
    if not Sandbox.is_strict() then
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
    local ok, result, envOut = Sandbox.execute(code, env)

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
    return HistoryUI.show(ctx)
end

return SystemCommands
