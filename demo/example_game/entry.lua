-- Caesura (AmeKAG) — Example Game entry: "The Last Letter"
-- A complete short visual novel demonstrating KAG Neo-Genesis.
--
-- Usage (from the repo root or build output):
--   lua demo/example_game/entry.lua
--
-- The entry script boots the KAG runner against story.ks, wires the
-- standard UI overlays (history / toast), and shows how Lua and KAG
-- scripts interoperate (kag.jump / kag.save_game from Lua).

local kag_runner = require("kag_runner")

local function file_exists(path)
    local f = io.open(path, "r")
    if f then f:close(); return true end
    return false
end

local story_path = nil
for _, p in ipairs({
    "demo/example_game/story.ks",
    "example_game/story.ks",
    "../demo/example_game/story.ks",
}) do
    if file_exists(p) then story_path = p; break end
end

if not story_path then
    print("[ExampleGame] FATAL: cannot find story.ks (run from repo root or build/tests/Debug)")
    return
end

print("[ExampleGame] Loading: " .. story_path)
local started = kag_runner.start(story_path)
if not started then
    print("[ExampleGame] FATAL: failed to start KAG runner")
    return
end

-- History overlay coroutine (see demo/entry.lua for the resume pattern).
local history_co = nil
local _toast = pcall(require, "toast") and require("toast") or nil

-- Expose a Lua-side API for [iscript] blocks to call: the notebook's
-- "letters read" counter drives the good-ending condition demo.
local letters_read = 0
_G.example_game = {
    read_letter = function()
        letters_read = letters_read + 1
        print("[ExampleGame] letters read: " .. letters_read)
        return letters_read
    end,
    letters = function() return letters_read end,
}

print("[ExampleGame] Ready. Press Esc for menu, Ctrl to skip.")
