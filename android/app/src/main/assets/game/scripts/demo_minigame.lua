-- ===========================================================================
-- demo_minigame.lua — Caesura (AmeKAG) MiniGame 3D Demo
-- ===========================================================================
-- Demonstrates: 4 geometries, PBR materials, multi-light, collision, physics
-- Reserved example: the mini_game Script adapter is not registered yet.
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

-- ── WASD Player Controller ─────────────────────────────────────────────────
-- Uses _GAME_KEY_W/A/S/D globals (set by Engine.cpp SDL event loop).
-- Runs in a cooperative coroutine to avoid blocking the engine.

local hero_velocity = 5.0
local wasd_active = false

function update_player_input()
    local vx, vz = 0, 0
    if _GAME_KEY_W then vz = vz - 1 end
    if _GAME_KEY_S then vz = vz + 1 end
    if _GAME_KEY_A then vx = vx - 1 end
    if _GAME_KEY_D then vx = vx + 1 end

    if vx ~= 0 or vz ~= 0 then
        wasd_active = true
        local speed = hero_velocity
        local len = math.sqrt(vx * vx + vz * vz)
        if len > 0 then
            vx = vx / len * speed
            vz = vz / len * speed
        end
        mini_game.set_velocity(hero_id, vx, 0, vz)
    elseif wasd_active then
        -- Stop when keys released
        mini_game.set_velocity(hero_id, 0, 0, 0)
        wasd_active = false
    end
end

-- Register per-frame update via the scheduler's yield pattern
local update_co = coroutine.create(function()
    while true do
        update_player_input()
        coroutine.yield()
    end
end)

-- Start the input loop (engine calls resume each frame when mini-game is active)
local function _minigame_update()
    if coroutine.status(update_co) ~= "dead" then
        coroutine.resume(update_co)
    end
end

print("[Demo] WASD input controller active (W/A/S/D to move hero)")
print("[Demo] === Demo ready — entering 3D scene ===")

-- Activate the programmatic scene: enter(0) starts rendering the objects
-- spawned above without a JSON scene descriptor (C2).
local act_ok = mini_game.enter(0)
print("[Demo] mini_game.enter(0) ->", tostring(act_ok), "active =", tostring(mini_game.is_active()))
