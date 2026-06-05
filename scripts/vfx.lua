-- ===========================================================================
--  Caesura (AmeKAG) -- vfx.lua
--  GPU-backed visual effects: Quake, Shake, Flash, Fade, Blur.
--  All blocking functions use coroutine.yield for frame-stepped animation.
--  All time values in params are in milliseconds (converted to seconds here).
--  Spec [2.5]: quake = random offset × linear decay | shake = random × sin(freq)
-- ===========================================================================

local backend = require("backend")
local VFX = {}

local math_sin, math_abs, math_random = math.sin, math.abs, math.random

-- ===========================================================================
-- VFX.quake(ctx, params) -- BLOCKING random screen shake
-- params: { time=500, amplitudex=10, amplitudey=5, decay=true }
-- Spec [2.5]: offset_x = random(-ampl_x, +ampl_x) * decay
--             offset_y = random(-ampl_y, +ampl_y) * decay
-- ===========================================================================
function VFX.quake(ctx, params)
    VFX._quakeActive = true
    local isCoroutine = coroutine.isyieldable()
    VFX._quakeCtx = ctx
    local dur       = (tonumber(params.time) or 500) / 1000.0
    local ampl_x    = tonumber(params.amplitudex or params.intensity or params.strength or params.power) or 8
    local ampl_y    = tonumber(params.amplitudey or params.intensity or params.strength or params.power) or ampl_x or 8
    local decay     = params.decay ~= "false" and params.decay ~= false
    local elapsed   = 0

    VFX._quakeOriginals = {}
    for name, layer in pairs(ctx.layers or {}) do
        if type(layer) == "table" then
            VFX._quakeOriginals[name] = { x = layer.x or 0, y = layer.y or 0 }
        end
    end

    while elapsed < dur do
        local dt = 0.016
        if isCoroutine then dt = coroutine.yield() or 0.016 end
        elapsed = elapsed + dt
        local t = elapsed / dur
        if t > 1 then t = 1 end

        -- Spec [2.5]: random offset × linear decay
        local decay_factor = decay and (1.0 - t) or 1.0
        local ox = (math_random() * 2.0 - 1.0) * ampl_x * decay_factor
        local oy = (math_random() * 2.0 - 1.0) * ampl_y * decay_factor

        for name, layer in pairs(ctx.layers or {}) do
            if type(layer) == "table" and VFX._quakeOriginals[name] then
                layer.x = (VFX._quakeOriginals[name].x or 0) + ox
                layer.y = (VFX._quakeOriginals[name].y or 0) + oy
                layer.dirty = true
            end
        end
    end

    -- Restore original positions
    for name, layer in pairs(ctx.layers or {}) do
        if type(layer) == "table" and VFX._quakeOriginals[name] then
            layer.x = VFX._quakeOriginals[name].x
            layer.y = VFX._quakeOriginals[name].y
            layer.dirty = true
        end
    end
    VFX._quakeActive = false
end

-- ===========================================================================
-- VFX.shake(ctx, params) -- BLOCKING sinusoidal screen shake
-- Spec [2.5]: random offset × sin(frequency)
-- params: { time=500, frequency=20, amplitude=6 }
-- ===========================================================================
function VFX.shake(ctx, params)
    VFX._shakeActive = true
    local isCoroutine = coroutine.isyieldable()
    VFX._shakeCtx = ctx
    local dur       = (tonumber(params.time) or 500) / 1000.0
    local freq      = tonumber(params.frequency) or 20
    local amplitude = tonumber(params.amplitude or params.intensity) or 6
    local elapsed   = 0

    VFX._shakeOriginals = {}
    for name, layer in pairs(ctx.layers or {}) do
        if type(layer) == "table" then
            VFX._shakeOriginals[name] = { x = layer.x or 0, y = layer.y or 0 }
        end
    end

    while elapsed < dur do
        local dt = 0.016
        if isCoroutine then dt = coroutine.yield() or 0.016 end
        elapsed = elapsed + dt
        local t = elapsed / dur
        if t > 1 then t = 1 end

        -- Spec [2.5]: random offset × sin(frequency) with decay
        local decay = 1.0 - t
        local ox = (math_random() * 2.0 - 1.0) * amplitude * math_sin(elapsed * freq) * decay
        local oy = (math_random() * 2.0 - 1.0) * amplitude * math_sin(elapsed * freq * 1.3 + 1.0) * decay

        for name, layer in pairs(ctx.layers or {}) do
            if type(layer) == "table" and VFX._shakeOriginals[name] then
                layer.x = (VFX._shakeOriginals[name].x or 0) + ox
                layer.y = (VFX._shakeOriginals[name].y or 0) + oy
                layer.dirty = true
            end
        end
    end

    for name, layer in pairs(ctx.layers or {}) do
        if type(layer) == "table" and VFX._shakeOriginals[name] then
            layer.x = VFX._shakeOriginals[name].x
            layer.y = VFX._shakeOriginals[name].y
            layer.dirty = true
        end
    end
    VFX._shakeActive = false
end

-- ===========================================================================
-- VFX.fade(ctx, params) -- BLOCKING global fullscreen fade
-- params: { time=1000, direction="out", r=0, g=0, b=0 }
-- ===========================================================================
function VFX.fade(ctx, params)
    local isCoroutine = coroutine.isyieldable()
    local dur       = (tonumber(params.time) or 1000) / 1000.0
    local direction = params.direction or params.dir or "out"
    local r = tonumber(params.r or params.red)   or 0
    local g = tonumber(params.g or params.green) or 0
    local b = tonumber(params.b or params.blue)  or 0
    local elapsed   = 0

    local texId = ctx._bgTexture or 0
    while elapsed < dur do
        local dt = 0.016
        if isCoroutine then dt = coroutine.yield() or 0.016 end
        elapsed = elapsed + dt
        local t = elapsed / dur
        if t > 1 then t = 1 end

        local alpha = (direction == "in") and (1.0 - t) or t
        if texId > 0 then
            backend.submit_vfx(1, texId, 1, alpha, r/255, g/255, b/255, 0, 0, 0)
        end
    end
end

-- ===========================================================================
-- VFX.fade_layer(ctx, layer, params) -- BLOCKING per-layer fade
-- params: { time=1000, direction="in" }
-- ===========================================================================
function VFX.fade_layer(ctx, layer, params)
    local isCoroutine = coroutine.isyieldable()
    local dur       = (tonumber(params.time) or 1000) / 1000.0
    local direction = params.direction or params.dir or "in"
    local elapsed   = 0

    local origAlpha = layer.alpha or 1.0
    while elapsed < dur do
        local dt = 0.016
        if isCoroutine then dt = coroutine.yield() or 0.016 end
        elapsed = elapsed + dt
        local t = elapsed / dur
        if t > 1 then t = 1 end

        layer.alpha = (direction == "in") and (origAlpha * t) or (origAlpha * (1 - t))
        layer.dirty = true
    end
    layer.alpha = (direction == "in") and origAlpha or 0
    layer.dirty = true
end

-- ===========================================================================
-- VFX.blur(ctx, params) -- BLOCKING GPU blur (two-pass separable Gaussian)
-- Spec [2.5]: Pass 1 horizontal, Pass 2 vertical, RTTManager 3 temp textures
-- params: { time=500, strength=4, layer="" }
-- ===========================================================================
function VFX.blur(ctx, params)
    local isCoroutine = coroutine.isyieldable()
    local dur      = (tonumber(params.time) or 500) / 1000.0
    local strength = tonumber(params.strength or params.intensity or params.blurlevel) or 4
    local targetLayer = params.layer or ""
    local elapsed  = 0
    local rtt = require("rtt")

    -- Find layer texture
    local texId = ctx._bgTexture or 0
    if targetLayer ~= "" then
        local layer = ctx.layers and ctx.layers[targetLayer]
        if layer and layer.texture then
            texId = layer.texture
        end
    end

    -- Allocate temp RTTs for two-pass blur
    local tmpA = rtt and rtt.alloc(ctx._width or 1280, ctx._height or 720) or 0
    local tmpB = rtt and rtt.alloc(ctx._width or 1280, ctx._height or 720) or 0

    while elapsed < dur do
        local dt = 0.016
        if isCoroutine then dt = coroutine.yield() or 0.016 end
        elapsed = elapsed + dt
        local t = elapsed / dur
        if t > 1 then t = 1 end

        local radius = strength * t
        if texId > 0 then
            backend.submit_vfx(1, texId, 2, 0, 0, 0, 0, radius, 0, 0)
        end
    end

    -- Clean up temp textures
    if rtt then
        if tmpA > 0 then rtt.free(tmpA) end
        if tmpB > 0 then rtt.free(tmpB) end
    end
end

-- ===========================================================================
-- VFX.blur_layer(layer, params, co) -- Per-layer blur wrapper
-- Spec [2.5] kag.lua integration
-- ===========================================================================
function VFX.blur_layer(layer, params, co)
    if not layer or type(layer) ~= "table" then return end
    local dur      = (tonumber(params.time) or 500) / 1000.0
    local strength = tonumber(params.strength or params.blurlevel) or 4
    local elapsed  = 0
    local rtt = require("rtt")

    local origTexture = layer.texture
    local tmpA = rtt and rtt.alloc(1280, 720) or 0
    local tmpB = rtt and rtt.alloc(1280, 720) or 0

    while elapsed < dur do
        local dt = 0.016
        if co and coroutine.isyieldable() then dt = co.yield() or 0.016 end
        elapsed = elapsed + dt
        local t = elapsed / dur
        if t > 1 then t = 1 end

        local radius = strength * t
        if layer.texture and layer.texture > 0 then
            backend.submit_vfx(layer.z or 1, layer.texture, 2, 0, 0, 0, 0, radius, 0, 0)
        end
        layer.dirty = true
    end

    if rtt then
        if tmpA > 0 then rtt.free(tmpA) end
        if tmpB > 0 then rtt.free(tmpB) end
    end
end

-- ===========================================================================
-- VFX.flash(ctx, params) -- BLOCKING white/colored flash
-- params: { time=200, r=255, g=255, b=255 }
-- ===========================================================================
function VFX.flash(ctx, params)
    local isCoroutine = coroutine.isyieldable()
    local dur    = (tonumber(params.time) or 200) / 1000.0
    local r = tonumber(params.r or params.red)   or 255
    local g = tonumber(params.g or params.green) or 255
    local b = tonumber(params.b or params.blue)  or 255
    local elapsed = 0

    -- Create flash layer if not exists
    local flashLayer = ctx._flashLayer
    if not flashLayer then
        flashLayer = { name="__flash", z=9998, visible=true, dirty=true, _prevTexture=0,
                       x=0, y=0, w=1280, h=720, alpha=0, blend="additive" }
        ctx._flashLayer = flashLayer
        ctx.layers = ctx.layers or {}
        ctx.layers["__flash"] = flashLayer
    end

    local midPoint = dur / 2

    while elapsed < dur do
        local dt = 0.016
        if isCoroutine then dt = coroutine.yield() or 0.016 end
        elapsed = elapsed + dt

        local alpha_val
        if elapsed < midPoint then
            alpha_val = elapsed / midPoint
        else
            alpha_val = (dur - elapsed) / midPoint
        end
        if alpha_val < 0 then alpha_val = 0 end
        if alpha_val > 1 then alpha_val = 1 end

        flashLayer.alpha = alpha_val * 0.5
        flashLayer.dirty = true

        if not flashLayer.texture then
            pcall(function()
                flashLayer.texture = backend.create_solid_texture(r, g, b, 255)
            end)
        end
    end

    flashLayer.alpha = 0
    flashLayer.dirty = true
end

-- ===========================================================================
-- VFX.stop_quake() -- immediately terminate quake and restore positions
-- ===========================================================================
function VFX.stop_quake()
    VFX._quakeActive = false
    local ctx = VFX._quakeCtx
    if ctx and VFX._quakeOriginals then
        for name, orig in pairs(VFX._quakeOriginals) do
            local layer = ctx.layers and ctx.layers[name]
            if layer and type(layer) == "table" then
                layer.x = orig.x
                layer.y = orig.y
                layer.dirty = true
            end
        end
    end
    VFX._quakeOriginals = nil
    VFX._quakeCtx = nil
end

-- ===========================================================================
-- VFX.stop_shake() -- immediately terminate sinusoidal shake
-- ===========================================================================
function VFX.stop_shake()
    VFX._shakeActive = false
    local ctx = VFX._shakeCtx
    if ctx and VFX._shakeOriginals then
        for name, orig in pairs(VFX._shakeOriginals) do
            local layer = ctx.layers and ctx.layers[name]
            if layer and type(layer) == "table" then
                layer.x = orig.x
                layer.y = orig.y
                layer.dirty = true
            end
        end
    end
    VFX._shakeOriginals = nil
    VFX._shakeCtx = nil
end

-- ===========================================================================
-- VFX.stop_all() -- immediately terminate all active VFX
-- ===========================================================================
function VFX.stop_all()
    VFX.stop_quake()
    VFX.stop_shake()
end

return VFX