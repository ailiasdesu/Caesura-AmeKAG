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


-- ═══════════════════════════════════════════════════════════════════════════
--  [10.2.25] LUT (Look-Up Table) cache — one texture, progress uniform
--  Pre-generate easing LUT textures; never create per-frame.
--  Shader uses progress uniform + hardcoded easing to interpolate.
-- ═══════════════════════════════════════════════════════════════════════════

local LUTCache = {
    _textures = {},   -- easing_name → texture handle
    _generated = false,
}

--- Pre-generate all LUT textures at init (called once from kag/init.lua)
function LUTCache.pregen()
    if LUTCache._generated then return end
    -- Standard easing LUTs: 256x1 R8 textures
    -- Each texel = precomputed easing(t/255)
    local easings = { "linear", "ease_in", "ease_out", "ease_in_out",
                      "cubic_in", "cubic_out", "cubic_in_out",
                      "quart_in", "quart_out", "back_in", "back_out", "elastic_out" }
    for _, name in ipairs(easings) do
        local tex = backend.create_lut_texture and backend.create_lut_texture(name)
        if tex then
            LUTCache._textures[name] = tex
        end
    end
    LUTCache._generated = true
    print("[LUT] Pre-generated " .. #easings .. " easing LUT textures.")
end

--- Get LUT texture handle by easing name (cached, no per-frame creation)
function LUTCache.get(name)
    return LUTCache._textures[name or "linear"]
end

--- Release all LUT textures
function LUTCache.release()
    for name, tex in pairs(LUTCache._textures) do
        if tex then backend.destroy_texture(tex) end
    end
    LUTCache._textures = {}
    LUTCache._generated = false
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [10.2.41] Cubic Bezier easing — control point interpolation
--  P0=(0,0), P1=(cp1x,cp1y), P2=(cp2x,cp2y), P3=(1,1)
--  Evaluates Y at given X using de Casteljau / Newton-Raphson.
-- ═══════════════════════════════════════════════════════════════════════════

local Bezier = {}

--- Evaluate cubic bezier at parameter t
--- cp1x,cp1y: first control point (relative to start 0,0)
--- cp2x,cp2y: second control point (relative to end 1,1)
--- Returns (x, y) at parameter t
function Bezier.eval(cp1x, cp1y, cp2x, cp2y, t)
    local u = 1 - t
    local tt = t * t
    local uu = u * u
    local uuu = uu * u
    local ttt = tt * t
    local x = uuu * 0 + 3 * uu * t * cp1x + 3 * u * tt * cp2x + ttt * 1
    local y = uuu * 0 + 3 * uu * t * cp1y + 3 * u * tt * cp2y + ttt * 1
    return x, y
end

--- Solve bezier Y at given X using Newton-Raphson (8 iterations)
function Bezier.solve_y(cp1x, cp1y, cp2x, cp2y, x)
    local t = x  -- initial guess
    for _ = 1, 8 do
        local bx, by = Bezier.eval(cp1x, cp1y, cp2x, cp2y, t)
        -- Derivative dx/dt
        local u = 1 - t
        local dx = 3 * u * u * cp1x + 6 * u * t * (cp2x - cp1x) + 3 * t * t * (1 - cp2x)
        if math.abs(dx) < 0.0001 then break end
        t = t - (bx - x) / dx
        t = math.max(0, math.min(1, t))
    end
    local _, y = Bezier.eval(cp1x, cp1y, cp2x, cp2y, t)
    return y
end

-- Preset bezier curves matching common CSS easing
Bezier.PRESETS = {
    ease        = { 0.25, 0.1, 0.25, 1.0 },
    ["ease-in"] = { 0.42, 0.0, 1.0,  1.0 },
    ["ease-out"]= { 0.0,  0.0, 0.58, 1.0 },
    ["ease-in-out"] = { 0.42, 0.0, 0.58, 1.0 },
    linear      = { 0.0, 0.0, 1.0, 1.0 },
}

--- Apply a named bezier preset to a progress value t
function Bezier.apply(preset_name, t)
    local p = Bezier.PRESETS[preset_name or "linear"]
    if not p then return t end
    return Bezier.solve_y(p[1], p[2], p[3], p[4], t)
end


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

TransCommands.LUTCache = LUTCache
TransCommands.Bezier = Bezier

return TransCommands
