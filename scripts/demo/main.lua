-- ============================================================================
-- Caesura (AmeKAG) Demo -- Complete engine capability demonstration
-- ============================================================================
-- Scene 1: Classical classroom -- layered rendering + text
-- Scene 2: MiniGame 3D -- geometry, PBR, multi-light, collision
-- Scene 3: Video playback + save/load demo
-- ============================================================================

local layers  = require("layers")
local backend = require("backend")
local w, h = backend.get_resolution()
if not w then w, h = 1280, 720 end

-- Color helpers
local function solid(r, g, b, a)
    return backend.create_solid_texture(
        math.floor(r), math.floor(g), math.floor(b), math.floor(a or 255))
end

local function draw_text(x, y, text, r, g, b, a, scale)
    backend.render_text(text, x, y, scale or 1.0, r, g, b, a)
end

-- =========================================================================
-- Scene 1: Classical Classroom
-- =========================================================================
local function scene1_classroom()
    print("[Demo] Scene 1: Classical Classroom")

    layers.init()
    local root = layers.get_root()

    -- Gradient background
    root.texture = solid(40, 20, 60, 255)
    layers.set_layer_image(root, root.texture)

    -- Title card
    local card = layers.add_layer(root, {
        name = "title_card", z = 2,
        x = w * 0.15, y = h * 0.15, w = w * 0.7, h = h * 0.7, visible = true
    })
    card.texture = solid(18, 15, 35, 220)
    layers.set_layer_image(card, card.texture)

    -- Render phase
    layers.render()
    draw_text(w * 0.25, h * 0.25, "Classical Classroom", 255, 220, 180, 255, 2.0)
    draw_text(w * 0.25, h * 0.38, "Afternoon sunlight filters through the window,", 200, 190, 220, 255, 1.0)
    draw_text(w * 0.25, h * 0.44, "dust particles floating in the air.", 200, 190, 220, 255, 1.0)
    draw_text(w * 0.25, h * 0.52, "A notebook lies open on the desk,", 200, 190, 220, 255, 1.0)
    draw_text(w * 0.25, h * 0.58, "filled with unfinished poems.", 200, 190, 220, 255, 1.0)

    -- Waiting indicator
    draw_text(w * 0.25, h * 0.72, "[ Scene 1 complete - press any key for next ]", 120, 130, 180, 200, 0.85)

    backend.wait_click()
    print("[Demo] Scene 1 done")
end

-- =========================================================================
-- Scene 2: MiniGame 3D
-- =========================================================================
local function scene2_minigame()
    print("[Demo] Scene 2: MiniGame 3D")

    -- Initialize MiniGame
    if not mini_game then
        print("[Demo] MiniGame not available (C++ backend missing)")
        return
    end

    local mat_red   = mini_game.create_material(1.0, 0.2, 0.2, 0.3, 0.0, 0.8, "RedPlastic")
    local mat_green = mini_game.create_material(0.2, 1.0, 0.2, 0.4, 0.0, 0.6, "GreenPlastic")
    local mat_blue  = mini_game.create_material(0.2, 0.3, 1.0, 0.2, 0.0, 1.0, "BlueShiny")
    local mat_gold  = mini_game.create_material(1.0, 0.84, 0.0, 0.15, 1.0, 1.5, "GoldMetal")
    local mat_floor = mini_game.create_material(0.4, 0.4, 0.4, 0.9, 0.0, 0.2, "FloorMatte")

    mini_game.set_ambient(0.1, 0.1, 0.15)
    mini_game.set_directional(0.4, -0.8, 0.3, 0.9, 0.85, 1.0, 1.2)

    mini_game.add_point_light(0, 3, 0, 1.0, 0.6, 0.2, 1.5, 8.0, "WarmHover")
    mini_game.add_point_light(5, 2, 0, 0.2, 0.3, 1.0, 1.0, 6.0, "CoolSide")

    -- Floor
    local floor_id = mini_game.spawn_plane(0, -2, 0, 20, 20, 0.5, 0.5, 0.5)
    mini_game.set_material(floor_id, mat_floor)

    -- Hero sphere
    local hero_id = mini_game.spawn_sphere(0, 0.5, 0, 1.0, 1.0, 0.2, 0.2)
    mini_game.set_material(hero_id, mat_red)
    mini_game.set_gravity(hero_id, true)

    -- Gold cube
    local cube_id = mini_game.spawn_cube(3, 0, 3, 0.5, 1.0, 0.84, 0.0)
    mini_game.set_material(cube_id, mat_gold)

    -- Green sphere
    local green_id = mini_game.spawn_sphere(-3, 1, 2, 0.8, 0.2, 1.0, 0.2)
    mini_game.set_material(green_id, mat_green)
    mini_game.set_gravity(green_id, true)

    -- Blue cube tower
    local blue_id = mini_game.spawn_cube(-2, 0, -3, 1.0, 0.2, 0.3, 1.0)
    mini_game.set_material(blue_id, mat_blue)

    mini_game.set_camera(8, 5, 10, 0, 1, 0)
    mini_game.set_collision(true)

    print("[Demo] MiniGame scene loaded: 5 objects, 2 lights, 5 materials")
    backend.wait_click()
    print("[Demo] Scene 2 done")
end

-- =========================================================================
-- Scene 3: Video + Save Demo
-- =========================================================================
local function scene3_video_save()
    print("[Demo] Scene 3: Video + Save Demo")

    layers.init()
    local root = layers.get_root()
    root.texture = solid(20, 20, 40, 255)
    layers.set_layer_image(root, root.texture)

    layers.render()
    draw_text(w * 0.15, h * 0.3, "Video & Save System", 180, 200, 255, 255, 1.8)
    draw_text(w * 0.15, h * 0.42, "Video playback: pl_mpeg (MPEG-1) + FFmpeg (optional)", 160, 170, 210, 255, 0.9)
    draw_text(w * 0.15, h * 0.49, "Save/Load: 8-slot manager with thumbnails + CRC", 160, 170, 210, 255, 0.9)
    draw_text(w * 0.15, h * 0.56, "CARC: zstd-compressed asset archives with Ed25519 signing", 160, 170, 210, 255, 0.9)
    draw_text(w * 0.15, h * 0.63, "Live2D: Cubism 5 Native SDK (D3D11/MSVC path)", 160, 170, 210, 255, 0.9)
    draw_text(w * 0.15, h * 0.72, "[ Demo complete - press any key to exit ]", 120, 130, 180, 200, 0.85)

    backend.wait_click()
    print("[Demo] Scene 3 done")
end

-- =========================================================================
-- Entry
-- =========================================================================
print("=== Caesura (AmeKAG) Engine Demo ===")
print(string.format("Resolution: %dx%d", w, h))

scene1_classroom()
scene2_minigame()
scene3_video_save()

print("[Demo] All scenes complete. Exiting.")
