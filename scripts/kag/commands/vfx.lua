-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/vfx.lua
--  VFX command handler: [vfx type="..." ...]
--  Routes to particle system, quake/shake/flash/fade/blur via vfx.lua module.
-- =============================================================================

local VFX = require("vfx")
local backend = require("backend")

local VFXCommands = {}

-- ═══════════════════════════════════════════════════════════════════════════
--  [vfx type="particle" action="create" x=0 y=0 rate=10 lifeMin=0.5 lifeMax=2.0]
--  [vfx type="particle" action="emit" emitter=0 count=10]
--  [vfx type="particle" action="destroy" emitter=0]
--  [vfx type="particle" action="clear"]
--  [vfx type="quake" time=500 amplitudex=10 amplitudey=5]
--  [vfx type="shake" time=500 frequency=20 amplitude=6]
--  [vfx type="flash" time=200 r=255 g=255 b=255]
--  [vfx type="fade" time=500 r=0 g=0 b=0]
--  [vfx type="blur" time=500 strength=4]
--  [palette effect="apply|clear|day|night|toggle|unload" ...]  -- LUT color grading
--  [vibrate time=300 intensity=3 ...]  -- KAG3 alias for [vib]
-- ═══════════════════════════════════════════════════════════════════════════

-- Neo-Genesis contracts: typed + clamped via kag/schema.
local schema = require("kag.schema")
-- Standalone [flash]/[shake]/[quake] (KAG3 names; routed to VFX).
schema.define("flash", {
    _meta = { category = "vfx", blocking = true, desc = "KAG3-compatible flash command" },
    r = { type = "number", default = 255, min = 0, max = 255 },
    g = { type = "number", default = 255, min = 0, max = 255 },
    b = { type = "number", default = 255, min = 0, max = 255 },
    time = { type = "number", default = 200, min = 0, max = 10000 },
})
schema.define("shake", {
    _meta = { category = "vfx", blocking = true, desc = "KAG3-compatible shake command" },
    time = { type = "number", default = 500, min = 0, max = 10000 },
    frequency = { type = "number", default = 20, min = 1, max = 120 },
    amplitude = { type = "number", default = 6, min = 0, max = 100 },
})
-- NOTE: [quake] has NO schema here -- transition.lua owns the quake
-- contract (time/duration/intensity/amplitude); re-defining it would
-- silently override that richer contract (schema.define overwrites).
schema.define("particles", {
    _meta = { category = "vfx", blocking = false, desc = "KAG3-compatible particles command" },
    x = { type = "number", default = 0 },
    y = { type = "number", default = 0 },
    rate = { type = "number", default = 10, min = 0, max = 1000 },
    lifeMin = { type = "number", default = 0.5, min = 0, max = 60 },
    life_max = { type = "number", default = 0.5, min = 0, max = 60 },
    lifeMax = { type = "number", default = 2.0, min = 0, max = 60 },
    speedMin = { type = "number", default = 10, min = 0, max = 10000 },
    speed_max = { type = "number", default = 10, min = 0, max = 10000 },
    speedMax = { type = "number", default = 50, min = 0, max = 10000 },
    angleMin = { type = "number", default = 0 },
    angle_max = { type = "number", default = 0 },
    angleMax = { type = "number", default = 6.283 },
    sizeMin = { type = "number", default = 2, min = 0, max = 512 },
    size_max = { type = "number", default = 2, min = 0, max = 512 },
    sizeMax = { type = "number", default = 8, min = 0, max = 512 },
    r = { type = "number", default = 1, min = 0, max = 255 },
    red = { type = "number", default = 1, min = 0, max = 255 },
    g = { type = "number", default = 1, min = 0, max = 255 },
    green = { type = "number", default = 1, min = 0, max = 255 },
    b = { type = "number", default = 1, min = 0, max = 255 },
    blue = { type = "number", default = 1, min = 0, max = 255 },
    a = { type = "number", default = 1, min = 0, max = 255 },
    alpha = { type = "number", default = 1, min = 0, max = 255 },
})

function VFXCommands.vfx(ctx, params)
    local vtype = params.type or params.effect or "particle"

    -- ── Particle effects ────────────────────────────────────────────────
    if vtype == "particle" then
        local action = params.action or "create"

        if action == "create" then
            local cfg = {
                x        = params.x or 0,
                y        = params.y or 0,
                rate     = params.rate or 10,
                lifeMin  = params.lifeMin or params.life_min or 0.5,
                lifeMax  = params.lifeMax or params.life_max or 2.0,
                speedMin = params.speedMin or params.speed_min or 10,
                speedMax = params.speedMax or params.speed_max or 50,
                angleMin = params.angleMin or params.angle_min or 0,
                angleMax = params.angleMax or params.angle_max or 6.283,
                sizeMin  = params.sizeMin or params.size_min or 2,
                sizeMax  = params.sizeMax or params.size_max or 8,
                r = params.r or params.red or 1,
                g = params.g or params.green or 1,
                b = params.b or params.blue or 1,
                a = params.a or params.alpha or 1,
                gravityX = tonumber(params.gravityX or params.gravity_x) or 0,
                gravityY = tonumber(params.gravityY or params.gravity_y) or 0,
            }
            local id = backend.particles_create_emitter(cfg)
            ctx._particleEmitters = ctx._particleEmitters or {}
            ctx._particleEmitters[id] = true

        elseif action == "emit" then
            local emitter = tonumber(params.emitter or params.id) or 0
            local count   = tonumber(params.count) or 1
            backend.particles_emit(emitter, count)

        elseif action == "destroy" then
            local emitter = tonumber(params.emitter or params.id) or 0
            backend.particles_destroy_emitter(emitter)
            if ctx._particleEmitters then
                ctx._particleEmitters[emitter] = nil
            end

        elseif action == "clear" then
            backend.clear_particles()
            ctx._particleEmitters = {}
        end

    -- ── Quake ───────────────────────────────────────────────────────────
    elseif vtype == "quake" then
        VFX.quake(ctx, params)

    -- ── Shake ───────────────────────────────────────────────────────────
    elseif vtype == "shake" then
        VFX.shake(ctx, params)

    -- ── Flash ───────────────────────────────────────────────────────────
    elseif vtype == "flash" then
        VFX.flash(ctx, params)

    -- ── Fade ────────────────────────────────────────────────────────────
    elseif vtype == "fade" then
        VFX.fade(ctx, params)

    -- ── Blur ────────────────────────────────────────────────────────────
    elseif vtype == "blur" then
        VFX.blur(ctx, params)

    -- ── Stop all ────────────────────────────────────────────────────────
    elseif vtype == "stop" then
        VFX.stop_all()
        -- Also clear particles
        if ctx._particleEmitters then
            for id, _ in pairs(ctx._particleEmitters) do
                pcall(backend.particles_destroy_emitter, id)
            end
            ctx._particleEmitters = {}
        end
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Standalone command aliases (KAG3 names, Neo-Genesis contracts).
-- ═══════════════════════════════════════════════════════════════════════════
function VFXCommands.flash(ctx, params)
    VFX.flash(ctx, params)
end

function VFXCommands.shake(ctx, params)
    VFX.shake(ctx, params)
end

function VFXCommands.quake(ctx, params)
    VFX.quake(ctx, params)
end

-- [particles action="create" x=0 y=0 rate=10 ...] -- standalone form
-- of [vfx type="particle" ...] (schema-vs-handler audit: the schema
-- existed with no handler key; the scheduler fallback rendered
-- 'particles' as dialogue). Same dispatch as the vfx wrapper.
function VFXCommands.particles(ctx, params)
    local action = params.action or "create"
    if action == "create" then
        local cfg = {
            x = tonumber(params.x) or 0,
            y = tonumber(params.y) or 0,
            rate = tonumber(params.rate) or 10,
            lifeMin = tonumber(params.lifeMin or params.life_min) or 0.5,
            lifeMax = tonumber(params.lifeMax or params.life_max) or 2.0,
            speedMin = tonumber(params.speedMin or params.speed_min) or 10,
            speedMax = tonumber(params.speedMax or params.speed_max) or 50,
            sizeMin = tonumber(params.sizeMin or params.size_min) or 2,
            sizeMax = tonumber(params.sizeMax or params.size_max) or 8,
            angleMin = tonumber(params.angleMin or params.angle_min) or 0,
            angleMax = tonumber(params.angleMax or params.angle_max) or 6.283,
            r = tonumber(params.r or params.red) or 1,
            g = tonumber(params.g or params.green) or 1,
            b = tonumber(params.b or params.blue) or 1,
            a = tonumber(params.a or params.alpha) or 1,
            gravityX = tonumber(params.gravityX or params.gravity_x) or 0,
            gravityY = tonumber(params.gravityY or params.gravity_y) or 0,
        }
        local id = backend.particles_create_emitter(cfg)
        ctx._particleEmitters = ctx._particleEmitters or {}
        ctx._particleEmitters[id] = true
    elseif action == "emit" then
        local emitter = tonumber(params.emitter or params.id) or 0
        local count = tonumber(params.count) or 1
        backend.particles_emit(emitter, count)
    elseif action == "destroy" then
        local emitter = tonumber(params.emitter or params.id) or 0
        backend.particles_destroy_emitter(emitter)
        if ctx._particleEmitters then ctx._particleEmitters[emitter] = nil end
    elseif action == "clear" then
        -- the backend name is clear_particles (not particles_clear)
        backend.clear_particles()
        if ctx._particleEmitters then ctx._particleEmitters = {} end
    end
end


-- ═══════════════════════════════════════════════════════════════════════
--  [palette effect="apply|clear|day|night|toggle|unload" ...]
--  Standalone LUT color-grading command routing to scripts/palette.lua.
--  The palette module drives the C++ backend through the GLOBAL backend
--  (backend.load_image / backend.is_valid / backend.set_palette /
--  backend.destroy_texture) -- it is not require()-bound.
--    effect="apply" id="<lut>" path="assets/lut/<name>.png" intensity=0..1
--        -> loads (when path given) then applies the LUT; without a path
--           re-applies a previously loaded id
--    effect="clear"     -> disable the active palette
--    effect="day"       -> neutral grading (clears any active LUT)
--    effect="night"     -> blue-dark grading (loads assets/lut/night.png)
--    effect="toggle"    -> day <-> night
--    effect="unload" id="<lut>" -> unload a cached LUT from memory
-- ═══════════════════════════════════════════════════════════════════════
schema.define("palette", {
    _meta = { category = "vfx", blocking = false, desc = "KAG3-compatible palette/LUT color-grading command" },
    effect = { type = "enum", values = {
        ["apply"] = true, ["clear"] = true, ["day"] = true,
        ["night"] = true, ["toggle"] = true, ["unload"] = true,
    }, default = "apply" },
    id        = { type = "string", default = "" },
    path      = { type = "string", default = "" },
    intensity = { type = "number", default = 1.0, min = 0.0, max = 1.0 },
})

function VFXCommands.palette(ctx, params)
    local effect = params.effect or "apply"
    local paletteModule = require("palette")
    local id = params.id or ""
    if effect == "apply" then
        -- Load-then-apply: a path means (id, path) is a fresh LUT.
        if params.path and #params.path > 0 then
            local ok, err = paletteModule.load(id, params.path)
            if not ok then
                print("[palette] " .. tostring(err))
                return
            end
        end
        -- Apply whatever is loaded under id (no-op if the id is empty).
        if id and #id > 0 then
            paletteModule.apply(id, params.intensity)
        end
    elseif effect == "clear" then
        paletteModule.clear()
    elseif effect == "day" then
        paletteModule.set_day_mode()
    elseif effect == "night" then
        paletteModule.set_night_mode()
    elseif effect == "toggle" then
        paletteModule.toggle_mode()
    elseif effect == "unload" then
        if id and #id > 0 then
            paletteModule.unload(id)
        end
    end
end

-- [vibrate] -- KAG3-compatible alias for [vib] (message-layer vibration).
-- [vib] is owned by transition.lua; this alias carries the SAME schema
-- contract and forwards verbatim so KAG3 scripts written with either tag
-- name behave identically. Lazy require keeps kag.lua's module load order
-- independent of the transition table.
schema.define("vibrate", {
    _meta = { category = "vfx", blocking = true, desc = "KAG3-compatible alias for [vib] (message-layer vibration)" },
    time      = { type = "number", default = 300, min = 0, max = 30000 },
    intensity = { type = "number", default = 3, min = 0, max = 50 },
    amplitude = { type = "number", min = 0, max = 50 },
})

function VFXCommands.vibrate(ctx, params)
    local trans = require("kag.commands.transition")
    if trans.vib then
        trans.vib(ctx, params)
    end
end

return VFXCommands