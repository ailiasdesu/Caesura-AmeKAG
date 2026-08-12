-- test_sma.lua — SMA driver tests (Battle 4d S3): JSON parse, bone
-- hierarchy world-pose resolution, animation track LERP, actor lifecycle
-- via a recording mock of the `sma.*` engine binding. GPU drawing is
-- deferred (mock records calls instead).
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

-- Recording mock of the SmaBinding global (resolved lazily by kag.sma).
_G.sma = {
    created = 0,
    destroyed = 0,
    updates = 0,
    draws = 0,
    last_handle = 0,
    create_mesh = function(verts, indices)
        _G.sma.created = _G.sma.created + 1
        _G.sma.last_handle = _G.sma.created
        return _G.sma.created
    end,
    destroy_mesh = function() _G.sma.destroyed = _G.sma.destroyed + 1 end,
    update_mesh = function() _G.sma.updates = _G.sma.updates + 1 end,
    draw_mesh = function() _G.sma.draws = _G.sma.draws + 1 end,
    count = function() return _G.sma.created end,
    initialized = function() return true end,
}

local sma = require("kag.sma")

-- ---------------------------------------------------------------------------
-- 1. JSON parse
-- ---------------------------------------------------------------------------
local asset = sma.load([==[
{
  "texture": "chara/hero.png",
  "bones": [ {"id":0,"parent":-1,"pivot":[1.0,0.0]}, {"id":1,"parent":0,"pivot":[0.0,0.0]} ],
  "mesh": { "positions":[[0,0],[1,0],[1,1]], "uvs":[[0,0],[1,0],[1,1]], "indices":[0,1,2] },
  "animations": { "idle": { "duration":2.0, "tracks":[ {"bone":1,"frames":[{"t":0,"rot":0},{"t":1,"rot":1.5708}]} ] } }
}]==])
check("json: bones parsed", #asset.bones == 2)
check("json: nested objects", type(asset.animations.idle.tracks) == "table"
      and #asset.animations.idle.tracks == 1)
check("json: nested arrays", #asset.mesh.positions == 3
      and asset.mesh.positions[2][1] == 1)
check("json: strings + numbers", asset.texture == "chara/hero.png"
      and asset.animations.idle.duration == 2.0)
check("json: escaped string", sma.load('{"a":"x\\ny"}').a == "x\ny")

-- ---------------------------------------------------------------------------
-- 2. Hierarchy world-pose resolution
-- ---------------------------------------------------------------------------
local function near(a, b) return math.abs(a - b) < 1e-4 end

-- root with pivot + rotation: T(p)R T(-p) baked into the BonePose offset
local w1 = sma.resolve_world(
    { { id = 0, parent = -1, pivot = { 1, 0 } } },
    { { rot = math.pi / 2, scale = 1, ox = 0, oy = 0 } })
check("hierarchy: root pivot baked (rot)", near(w1[1].rot, math.pi / 2))
check("hierarchy: root pivot baked (offset)",
      near(w1[1].ox, 1) and near(w1[1].oy, -1))

-- identity case: pivot cancels without rotation
local w2 = sma.resolve_world(
    { { id = 0, parent = -1, pivot = { 1, 0 } } },
    { { rot = 0, scale = 1, ox = 0, oy = 0 } })
check("hierarchy: pivot cancels when idle",
      near(w2[1].ox, 0) and near(w2[1].oy, 0))

-- child inherits parent rotation + scale (parent chain)
local w3 = sma.resolve_world(
    {
        { id = 0, parent = -1, pivot = { 0, 0 } },
        { id = 1, parent = 0, pivot = { 0, 0 } },
    },
    {
        { rot = math.pi / 2, scale = 2, ox = 0, oy = 0 },
        { rot = math.pi / 4, scale = 1, ox = 5, oy = 0 },
    })
check("hierarchy: child world rot = parent + local",
      near(w3[2].rot, math.pi / 2 + math.pi / 4))
check("hierarchy: child world scale = product",
      near(w3[2].scale, 2))
-- child local offset (5,0) is rotated+scaled by the parent: 90deg * 2 -> (0, 10)
check("hierarchy: child offset transformed by parent",
      near(w3[2].ox, 0) and near(w3[2].oy, 10))

-- ---------------------------------------------------------------------------
-- 3. Animation LERP
-- ---------------------------------------------------------------------------
local l1 = sma.sample(asset, "idle", 0.5)
check("lerp: midpoint of 0 -> 1.5708", l1[2] and near(l1[2].rot, 0.7854))
local l2 = sma.sample(asset, "idle", 2.0)
check("lerp: clamps after last frame", l2[2] and near(l2[2].rot, 1.5708))
local l3 = sma.sample(asset, "missing", 0.5)
check("lerp: unknown animation -> nil", l3 == nil)

-- ---------------------------------------------------------------------------
-- 4. Actor lifecycle through the binding mock
-- ---------------------------------------------------------------------------
sma.register("hero", asset)
local ctx = {}
local actor = sma.spawn(ctx, "hero1", asset, "idle", {
    x = 100, y = 50, scale = 2, texId = 42,
})
check("spawn: binding create called", _G.sma.created == 1 and actor.handle == 1)
check("spawn: actor state", ctx.sma_actors.hero1 ~= nil
      and ctx.sma_actors.hero1.texId == 42
      and ctx.sma_actors.hero1.scale == 2)

sma.update(ctx, 0.5)
check("update: binding update called with world poses", _G.sma.updates == 1)
sma.render(ctx)
check("render: binding draw called", _G.sma.draws == 1)

sma.despawn(ctx, "hero1")
check("despawn: binding destroy called", _G.sma.destroyed == 1)
check("despawn: actor removed", ctx.sma_actors.hero1 == nil)

-- [sma_play] command registers an actor; [sma_stop] removes it
local cmdCtx = { sma_actors = {} }
sma.commands.sma_play(cmdCtx, { name = "a", asset = "hero", anim = "idle", x = 1 })
check("sma_play: actor spawned", cmdCtx.sma_actors.a ~= nil
      and cmdCtx.sma_actors.a.anim == "idle")
sma.commands.sma_stop(cmdCtx, { name = "a" })
check("sma_stop: actor removed", cmdCtx.sma_actors.a == nil)

sma.commands.sma_play(cmdCtx, { name = "a", asset = "nope" })
check("sma_play: unknown asset is inert", cmdCtx.sma_actors.a == nil)

-- despawn without binding present (inert path)
local old = _G.sma
_G.sma = nil
local ctx2 = {}
sma.spawn(ctx2, "x", asset, "idle", {})
check("spawn without binding: handle 0", ctx2.sma_actors.x.handle == 0)
sma.update(ctx2, 1.0)
sma.render(ctx2)
check("update/render without binding: no crash", true)
_G.sma = old

if failed > 0 then
    print(string.format("SMA TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("SMA TESTS DONE (%d passed)", passed))
