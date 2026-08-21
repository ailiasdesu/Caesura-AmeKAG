-- Caesura (AmeKAG) — Golden Project entry point.
--
-- Boots the KAG runner against story.ks with the standard UI wiring, exactly
-- like demo/example_game/entry.lua but scoped to the golden regression
-- fixture. Golden VN lives under tests/projects/ so it is never packaged as
-- a user-facing artifact; it exists for release regression (task book §14).
--
-- Usage (from the repo root or build output):
--   lua tests/projects/golden_vn/entry.lua
--   # or via the gate:  bash scripts/verify_golden_vn.sh

local kag_runner = require("kag_runner")

local function file_exists(path)
    local f = io.open(path, "r")
    if f then f:close(); return true end
    return false
end

local story_path = nil
for _, p in ipairs({
    "tests/projects/golden_vn/story.ks",
    "golden_vn/story.ks",
    "../tests/projects/golden_vn/story.ks",
}) do
    if file_exists(p) then story_path = p; break end
end

if not story_path then
    print("[GoldenVN] FATAL: cannot find story.ks (run from repo root or build/tests/Debug)")
    return
end

print("[GoldenVN] Loading: " .. story_path)
local started = kag_runner.start(story_path)
if not started then
    print("[GoldenVN] FATAL: failed to start KAG runner")
    return
end

-- Lua-side API for [iscript] blocks (optional; the story uses flags only).
_G.golden_vn = {
    version = "1.0",
    feature_flags = { "dialogue", "choices", "save", "rollback", "history",
                      "backlog", "nvl", "i18n", "audio", "tween", "layout",
                      "replay", "mod", "markup", "transition", "particles" },
}

print("[GoldenVN] Ready. Golden Project is the long-term release fixture.")
