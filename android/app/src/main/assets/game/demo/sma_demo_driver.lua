-- =============================================================================
--  demo/sma_demo_driver.lua — SMA showcase driver (Battle 4d, round 19).
--
--  Shared by demo/sma_entry.lua (real engine: kag_runner + engine hooks)
--  and tests/scripts/test_sma_demo.lua (mock binding). The .ks "script" is
--  the same sequence expressed as KAG commands for the real demo; the
--  driver keeps the Lua side (asset load + spawn with a solid texture)
--  testable without a GPU.
--
--  Sequence: load hero -> spawn (idle, loop) -> crossfade to wave ->
--  eyes variant (happy) -> 2-bone IK reach -> stop.
-- =============================================================================

local sma = require("kag.sma")

local M = {}

M.asset_path = "demo/assets/sma/hero.json"

--- load_asset() → registered asset (validated). Returns nil on failure.
function M.load_asset()
    local f = io.open(M.asset_path, "r")
    if not f then
        print("[SMA Demo] cannot open " .. M.asset_path)
        return nil
    end
    local text = f:read("*a")
    f:close()
    local asset = sma.load(text, { validate = true })
    sma.register("hero", asset)
    return asset
end

--- make_texture() → texture id from backend.create_solid_texture, or 0.
--  The demo assets ship without PNG textures (repo has no binary assets);
--  a solid color keeps the draw path bound and visible.
function M.make_texture()
    local b = rawget(_G, "backend")
    if type(b) == "table" and type(b.create_solid_texture) == "function" then
        return tonumber(b.create_solid_texture(16, 16, 0xffff88cc)) or 0
    end
    return 0
end

--- spawn_hero(ctx) → actor (spawns at stage center, idle loop).
function M.spawn_hero(ctx)
    local asset = M.load_asset()
    if not asset then return nil end
    local tex = M.make_texture()
    return sma.spawn(ctx, "hero", asset, "idle", {
        x = 0.5, y = 0.5, scale = 1.6, texId = tex, loop = true,
    })
end

--- run_script(ctx) — the demo sequence as Lua calls (mirrors sma_demo.ks).
--  Returns a table of stage names for the test to assert against.
function M.run_script(ctx)
    local stages = {}
    local actor = ctx and ctx.sma_actors and ctx.sma_actors.hero
    if not actor then actor = M.spawn_hero(ctx) end
    if not actor then return stages end
    stages[#stages + 1] = "spawn"
    sma.play_anim(ctx, "hero", "wave", { blend_time = 0.4, loop = true })
    stages[#stages + 1] = "wave"
    sma.set_variant(ctx, "hero", "eyes", "happy")
    stages[#stages + 1] = "variant"
    sma.set_ik(ctx, "hero", { 5, 6 }, 0.95, 0.25, nil, 0.3)
    stages[#stages + 1] = "ik"
    return stages
end

--- teardown(ctx) — stop the actor.
function M.teardown(ctx)
    sma.despawn(ctx, "hero")
end

return M
