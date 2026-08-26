-- test_particle_weather.lua — Unit tests for declarative weather particle system [particle_weather]
package.path = "scripts/?.lua;scripts/kag/?.lua;tests/scripts/?.lua;" .. package.path

local passed, failed = 0, 0
local function check(name, cond)
    if cond then
        print("PASS " .. name)
        passed = passed + 1
    else
        print("FAIL " .. name)
        failed = failed + 1
    end
end

local schema = require("kag.schema")
local VFXCommands = require("kag.commands.vfx")
local KAG = require("kag")
local backend = require("backend")

-- ═══════════════════════════════════════════════════════════════════════
-- 1. Schema Registration & Coercion Tests
-- ═══════════════════════════════════════════════════════════════════════
check("particle_weather schema is registered", schema.isMigrated("particle_weather"))

-- Defaults.
-- type has NO schema default on purpose: a missing type means "rain" for
-- action="start" (resolved in the handler) but "every weather type" for
-- action="stop". A schema default of "rain" made [particle_weather
-- action="stop"] unable to express stop-all through the real KAG dispatch.
local defP = schema.coerce("particle_weather", {}, {})
check("type has no schema default (stop-all stays expressible)", defP.type == nil)
check("default action is start", defP.action == "start")
check("default count is 10", defP.count == 10)
check("default wind is 0", defP.wind == 0)
check("default speed is 1.0", defP.speed == 1.0)
check("default intensity is 1.0", defP.intensity == 1.0)

-- Valid types
for _, t in ipairs({ "rain", "snow", "sakura", "dust" }) do
    local p = schema.coerce("particle_weather", { type = t }, {})
    check("accepts type=" .. t, p.type == t)
end

-- Invalid type throws
local ok, err = pcall(function()
    schema.coerce("particle_weather", { type = "tornado" }, {})
end)
check("invalid type throws", not ok)

-- Valid actions
for _, a in ipairs({ "start", "stop", "clear" }) do
    local p = schema.coerce("particle_weather", { action = a }, {})
    check("accepts action=" .. a, p.action == a)
end

-- Invalid action throws
ok, err = pcall(function()
    schema.coerce("particle_weather", { action = "pause" }, {})
end)
check("invalid action throws", not ok)

-- Clamping tests
local clamped = schema.coerce("particle_weather", {
    count = "1000",
    wind = "25",
    speed = "50",
    intensity = "10",
}, {})
check("count clamped to 500", clamped.count == 500)
check("wind clamped to 10", clamped.wind == 10)
check("speed clamped to 10.0", clamped.speed == 10.0)
check("intensity clamped to 5.0", clamped.intensity == 5.0)

local minClamped = schema.coerce("particle_weather", {
    count = "-10",
    wind = "-30",
    speed = "-5",
    intensity = "-2",
}, {})
check("count min clamped to 0", minClamped.count == 0)
check("wind min clamped to -10", minClamped.wind == -10)
check("speed min clamped to 0.1", minClamped.speed == 0.1)
check("intensity min clamped to 0.0", minClamped.intensity == 0.0)

-- ═══════════════════════════════════════════════════════════════════════
-- 2. Mock Backend & Weather Preset Mapping Tests
-- ═══════════════════════════════════════════════════════════════════════
local createdEmitters = {}
local emittedCalls = {}
local destroyedEmitters = {}
local clearedParticles = 0
local nextEmitterId = 1

local orig_create = backend.particles_create_emitter
local orig_destroy = backend.particles_destroy_emitter
local orig_emit = backend.particles_emit
local orig_clear = backend.clear_particles

backend.particles_create_emitter = function(cfg)
    local id = nextEmitterId
    nextEmitterId = nextEmitterId + 1
    createdEmitters[id] = cfg
    return id
end
backend.particles_destroy_emitter = function(id)
    destroyedEmitters[id] = true
    createdEmitters[id] = nil
    return true
end
backend.particles_emit = function(id, count)
    emittedCalls[#emittedCalls + 1] = { id = id, count = count }
    return true
end
backend.clear_particles = function()
    clearedParticles = clearedParticles + 1
    createdEmitters = {}
    return true
end

-- Test 'rain' preset configuration
do
    createdEmitters = {}; emittedCalls = {}; destroyedEmitters = {}
    local ctx = {}
    local p = schema.coerce("particle_weather", { type = "rain", count = 20, wind = 2, speed = 1.2 }, ctx)
    local id = VFXCommands.particle_weather(ctx, p)
    check("rain emitter created", id ~= nil and createdEmitters[id] ~= nil)
    local cfg = createdEmitters[id]
    check("rain has downward gravityY >= 500", cfg.gravityY >= 500)
    check("rain has high speed >= 600", cfg.speedMin >= 600 * 1.2)
    check("rain has downward angle ~1.57 rad", cfg.angleMin >= 1.4 and cfg.angleMax <= 1.7)
    check("rain wind affects gravityX", cfg.gravityX == 2 * 40)
    check("rain particle color blue-tinted", cfg.b > cfg.r)
    check("rain tracked in ctx._weatherEmitters", ctx._weatherEmitters and ctx._weatherEmitters.rain == id)
    check("rain initial burst emitted", #emittedCalls > 0 and emittedCalls[1].id == id)
end

-- Test 'snow' preset configuration
do
    createdEmitters = {}; emittedCalls = {}; destroyedEmitters = {}
    local ctx = {}
    local p = schema.coerce("particle_weather", { type = "snow", count = 15, wind = -3, speed = 1.0 }, ctx)
    local id = VFXCommands.particle_weather(ctx, p)
    check("snow emitter created", id ~= nil and createdEmitters[id] ~= nil)
    local cfg = createdEmitters[id]
    check("snow has gentle gravityY <= 50", cfg.gravityY <= 50)
    check("snow has slow speedMin <= 50", cfg.speedMin <= 50)
    check("snow has white color r=1,g=1,b=1", cfg.r == 1.0 and cfg.g == 1.0 and cfg.b == 1.0)
    check("snow negative wind affects gravityX", cfg.gravityX == -3 * 35)
    check("snow longer lifespan >= 3.0s", cfg.lifeMin >= 3.0)
end

-- Test 'sakura' preset configuration
do
    createdEmitters = {}; emittedCalls = {}; destroyedEmitters = {}
    local ctx = {}
    local p = schema.coerce("particle_weather", { type = "sakura", count = 10, wind = 1 }, ctx)
    local id = VFXCommands.particle_weather(ctx, p)
    check("sakura emitter created", id ~= nil and createdEmitters[id] ~= nil)
    local cfg = createdEmitters[id]
    check("sakura pink color (high r, lower g)", cfg.r > cfg.g and cfg.b > cfg.g)
    check("sakura flutter speed", cfg.speedMin <= 40 and cfg.speedMax <= 100)
    check("sakura size variation", cfg.sizeMin >= 3 and cfg.sizeMax >= 6)
    check("sakura tracked in ctx._weatherEmitters", ctx._weatherEmitters.sakura == id)
end

-- Test 'dust' preset configuration
do
    createdEmitters = {}; emittedCalls = {}; destroyedEmitters = {}
    local ctx = {}
    local p = schema.coerce("particle_weather", { type = "dust", count = 8, intensity = 0.5 }, ctx)
    local id = VFXCommands.particle_weather(ctx, p)
    check("dust emitter created", id ~= nil and createdEmitters[id] ~= nil)
    local cfg = createdEmitters[id]
    check("dust omnidirectional angle 0 to 2pi", cfg.angleMin == 0 and math.abs(cfg.angleMax - 6.283) < 0.01)
    check("dust low ambient speed", cfg.speedMin <= 10 and cfg.speedMax <= 30)
    check("dust subtle alpha <= 0.5", cfg.a <= 0.5)
    check("dust spawned at screen center", cfg.x == 640 and cfg.y == 360)
end

-- ═══════════════════════════════════════════════════════════════════════
-- 3. Action Lifecycle Tests (start, stop, clear, re-start)
-- ═══════════════════════════════════════════════════════════════════════
-- Re-starting same weather type replaces previous emitter
do
    createdEmitters = {}; emittedCalls = {}; destroyedEmitters = {}
    local ctx = {}
    local id1 = VFXCommands.particle_weather(ctx, { type = "rain", action = "start" })
    check("first rain emitter created", id1 ~= nil)
    local id2 = VFXCommands.particle_weather(ctx, { type = "rain", action = "start", wind = 5 })
    check("second rain emitter created", id2 ~= nil and id2 ~= id1)
    check("old rain emitter destroyed on replacement", destroyedEmitters[id1] == true)
    check("ctx._weatherEmitters updated to new id", ctx._weatherEmitters.rain == id2)
end

-- Stopping specific weather type
do
    createdEmitters = {}; emittedCalls = {}; destroyedEmitters = {}
    local ctx = {}
    local idRain = VFXCommands.particle_weather(ctx, { type = "rain", action = "start" })
    local idSnow = VFXCommands.particle_weather(ctx, { type = "snow", action = "start" })
    check("both rain and snow created", ctx._weatherEmitters.rain == idRain and ctx._weatherEmitters.snow == idSnow)

    VFXCommands.particle_weather(ctx, { type = "rain", action = "stop" })
    check("rain destroyed on stop", destroyedEmitters[idRain] == true)
    check("rain removed from ctx._weatherEmitters", ctx._weatherEmitters.rain == nil)
    check("snow remains active", ctx._weatherEmitters.snow == idSnow)
end

-- Stopping all weather
do
    createdEmitters = {}; emittedCalls = {}; destroyedEmitters = {}
    local ctx = {}
    local idRain = VFXCommands.particle_weather(ctx, { type = "rain", action = "start" })
    local idSnow = VFXCommands.particle_weather(ctx, { type = "snow", action = "start" })

    VFXCommands.particle_weather(ctx, { action = "stop" })
    check("all weather emitters destroyed", destroyedEmitters[idRain] == true and destroyedEmitters[idSnow] == true)
    check("ctx._weatherEmitters empty after stop all", next(ctx._weatherEmitters) == nil)
end

-- Clearing weather and all particles
do
    clearedParticles = 0
    local ctx = {}
    VFXCommands.particle_weather(ctx, { type = "sakura", action = "start" })
    VFXCommands.particle_weather(ctx, { action = "clear" })
    check("clear_particles called on clear", clearedParticles == 1)
    check("ctx._weatherEmitters empty after clear", next(ctx._weatherEmitters) == nil)
    check("ctx._particleEmitters empty after clear", next(ctx._particleEmitters) == nil)
end

-- ═══════════════════════════════════════════════════════════════════════
-- 3b. Behaviour through the REAL dispatch path (schema.coerce -> handler)
-- ═══════════════════════════════════════════════════════════════════════
--  The lifecycle tests above call VFXCommands.* with hand-built tables, which
--  skips coercion. The engine always coerces first (scheduler -> schema.coerce
--  -> handler), and that is where a schema default can change behaviour: a
--  default type="rain" turned [particle_weather action="stop"] into "stop the
--  rain only" and left snow/sakura running. These cases go through coerce so
--  the stop-all path is covered as an AUTHOR writes it, not as a test calls it.
do
    createdEmitters = {}; emittedCalls = {}; destroyedEmitters = {}
    local ctx = {}
    local pRain = schema.coerce("particle_weather", { type = "rain", action = "start" }, ctx)
    local idRain = VFXCommands.particle_weather(ctx, pRain)
    local pSnow = schema.coerce("particle_weather", { type = "snow", action = "start" }, ctx)
    local idSnow = VFXCommands.particle_weather(ctx, pSnow)
    local pSak = schema.coerce("particle_weather", { type = "sakura", action = "start" }, ctx)
    local idSak = VFXCommands.particle_weather(ctx, pSak)
    check("coerced start: three distinct emitters",
          idRain and idSnow and idSak
          and idRain ~= idSnow and idSnow ~= idSak)

    -- Bare stop through coercion must reach the stop-ALL branch.
    local pStop = schema.coerce("particle_weather", { action = "stop" }, ctx)
    VFXCommands.particle_weather(ctx, pStop)
    check("coerced bare stop destroys every weather emitter",
          destroyedEmitters[idRain] == true
          and destroyedEmitters[idSnow] == true
          and destroyedEmitters[idSak] == true)
    check("coerced bare stop empties ctx._weatherEmitters",
          next(ctx._weatherEmitters) == nil)
end

do
    -- A TYPED stop through coercion must still be surgical.
    createdEmitters = {}; emittedCalls = {}; destroyedEmitters = {}
    local ctx = {}
    local idRain = VFXCommands.particle_weather(ctx,
        schema.coerce("particle_weather", { type = "rain", action = "start" }, ctx))
    local idSnow = VFXCommands.particle_weather(ctx,
        schema.coerce("particle_weather", { type = "snow", action = "start" }, ctx))
    VFXCommands.particle_weather(ctx,
        schema.coerce("particle_weather", { type = "rain", action = "stop" }, ctx))
    check("coerced typed stop destroys only that type",
          destroyedEmitters[idRain] == true and destroyedEmitters[idSnow] ~= true)
    check("coerced typed stop keeps the other emitter tracked",
          ctx._weatherEmitters.snow == idSnow and ctx._weatherEmitters.rain == nil)
end

do
    -- type="all" is a stop-only selector: starting it must NOT quietly create
    -- a rain emitter through the preset fallback.
    createdEmitters = {}; emittedCalls = {}; destroyedEmitters = {}
    local ctx = {}
    local id = VFXCommands.particle_weather(ctx,
        schema.coerce("particle_weather", { type = "all", action = "start" }, ctx))
    check('type="all" with action="start" creates nothing', id == nil
          and next(createdEmitters) == nil)
end

do
    -- Every field the preset table sets must be a field the C++ binding reads
    -- (src/script/bindings/VFXBinding.cpp lua_VFX_particles_create_emitter ->
    -- ParticleEmitterConfig in src/render/api/IParticleSystem.h). A preset key
    -- that is NOT in this set would be a silently-ignored knob.
    local CONSUMED = {
        x = true, y = true, rate = true, lifeMin = true, lifeMax = true,
        speedMin = true, speedMax = true, angleMin = true, angleMax = true,
        sizeMin = true, sizeMax = true, r = true, g = true, b = true,
        a = true, gravityX = true, gravityY = true,
    }
    createdEmitters = {}
    local ctx = {}
    local unconsumed = {}
    for _, t in ipairs({ "rain", "snow", "sakura", "dust" }) do
        local id = VFXCommands.particle_weather(ctx,
            schema.coerce("particle_weather", { type = t, action = "start" }, ctx))
        local cfg = createdEmitters[id]
        if cfg then
            for k in pairs(cfg) do
                if not CONSUMED[k] then unconsumed[#unconsumed + 1] = t .. "." .. k end
            end
        end
    end
    check("every emitter cfg field reaches ParticleEmitterConfig",
          #unconsumed == 0, table.concat(unconsumed, ","))
end

-- ═══════════════════════════════════════════════════════════════════════
-- 4. KAG Integration Tests
-- ═══════════════════════════════════════════════════════════════════════
check("KAG.particle_weather is bound", type(KAG.particle_weather) == "function")

do
    createdEmitters = {}; emittedCalls = {}; destroyedEmitters = {}
    local ctx = {}
    local id = KAG.particle_weather(ctx, { type = "rain", count = 10, wind = 0 })
    check("KAG.particle_weather creates emitter", id ~= nil and createdEmitters[id] ~= nil)
end

-- Restore backend
backend.particles_create_emitter = orig_create
backend.particles_destroy_emitter = orig_destroy
backend.particles_emit = orig_emit
backend.clear_particles = orig_clear

if failed > 0 then
    print(string.format("TESTS FAILED: %d failures", failed))
    os.exit(1)
end
print("PARTICLE WEATHER TESTS DONE")
