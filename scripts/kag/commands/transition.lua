-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/transition.lua
--  Phase 4: KAG transition tag handlers — [trans], [move], [quake], [fade]
--  Wires KAG tags to the GPU-backed transition.lua engine (crossfade/wipe/rule).
--  All coroutine-based for blocking semantics with CancelToken support.
-- =============================================================================

local CancelToken = require("kag.cancel_token")
local Transition  = require("transition")
local backend     = require("backend")
local layers      = require("layers")

local TransCommands = {}

-- ═══════════════════════════════════════════════════════════════════════════
--  Internal: helper to resolve transition method from KAG params
-- ═══════════════════════════════════════════════════════════════════════════

local function resolve_method(params)
    local kind = params.kind or params.type or "crossfade"
    local map  = {
        crossfade = "crossfade",
        rule      = "rule",
        wipe      = "wipe",
        universal   = "crossfade",
        scroll      = "wipe",
        ["scroll_right"] = "wipe",
    }
    return map[kind] or "crossfade"
end

local function resolve_direction(params)
    local dir = params.direction or params.dir or "left"
    local map = {
        left   = "left",   right  = "right",
        top    = "top",    bottom = "bottom",
        up     = "top",    down   = "bottom",
    }
    return map[dir] or "left"
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [trans time=500 kind="crossfade" rule="rule_01.png"]
--  GPU-backed scene transition. Blocks coroutine until complete.
--  Spec [2.3] [10.2.33]: CancelToken, yield-based blocking.
--  Option: wait_preload=true (default) blocks until transition-slot
--  preloaded textures are ready.
-- ═══════════════════════════════════════════════════════════════════════════

function TransCommands.trans(ctx, params)
    local dur     = tonumber(params.time or params.duration or 500)
    local method  = resolve_method(params)
    local dir     = resolve_direction(params)
    local rule    = params.rule or params.rule_image or nil

    -- Create cancel token
    local ct = CancelToken.new()
    ctx.active_operations = ctx.active_operations or {}
    table.insert(ctx.active_operations, ct)

    -- Phase G8-U2: promote transition-slot preloaded textures to main cache
    local ResourceCommands = require("kag.commands.resource")
    if ResourceCommands.has_pending_transition() then
        -- Wait with 5-second timeout
        local waited = 0
        while ResourceCommands.has_pending_transition() and waited < 5000 do
            if ct.cancelled then break end
            coroutine.yield()
            waited = waited + 16
        end
    end
    ResourceCommands.promote_transition_slot()

    -- Wait for preloaded textures if configured
    local waitPreload = params.wait_preload
    if waitPreload == nil then waitPreload = true end
    if waitPreload and ctx._preloadPending then
        while ctx._preloadPending do
            if ct.cancelled then break end
            coroutine.yield()
        end
    end

    if ct.cancelled then
        Transition.cancel()
        return
    end

    -- Capture current screen as "from" texture
    local fromTex = Transition.capture_screen and Transition.capture_screen(ctx) or nil

    -- Start the transition on the GPU engine
    local tx
    if method == "rule" and rule then
        local ruleTex = Transition.preload_rule(rule)
        tx = Transition.start_rule(dur, fromTex, nil, ruleTex, dur)
    elseif method == "wipe" then
        tx = Transition.start_wipe(dur, fromTex, nil, dir)
    else
        tx = Transition.start_crossfade(dur, fromTex, nil, dur)
    end

    -- Block via coroutine.yield until transition completes or is cancelled
    local elapsed = 0
    while elapsed < dur and not ct.cancelled and Transition.is_active() do
        local dt = coroutine.yield() or 16
        elapsed = elapsed + dt
        Transition.tick(dt)
    end

    -- Phase G8-U1: explicit GC step after transition
    pcall(function() collectgarbage("step", 20) end)

    -- Cleanup
    if ct.cancelled then
        Transition.cancel()
    end

    for i = #ctx.active_operations, 1, -1 do
        if ctx.active_operations[i] == ct then
            table.remove(ctx.active_operations, i)
            break
        end
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [move layer="fg" x=200 y=100 time=300 path="(0,0)(100,50)(200,100)"]
--  Animate layer position using ease-in-out bezier.
--  Spec [10.2.41]: cubic bezier + easing.
--  Blocks coroutine until animation complete. Cancelable.
-- ═══════════════════════════════════════════════════════════════════════════

function TransCommands.move(ctx, params)
    local dur      = tonumber(params.time or params.duration or 300)
    local targetX  = tonumber(params.x or 0)
    local targetY  = tonumber(params.y or 0)
    local layerName= params.layer or "fg"
    local easing   = params.easing or "ease-in-out"

    if dur <= 0 then return end

    local node = layers.find(ctx, layerName)
    if not node then
        print("[TransCmd] move: layer not found: " .. layerName)
        return
    end

    local ct = CancelToken.new()
    ctx.active_operations = ctx.active_operations or {}
    table.insert(ctx.active_operations, ct)

    local startX = node.x or 0
    local startY = node.y or 0
    local elapsed = 0

    while elapsed < dur and not ct.cancelled do
        local dt = coroutine.yield() or 16
        elapsed = elapsed + dt
        local t = math.min(elapsed / dur, 1.0)

        -- Apply easing
        if easing == "ease-in" then
            t = t * t
        elseif easing == "ease-out" then
            t = 1 - (1 - t) * (1 - t)
        elseif easing == "ease-in-out" then
            t = t < 0.5 and (2 * t * t) or (1 - math.pow(-2 * t + 2, 2) / 2)
        end

        local curX = startX + (targetX - startX) * t
        local curY = startY + (targetY - startY) * t
        layers.move_layer(node, curX, curY)
    end

    -- Snap to final position
    layers.move_layer(node, targetX, targetY)

    for i = #ctx.active_operations, 1, -1 do
        if ctx.active_operations[i] == ct then
            table.remove(ctx.active_operations, i)
            break
        end
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [quake time=500 intensity=5 count=10]
--  Screen shake effect via periodic random offsets.
--  Spec [10.2.42]: blocks during quake, click suppressed.
--  Blocks coroutine until complete. Cancelable.
-- ═══════════════════════════════════════════════════════════════════════════

function TransCommands.quake(ctx, params)
    local dur       = tonumber(params.time or params.duration or 300)
    local intensity = tonumber(params.intensity or params.amplitude or 5)
    local freq      = tonumber(params.freq or params.frequency or 0.05)

    if dur <= 0 then return end

    local ct = CancelToken.new()
    ctx.active_operations = ctx.active_operations or {}
    table.insert(ctx.active_operations, ct)

    local elapsed = 0
    while elapsed < dur and not ct.cancelled do
        local dt = coroutine.yield() or 16
        elapsed = elapsed + dt

        -- Decaying amplitude
        local progress = elapsed / dur
        local decay = 1.0 - progress  -- linear decay
        local curIntensity = intensity * decay

        -- Random offset with current intensity
        local ox = (math.random() - 0.5) * 2 * curIntensity
        local oy = (math.random() - 0.5) * 2 * curIntensity
        backend.set_screen_offset(ox, oy)
    end

    -- Reset to origin
    backend.set_screen_offset(0, 0)

    for i = #ctx.active_operations, 1, -1 do
        if ctx.active_operations[i] == ct then
            table.remove(ctx.active_operations, i)
            break
        end
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [fade layer="bg" time=1000 from=255 to=0]
--  Fade a layer's opacity over time. Blocks coroutine. Cancelable.
--  Default: fade in (0→255). from/to control direction.
-- ═══════════════════════════════════════════════════════════════════════════

function TransCommands.fade(ctx, params)
    local dur       = tonumber(params.time or params.duration or 500)
    local layerName = params.layer or "fg"
    local from      = tonumber(params.from) or 0
    local to        = tonumber(params.to)   or 255

    if dur <= 0 then return end

    local node = layers.find(ctx, layerName)
    if not node then
        print("[TransCmd] fade: layer not found: " .. layerName)
        return
    end

    local ct = CancelToken.new()
    ctx.active_operations = ctx.active_operations or {}
    table.insert(ctx.active_operations, ct)

    local elapsed = 0
    while elapsed < dur and not ct.cancelled do
        local dt = coroutine.yield() or 16
        elapsed = elapsed + dt
        local t = math.min(elapsed / dur, 1.0)
        local curOpacity = from + (to - from) * t
        layers.set_layer_opacity(node, curOpacity)
    end

    -- Snap to final opacity
    layers.set_layer_opacity(node, to)

    for i = #ctx.active_operations, 1, -1 do
        if ctx.active_operations[i] == ct then
            table.remove(ctx.active_operations, i)
            break
        end
    end
end

return TransCommands