-- ===========================================================================
--  Caesura Scene Template
--  Copy this file and customize for each scene in your visual novel.
--  All KAG APIs documented in: docs/api/KAG-API.md
--  Sandbox rules documented in: scripts/sandbox.lua
-- ===========================================================================

-- Scene metadata (used by save/load system)
local scene_name = "prologue"  -- unique scene identifier

-- ---------------------------------------------------------------------------
--  Entry point: called when scene is loaded
-- ---------------------------------------------------------------------------
function scene_start()
    KAG.log("info", "Scene: " .. scene_name .. " started")

    -- Set up background
    KAG.clear_screen()
    KAG.show_image("bg", "assets/bg/classroom.png", 0, 0)

    -- Play background music
    KAG.play_bgm("assets/bgm/morning.ogg", 1.0)

    -- Show first dialogue
    KAG.show_text("???: Good morning...")
    KAG.wait_click()

    -- Your scene content here...
end

-- ---------------------------------------------------------------------------
--  Optional: called before scene is unloaded
-- ---------------------------------------------------------------------------
function scene_end()
    KAG.stop_bgm(1.0)
    KAG.clear_screen()
end
