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
-- ═══════════════════════════════════════════════════════════════════════════

-- Next-gen contracts: typed + clamped via kag/schema.
local schema = require("kag.schema")
schema.define("particles", {
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

return VFXCommands