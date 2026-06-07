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

function VFXCommands.vfx(ctx, params)
    local vtype = params.type or params.effect or "particle"

    -- ── Particle effects ────────────────────────────────────────────────
    if vtype == "particle" then
        local action = params.action or "create"

        if action == "create" then
            local cfg = {
                x        = tonumber(params.x) or 0,
                y        = tonumber(params.y) or 0,
                rate     = tonumber(params.rate) or 10,
                lifeMin  = tonumber(params.lifeMin or params.life_min) or 0.5,
                lifeMax  = tonumber(params.lifeMax or params.life_max) or 2.0,
                speedMin = tonumber(params.speedMin or params.speed_min) or 10,
                speedMax = tonumber(params.speedMax or params.speed_max) or 50,
                angleMin = tonumber(params.angleMin or params.angle_min) or 0,
                angleMax = tonumber(params.angleMax or params.angle_max) or 6.283,
                sizeMin  = tonumber(params.sizeMin or params.size_min) or 2,
                sizeMax  = tonumber(params.sizeMax or params.size_max) or 8,
                r = tonumber(params.r or params.red)   or 1,
                g = tonumber(params.g or params.green) or 1,
                b = tonumber(params.b or params.blue)  or 1,
                a = tonumber(params.a or params.alpha) or 1,
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