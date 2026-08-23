-- Caesura (AmeKAG) — First-VN E2E Project entry point.
--
-- Boots the KAG runner against story.ks, exactly like demo/example_game/
-- entry.lua but scoped to the first_vn fixture: the COMPLETE USER CREATION
-- FLOW acceptance project (task book §6). Where golden_vn regresses the
-- Runtime feature surface, first_vn walks the path a new author takes:
-- create -> write -> run -> choose -> save/load -> package.
--
-- Usage (from the repo root or build output):
--   lua tests/projects/first_vn/entry.lua
--   # or via the gate:  bash scripts/verify_first_vn.sh

local kag_runner = require("kag_runner")

local function file_exists(path)
    local f = io.open(path, "r")
    if f then f:close(); return true end
    return false
end

local story_path = nil
for _, p in ipairs({
    "tests/projects/first_vn/story.ks",
    "first_vn/story.ks",
    "../tests/projects/first_vn/story.ks",
}) do
    if file_exists(p) then story_path = p; break end
end

if not story_path then
    print("[FirstVN] FATAL: cannot find story.ks (run from repo root or build/tests/Debug)")
    return
end

print("[FirstVN] Loading: " .. story_path)
local started = kag_runner.start(story_path)
if not started then
    print("[FirstVN] FATAL: failed to start KAG runner")
    return
end

-- Lua-side API for [iscript] blocks (optional; the story uses flags only).
_G.first_vn = {
    version = "1.0",
    feature_flags = { "dialogue", "character", "background", "bgm", "se",
                      "choice", "save", "load", "i18n", "expression_conditional",
                      "transition", "ending" },
}

print("[FirstVN] Ready. First-VN E2E project is the user-creation-flow fixture.")
