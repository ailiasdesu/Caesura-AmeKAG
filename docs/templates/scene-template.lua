-- ===========================================================================
--  Caesura Scene Template
--  Copy and customize for each scene. Docs: docs/api/KAG-API.md
-- ===========================================================================

local scene = "untitled"

function scene_start()
    KAG.log("info", "Scene: " .. scene .. " started")

    -- Clear all layers
    KAG.clear_screen()

    -- Background image
    KAG.show_image("bg", "assets/bg/placeholder.png", 0, 0)

    -- Background music
    KAG.play_bgm("assets/bgm/placeholder.ogg", 1.0)

    -- First dialogue
    KAG.show_text("Character: Hello, world.")
    KAG.wait_click()

    -- Your content here --
end

function scene_end()
    KAG.stop_bgm(1.0)
    KAG.clear_screen()
end

-- ===========================================================================
--  Common patterns
-- ===========================================================================

-- Character dialogue
-- KAG.show_image("fg", "assets/chara/alice_smile.png", 400, 100)
-- KAG.show_text("Alice: Nice to meet you!")
-- KAG.wait_click()

-- Sound effect
-- local se = KAG.play_se("assets/se/door_open.wav")

-- Transition: fade to black then new scene
-- KAG.set_global_volume(1.0)
-- for i = 10, 0, -1 do
--     KAG.set_global_volume(i / 10)
--     -- wait one frame
-- end
-- KAG.clear_screen()
-- KAG.set_global_volume(1.0)

-- Screen shake
-- KAG.quake(5, 0.5)

-- 3D positional audio
-- KAG.set_listener(0, 0, 0, 1, 0, 0)
-- KAG.play_se_3d("assets/se/footstep.wav", 2, 0, 0)