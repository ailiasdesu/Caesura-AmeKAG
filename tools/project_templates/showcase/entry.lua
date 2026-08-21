-- Caesura (AmeKAG) — New Project Template entry point.
--
-- Boots the KAG runner against story.ks, exactly like demo/example_game/
-- entry.lua but with the absolute minimum wiring. Copy this file into your
-- own project and adjust the story path once you rename/move things.
--
-- Usage (from the repo root or build output dir):
--   lua demo/template/entry.lua

local kag_runner = require("kag_runner")

local function file_exists(path)
    local f = io.open(path, "r")
    if f then f:close(); return true end
    return false
end

local story_path = nil
for _, p in ipairs({
    "tools/project_templates/showcase/story.ks",
    "showcase/story.ks",
    "../tools/project_templates/showcase/story.ks",
}) do
    if file_exists(p) then story_path = p; break end
end

if not story_path then
    print("[Template] FATAL: cannot find story.ks (run from repo root or build/tests/Debug)")
    return
end

print("[Template] Loading: " .. story_path)
local started = kag_runner.start(story_path)
if not started then
    print("[Template] FATAL: failed to start KAG runner")
    return
end

-- Minimal Lua-side API for [iscript] blocks to call (optional).
local _G_template = {
    title = "My New Caesura Game",
}
_G.template_game = _G_template

print("[Template] Ready. Press Esc for menu, Ctrl to skip.")

