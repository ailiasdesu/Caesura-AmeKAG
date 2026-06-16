-- =============================================================================
--  Caesura (AmeKAG) -- tests/scripts/test_p2_features.lua
--  U2.1-U2.4: gallery, music_room, palette, i18n module tests
--  Run: external\lua\lua.exe tests/scripts/test_p2_features.lua
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;scripts/kag/commands/?.lua;" .. package.path

local passed, failed = 0, 0

local function assert_eq(actual, expected, msg)
    if actual == expected then
        passed = passed + 1
    else
        failed = failed + 1
        print(string.format("FAIL: %s (expected %s, got %s)", msg, tostring(expected), tostring(actual)))
    end
end

-- =============================================================================
-- U2.1: gallery.lua
-- =============================================================================

do
    package.loaded["backend"] = { load_texture = function() return 1 end }
    package.loaded["layers"] = { Type = {}, find = function() return nil end }
    package.loaded["fileutil"] = { scan_dir = function() return {} end }

    local Gallery = require("gallery")

    -- Test: unlock sets ctx.unlockedCG
    local ctx = {}
    Gallery.unlock(ctx, "scene01")
    assert_eq(ctx.unlockedCG ~= nil, true, "gallery: unlockedCG should be created")
    assert_eq(ctx.unlockedCG["scene01"], true, "gallery: scene01 should be unlocked")
    Gallery.unlock(ctx, "scene02")
    assert_eq(ctx.unlockedCG["scene02"], true, "gallery: scene02 should be unlocked")

    -- Test: double unlock is idempotent
    Gallery.unlock(ctx, "scene01")
    assert_eq(ctx.unlockedCG["scene01"], true, "gallery: double unlock idempotent")

    -- Test: list returns table
    local list = Gallery.list(ctx)
    assert_eq(type(list), "table", "gallery: list returns table")

    package.loaded["gallery"] = nil
    package.loaded["backend"] = nil
    package.loaded["layers"] = nil
    package.loaded["fileutil"] = nil
end

-- =============================================================================
-- U2.2: music_room.lua
-- =============================================================================

do
    package.loaded["backend"] = {
        load_texture = function() return 1 end,
        audio_play = function() return true end,
        audio_stop = function() return true end,
    }
    package.loaded["audio"] = { play = function() end, stop = function() end }
    package.loaded["fileutil"] = { scan_dir = function() return {} end }
    package.loaded["layers"] = { Type = {}, find = function() return nil end }

    local MusicRoom = require("music_room")

    -- Test: favorite toggle returns boolean
    local s1 = MusicRoom.favorite("track01")
    assert_eq(type(s1), "boolean", "music_room: favorite returns boolean")
    local s2 = MusicRoom.favorite("track01")
    assert_eq(s2, not s1, "music_room: toggle flips state")

    -- Test: list returns table
    local list = MusicRoom.list()
    assert_eq(type(list), "table", "music_room: list returns table")

    package.loaded["music_room"] = nil
    package.loaded["backend"] = nil
    package.loaded["audio"] = nil
    package.loaded["fileutil"] = nil
    package.loaded["layers"] = nil
end

-- =============================================================================
-- U2.3: palette.lua
-- =============================================================================

do
    local mock_backend = {
        load_image = function(path) return 42 end,
        is_valid = function(h) return h and h > 0 end,
        apply_lut = function(h, i) return true end,
        clear_lut = function() return true end,
        set_palette = function(handle, intensity) return true end,
        free_image = function(handle) end,
        destroy_texture = function(handle) end,
    }
    package.loaded["backend"] = mock_backend
    _G.backend = mock_backend  -- palette uses global, not require

    local palette = require("palette")

    -- Test: load LUT
    local ok, err = palette.load("test_lut", "fake/path.png")
    assert_eq(ok, true, "palette: load succeeds")

    -- Test: load with empty id fails
    local ok2 = palette.load("", "")
    assert_eq(ok2, nil, "palette: load empty id fails")

    -- Test: apply loaded LUT
    local ok3 = palette.apply("test_lut", 0.5)
    assert_eq(ok3, true, "palette: apply succeeds")

    -- Test: apply nonexistent fails
    local ok4 = palette.apply("nonexistent")
    assert_eq(ok4, nil, "palette: apply nonexistent fails")

    -- Test: clear
    local ok5 = pcall(palette.clear)
    assert_eq(ok5, true, "palette: clear no crash")

    -- Test: unload
    palette.load("temp", "fake/path.png")
    local ok6, err6 = pcall(palette.unload, "temp")
    if not ok6 then print("  unload error:", err6) end
    assert_eq(ok6, true, "palette: unload no crash")

    package.loaded["palette"] = nil
    package.loaded["backend"] = nil
end

-- =============================================================================
-- U2.4: i18n.lua
-- =============================================================================

do
    local i18n = require("i18n")

    -- Test: set strings directly
    i18n.strings = { start = "kai shi", load = "jia zai", save = "bao cun" }
    i18n.fallback = { start = "Start", load = "Loading", save = "Save" }

    assert_eq(i18n.t("start"), "kai shi", "i18n: t('start') returns translation")
    assert_eq(i18n.t("load"), "jia zai", "i18n: t('load') returns translation")

    -- Test: fallback to English
    i18n.strings = { start = "kai shi" }
    assert_eq(i18n.t("load"), "Loading", "i18n: fallback to English")
    assert_eq(i18n.t("missing"), "missing", "i18n: missing key returns key")

    -- Test: _expand replaces {key} tokens
    local expanded = i18n.expand("Press {start} to begin")
    assert_eq(type(expanded), "string", "i18n: _expand returns string")

    -- Test: current language
    assert_eq(i18n.current, "zh", "i18n: default language is zh")

    package.loaded["i18n"] = nil
end

-- =============================================================================
-- Results
-- =============================================================================

print(string.format("\nP2 feature tests: %d passed, %d failed", passed, failed))
if failed > 0 then
    os.exit(1)
end
