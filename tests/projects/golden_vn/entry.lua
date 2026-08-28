-- Caesura (AmeKAG) — Golden Project entry point.
--
-- Boots the KAG runner against story.ks with the standard UI wiring, exactly
-- like demo/example_game/entry.lua but scoped to the golden regression
-- fixture. Golden VN lives under tests/projects/ so it is never packaged as
-- a user-facing artifact; it exists for release regression (task book §14).
--
-- Usage (from the repo root):
--   lua tests/projects/golden_vn/entry.lua   (bare lua self-locates scripts/)
--   # or via the gate:  bash scripts/verify_golden_vn.sh

-- [[ P0-1: self-locating scripts path. A bare `lua <this file>` has no
-- scripts/ entry in package.path, so resolve the engine root from this
-- file's location and prepend it. Prepend-only & idempotent; a no-op when
-- the engine (which configures the path itself) or a headless driver
-- already supplies the scripts path. ]]
pcall(function()
    local root, dir = nil, nil
    local a0 = arg and arg[0]
    if type(a0) == "string" and a0 ~= "" then
        dir = a0:gsub("\\", "/"):match("^(.*)/[^/]+$")
    end
    if not dir then
        local src = debug and debug.getinfo and debug.getinfo(1, "S") and debug.getinfo(1, "S").source
        if type(src) == "string" and src:sub(1, 1) == "@" then
            dir = src:sub(2):gsub("\\", "/"):match("^(.*)/[^/]+$")
        end
    end
    if not package.path:find("scripts/?/init.lua", 1, true) then
        local probes, probe = {}, dir
        while probe and probe ~= "" do
            probes[#probes + 1] = probe
            local up = probe:match("^(.*)/[^/]+$")
            if not up or up == probe then break end
            probe = up
        end
        probes[#probes + 1] = "."
        for _, p in ipairs(probes) do
            local ok, f = pcall(io.open, p .. "/scripts/kag_runner.lua", "r")
            if ok and f then
                f:close()
                root = p
                break
            end
        end
        if root and not package.path:find(root .. "/scripts/?/init.lua", 1, true) then
            package.path = root .. "/scripts/?.lua;"
                         .. root .. "/scripts/?/init.lua;"
                         .. root .. "/scripts/kag/?.lua;"
                         .. root .. "/scripts/kag/commands/?.lua;"
                         .. package.path
        end
    end
end)

local kag_runner = require("kag_runner")

local function file_exists(path)
    local f = io.open(path, "r")
    if f then f:close(); return true end
    return false
end

-- P0-1: prefer the story that sits NEXT TO this entry (the project's own
-- story, whatever the CWD is), then fall back to the historical
-- repo/build-relative candidates.
local script_dir = nil
do
    local src = debug and debug.getinfo and debug.getinfo(1, "S") and debug.getinfo(1, "S").source
    if src and src:sub(1, 1) == "@" then src = src:sub(2) end
    if src then
        local norm = src:gsub("\\", "/")
        local d = norm:match("^(.*)/entry%.lua$")
        if d and d ~= "" then script_dir = d end
    end
end

local story_path = nil
-- Packaged builds: the boot shim (scripts/caesura_build.py BOOT_TEMPLATE)
-- publishes the authoritative entry scene; prefer it over sibling/legacy
-- candidates so `caesura build --entry <other.ks>` keeps its contract.
if type(_G.CAESURA_STORY_PATH) == "string" and _G.CAESURA_STORY_PATH ~= "" then
    story_path = _G.CAESURA_STORY_PATH
elseif script_dir and file_exists(script_dir .. "/story.ks") then
    story_path = script_dir .. "/story.ks"
end
for _, p in ipairs({
    "tests/projects/golden_vn/story.ks",
    "golden_vn/story.ks",
    "../tests/projects/golden_vn/story.ks",
}) do
    if not story_path and file_exists(p) then story_path = p; break end
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
