-- ===========================================================================
-- demo_minigame.lua — Caesura (AmeKAG) MiniGame 3D Demo
-- ===========================================================================
-- Demonstrates: 4 geometries, PBR materials, multi-light, collision, physics
-- Requires: mini_game Lua module (registerMiniGameBinding)
-- ===========================================================================

print("[Demo] === MiniGame 3D Demo Start ===")

-- ── Materials ───────────────────────────────────────────────────────────
local mat_red    = mini_game.create_material(1.0, 0.2, 0.2, 0.3, 0.0, 0.8, "RedPlastic")
local mat_green  = mini_game.create_material(0.2, 1.0, 0.2, 0.4, 0.0, 0.6, "GreenPlastic")
local mat_blue   = mini_game.create_material(0.2, 0.3, 1.0, 0.2, 0.0, 1.0, "BlueShiny")
local mat_gold   = mini_game.create_material(1.0, 0.84,0.0, 0.15, 1.0, 1.5, "GoldMetal")
local mat_floor  = mini_game.create_material(0.4, 0.4, 0.4, 0.9, 0.0, 0.2, "FloorMatte")

-- ── Lighting ────────────────────────────────────────────────────────────
mini_game.set_ambient(0.1, 0.1, 0.15)
mini_game.set_directional(0.4, -0.8, 0.3, 0.9, 0.85, 1.0, 1.2)

-- Warm point light hovering above
local light1 = mini_game.add_point_light(0, 3, 0, 1.0, 0.6, 0.2, 1.5, 8.0, "WarmHover")
local light2 = mini_game.add_point_light(5, 2, 0, 0.2, 0.3, 1.0, 1.0, 6.0, "CoolSide")

-- ── Scene objects ───────────────────────────────────────────────────────
-- Floor plane
local floor_id = mini_game.spawn_plane(0, -2, 0, 20, 20, 0.5, 0.5, 0.5)
mini_game.set_material(floor_id, mat_floor)

-- Red sphere (main character)
local hero_id = mini_game.spawn_sphere(0, 0.5, 0, 1.0, 1.0, 0.2, 0.2)
mini_game.set_material(hero_id, mat_red)
mini_game.set_gravity(hero_id, true)

-- Gold cube on a pedestal
local cube_id = mini_game.spawn_cube(3, 0, 3, 0.5, 1.0, 0.84, 0.0)
mini_game.set_material(cube_id, mat_gold)

-- Green sphere
local green_id = mini_game.spawn_sphere(-3, 1, 2, 0.8, 0.2, 1.0, 0.2)
mini_game.set_material(green_id, mat_green)
mini_game.set_gravity(green_id, true)

-- Blue cube (tower)
local blue_id = mini_game.spawn_cube(-2, 0, -3, 1.0, 0.2, 0.3, 1.0)
mini_game.set_material(blue_id, mat_blue)

-- ── Camera ──────────────────────────────────────────────────────────────
mini_game.set_camera(8, 5, 10, 0, 1, 0)

-- ── Collision callback ──────────────────────────────────────────────────
function on_collision(objA, objB)
    print(string.format("[Demo] Collision: #%d <-> #%d", objA, objB))
end

print("[Demo] Scene loaded: 5 objects, 2 point lights, 5 materials")
print("[Demo] Gravity applied to: hero sphere + green sphere")
print("[Demo] === Demo ready — entering 3D scene ===")
