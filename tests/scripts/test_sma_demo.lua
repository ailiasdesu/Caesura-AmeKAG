-- test_sma_demo.lua — SMA demo showcase tests (round 19): the demo driver
-- (demo/sma_demo_driver.lua) runs against a mock binding + mock backend,
-- exercising the exact sequence demo/sma_entry.lua + sma_demo.ks perform
-- on a real GPU. Asset loading runs through the validator (validate=true).
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

package.path = "scripts/?.lua;scripts/kag/?.lua;demo/?.lua;" .. package.path

-- Mock engine binding (recording) + mock backend texture factory.
_G.sma = {
    created = 0, destroyed = 0, updates = 0, draws = 0,
    last_handle = 0, last_verts = nil,
    create_mesh = function(verts, indices)
        _G.sma.created = _G.sma.created + 1
        _G.sma.last_handle = _G.sma.created
        _G.sma.last_verts = verts
        return _G.sma.created
    end,
    destroy_mesh = function() _G.sma.destroyed = _G.sma.destroyed + 1 end,
    update_mesh = function() _G.sma.updates = _G.sma.updates + 1 end,
    draw_mesh = function() _G.sma.draws = _G.sma.draws + 1 end,
    count = function() return _G.sma.created end,
    initialized = function() return true end,
}
_G.backend = {
    create_solid_texture = function(w, h, rgb) return 7 end,
}

local sma = require("kag.sma")
local driver = require("demo.sma_demo_driver")

-- ---------------------------------------------------------------------------
-- 1. Asset load: demo hero validates and registers.
-- ---------------------------------------------------------------------------
local asset = driver.load_asset()
check("demo: hero asset loads", asset ~= nil)
check("demo: asset registered", sma.get("hero") ~= nil)
check("demo: asset has 8 bones", asset and #asset.bones == 8)
check("demo: two clips", asset and asset.animations.idle ~= nil
      and asset.animations.wave ~= nil)
check("demo: eye variants", asset and asset.parts
      and asset.parts[5].variants.happy ~= nil)

-- ---------------------------------------------------------------------------
-- 2. Spawn with runtime texture.
-- ---------------------------------------------------------------------------
local ctx = {}
local actor = driver.spawn_hero(ctx)
check("demo: spawn creates actor", actor ~= nil and ctx.sma_actors.hero ~= nil)
check("demo: idle loop default", actor and actor.anim == "idle" and actor.loop == true)
check("demo: runtime texture injected", actor and actor.texId == 7)
check("demo: multi-part meshes created", _G.sma.created == 5)  -- 5 parts

-- ---------------------------------------------------------------------------
-- 3. Script sequence (mirrors sma_demo.ks command stages).
-- ---------------------------------------------------------------------------
local stages = driver.run_script(ctx)
check("demo: all stages run", #stages == 4
      and stages[1] == "spawn" and stages[2] == "wave"
      and stages[3] == "variant" and stages[4] == "ik")
-- run_script reuses the existing actor (single spawn; the variant
-- switch rebuilds one part mesh: 5 parts + 1 rebuild = 6).
local live = ctx.sma_actors.hero
check("demo: single spawn across sequence", _G.sma.created == 6)
check("demo: crossfade armed", live.blend ~= nil)
-- set_variant switched eyes to happy: the part mesh was rebuilt.
check("demo: eyes variant switched", live.parts[5].current == "happy")
check("demo: IK constraint stored", live.ik ~= nil and live.ik.l2 == 0.3)

-- Advance a few frames: update_mesh is driven per frame.
local before = _G.sma.updates
for i = 1, 10 do sma.update(ctx, 0.016) end
check("demo: per-frame skinning driven", _G.sma.updates >= before + 10)
sma.render(ctx)
check("demo: per-frame draw driven", _G.sma.draws >= 1)

-- ---------------------------------------------------------------------------
-- 4. Teardown + failure paths.
-- ---------------------------------------------------------------------------
driver.teardown(ctx)
check("demo: teardown destroys actor", ctx.sma_actors.hero == nil)
check("demo: meshes destroyed", _G.sma.destroyed >= 5)

local saved = driver.asset_path
driver.asset_path = "definitely_missing_hero.json"
check("demo: missing asset -> nil", driver.load_asset() == nil)
driver.asset_path = saved

-- ---------------------------------------------------------------------------
-- 5. Script integrity: sma_demo.ks drives the same stages via commands.
-- ---------------------------------------------------------------------------
local f = io.open("demo/sma_demo.ks", "r")
local ksText = f and f:read("*a") or ""
if f then f:close() end
check("demo: ks script exists", #ksText > 0)
check("demo: ks drives anim", ksText:find("[sma_anim", 1, true) ~= nil)
check("demo: ks drives variant", ksText:find("[sma_variant", 1, true) ~= nil)
check("demo: ks drives ik", ksText:find("[sma_ik", 1, true) ~= nil)
check("demo: ks drives stop", ksText:find("[sma_stop", 1, true) ~= nil)

print(("SUMMARY sma_demo: %d passed, %d failed"):format(passed, failed))
if failed > 0 then os.exit(1) end
