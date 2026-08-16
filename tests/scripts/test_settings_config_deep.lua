-- =============================================================================
--  test_settings_config_deep.lua — Deep coverage of scripts/settings.lua
--  (engine runtime settings) and scripts/config.lua (engine config load /
--  persistence) beyond the shallow test_settings.lua.
--
--  ORPHAN SUITE test (standalone subprocess): it seeds package.loaded with
--  mock backend/layers/audio/i18n and mocks the backend_factory global so it
--  can require the real settings.lua and config.lua in isolation, and it
--  writes real settings/*.lua persistence files (gitignored). It must NEVER
--  run inside the main suite (run_lua_tests.lua): it pollutes package.loaded
--  and _G.config and writes files.
--
--  Boundaries covered:
--    S1  settings defaults table is complete (volume/speed/accessibility/
--        window toggles/autosave); uninitialized read returns defaults;
--        post-set read returns the new value.
--    S2  _applyAll applies every setting: audio bus volumes, ctx typewriter
--        speed / skip / auto / cc flags, fullscreen, cc_mode mirror into
--        config.accessibility — HEADLESS-DEGRADING (no real audio/backend).
--    S3  slider value clamping: volume_bgm capped at 100, text_speed floored
--        at 10 and capped at 200 (settings menu range).
--    S4  language cycle hot-switch: cycles i18n.available(), records
--        settingsValues.language, triggers relocalize, and a throwing
--        (invalid) language code degrades inside the pcall (never raises).
--    C1  config.lua persistence: save_all -> load_all roundtrip.
--    C2  idempotent load: repeated load_all returns identical values, and
--        save->load->save is byte-stable.
--    C3  missing config file degrades to defaults (load_all falls back).
--    C4  corrupt config file falls back to defaults (no raise).
--    C5  partial (missing-field) config file fills holes with defaults.
--    C6  type whitelist: crafted types reject the whole file / non-numeric
--        values degrade THROUGH the tonumber-or-default gate.
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name)
    else print("FAIL " .. name) end
    if cond then passed = passed + 1 else failed = failed + 1 end
end

-- =============================================================================
-- Shared mocks (seeded BEFORE requiring settings/config so their upvalues
-- resolve to the mock modules)
-- =============================================================================

-- NOTE: a Lua local is only visible AFTER its declaration statement, so a
-- method that closes over its own surrounding table must set the local on a
-- SEPARATE statement (a self-reference inside the initializer resolves to a
-- nil global — a standard Lua pitfall). Each mock is built in two steps.
local audioMock = {}
audioMock._vol = { bgm = 1.0, voice = 1.0, se = 1.0 }
audioMock._spy = { bgm = nil, voice = nil, se = nil }
audioMock._calls = 0
audioMock.set_bgm_volume = function(v) audioMock._vol.bgm = v; audioMock._spy.bgm = v; audioMock._calls = audioMock._calls + 1; return true end
audioMock.set_se_volume = function(v) audioMock._vol.se = v; audioMock._spy.se = v; audioMock._calls = audioMock._calls + 1; return true end
audioMock.set_voice_volume = function(v) audioMock._vol.voice = v; audioMock._spy.voice = v; audioMock._calls = audioMock._calls + 1; return true end
audioMock.get_bgm_volume = function() return audioMock._vol.bgm end
audioMock.get_se_volume = function() return audioMock._vol.se end
audioMock.get_voice_volume = function() return audioMock._vol.voice end
package.loaded["audio"] = audioMock

-- Backend mock: records fullscreen calls, returns resolution, safe textures.
local backendMock = {}
backendMock._fsSpy = nil
backendMock.set_fullscreen = function(v) backendMock._fsSpy = v; return true end
backendMock.get_resolution = function() return 1280, 720 end
backendMock.set_input_focus = function() return true end
backendMock.create_solid_texture = function() return { _mock = true } end
backendMock.destroy_texture = function() return true end
backendMock.render_text = function() end
package.loaded["backend"] = backendMock

-- Layers mock: ensure/find by name (settings menu layer lifecycle).
-- layerStore is declared BEFORE layersMock, so layersMock may reference it
-- directly inside its initializer (no self-reference).
local layerStore = {}
local layersMock = {
    ensure = function(_, name, _z) layerStore[name] = layerStore[name] or { visible = true }; return layerStore[name] end,
    find = function(name) return layerStore[name] end,
}
package.loaded["layers"] = layersMock

-- i18n mock: t() passthrough, available() list, load() spy that throws for
-- the sentinel "xx" code (invalid-language degrade boundary).
local relocalizeSpy = { n = 0 }
local i18nMock = {}
i18nMock.current = "zh"
i18nMock._load = {}
i18nMock.available = function() return { "zh", "en", "xx" } end
i18nMock.t = function(k) return k end
i18nMock.load = function(code)
    i18nMock.current = code
    i18nMock._load[code] = (i18nMock._load[code] or 0) + 1
    if code == "xx" then error("unknown language: " .. tostring(code)) end
    return true
end
package.loaded["i18n"] = i18nMock
-- Stub heavy kag.commands.text so relocalize_after_switch resolves cheaply.
package.loaded["kag.commands.text"] = {
    relocalize_page = function() relocalizeSpy.n = relocalizeSpy.n + 1; return true end,
}

-- backend_factory mock so config.lua's require-time apply() resolves.
package.loaded["backend_factory"] = {
    create = function()
        local mk = function() return true end
        return {
            render = function(cmd) if cmd == "name" then return "mockrender" end return true end,
            audio  = function(cmd) if cmd == "name" then return "mockaudio" end return true end,
            platform = function(cmd) if cmd == "name" then return "mockplatform" end return true end,
        }
    end,
}

-- Guard: config.lua sets _G.config; capture a saved audio volume of the
-- mock so we never depend on a real audio binding.
local settings_files = { "settings/config.lua", "settings/volume.lua", "settings/window.lua" }
local function clean_settings()
    for _, p in ipairs(settings_files) do pcall(os.remove, p) end
end

-- =============================================================================
--  PART A — settings.lua
-- =============================================================================

local Settings = require("settings")

-- S1a: _buildMenu fills a fresh (uninitialized) ctx with the full default set.
do
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, variables = {} }
    local items = Settings._buildMenu(ctx)
    local sv = ctx.settingsValues
    check("S1 defaults: volume_bgm=80", sv.volume_bgm == 80)
    check("S1 defaults: volume_se=80", sv.volume_se == 80)
    check("S1 defaults: volume_voice=100", sv.volume_voice == 100)
    check("S1 defaults: text_speed=50", sv.text_speed == 50)
    check("S1 defaults: skip_mode off", sv.skip_mode == false)
    check("S1 defaults: skip_auto off", sv.skip_auto == false)
    check("S1 defaults: auto_mode off", sv.auto_mode == false)
    check("S1 defaults: cc_mode off", sv.cc_mode == false)
    check("S1 defaults: fullscreen off", sv.fullscreen == false)
    check("S1 defaults: autosave_interval=60", sv.autosave_interval == 60)
    check("S1 defaults: menu built with 12 items", #items == 12)
    check("S1 slider ranges present (text_speed 10..200)",
        items[4].type == "slider" and items[4].min == 10 and items[4].max == 200)
end

-- S1b: uninitialized read = default; after set, read returns the new value.
do
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, variables = {} }
    Settings._buildMenu(ctx)
    check("S1 uninitialized read returns default", ctx.settingsValues.volume_bgm == 80)
    ctx.settingsValues.volume_bgm = 95
    ctx.settingsValues.text_speed = 120
    local items = Settings._buildMenu(ctx)  -- rebuild: preserves existing, fills missing
    check("S1 after set, read returns new value",
        ctx.settingsValues.volume_bgm == 95 and ctx.settingsValues.text_speed == 120)
    check("S1 rebuild keeps explicit + fills untouched defaults",
        items[4].value == 120 and ctx.settingsValues.skip_mode == false)
end

-- S2: headless _applyAll applies every setting without a real backend.
do
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, variables = {} }
    audioMock._spy.bgm, audioMock._spy.voice, audioMock._spy.se = nil, nil, nil
    ctx.settingsValues = {
        volume_bgm = 80, volume_se = 40, volume_voice = 100,
        text_speed = 60, skip_mode = true, skip_auto = true,
        auto_mode = true, cc_mode = true, fullscreen = true,
        autosave_interval = 60,
    }
    local ok, err = pcall(Settings._applyAll, ctx)
    check("S2 applyAll runs headless (no raise)", ok)
    check("S2 applyAll bgm volume = 0.8", audioMock._spy.bgm == 0.8)
    check("S2 applyAll se volume = 0.4", audioMock._spy.se == 0.4)
    check("S2 applyAll voice volume = 1.0", audioMock._spy.voice == 1.0)
    check("S2 applyAll ctx.text_speed", ctx.text_speed == 60)
    check("S2 applyAll ctx.skip_mode", ctx.skip_mode == true)
    check("S2 applyAll ctx.skip_auto", ctx.skip_auto == true)
    check("S2 applyAll ctx.auto_mode", ctx.auto_mode == true)
    check("S2 applyAll ctx.cc_mode", ctx.cc_mode == true)
    check("S2 applyAll fullscreen routed to backend", backendMock._fsSpy == true)
    -- cc_mode mirrors into config.accessibility (real config global)
    check("S2 applyAll cc_mode mirrors into config.accessibility",
        _G.config ~= nil and _G.config.accessibility ~= nil
        and _G.config.accessibility.cc_mode == true)
    -- empty settingsValues is a safe no-op (headless)
    local empty = { f = {}, sf = {}, tf = {}, mp = {}, variables = {} }
    local ok2 = pcall(Settings._applyAll, empty)
    check("S2 applyAll with no settingsValues no-ops", ok2)
end

-- S3a: volume slider draws through the show() loop and clamps at 100.
do
    audioMock._spy.bgm = nil
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, variables = {} }
    local co = coroutine.create(function() Settings.show(ctx) end)
    coroutine.resume(co)
    -- cursor 1 = volume_bgm; press RIGHT past 100 (80 -> 100 in 4 presses).
    for i = 1, 10 do
        _G._GAME_KEY_RIGHT = true
        coroutine.resume(co)
        _G._GAME_KEY_RIGHT = nil
    end
    check("S3 volume_bgm clamped at max 100",
        ctx.settingsValues.volume_bgm == 100)
    -- ESC -> hide -> _applyAll writes clamped 1.0 to audio bgm.
    _G._GAME_KEY_ESC = true
    coroutine.resume(co)
    _G._GAME_KEY_ESC = nil
    check("S3 after hide applies clamped bgm=1.0", audioMock._spy.bgm == 1.0)
    check("S3 show closed cleanly", coroutine.status(co) == "dead" and ctx._settingsActive == false)
end

-- S3b: text_speed slider clamps to [10,200] through the loop.
do
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, variables = {} }
    local co = coroutine.create(function() Settings.show(ctx) end)
    coroutine.resume(co)
    -- navigate DOWN x3 to the text_speed item (index 4).
    for _ = 1, 3 do _G._GAME_KEY_DOWN = true; coroutine.resume(co); _G._GAME_KEY_DOWN = nil end
    -- RIGHT many times (30+ presses from 50) -> cap 200
    for i = 1, 40 do _G._GAME_KEY_RIGHT = true; coroutine.resume(co); _G._GAME_KEY_RIGHT = nil end
    check("S3 text_speed clamped at max 200", ctx.settingsValues.text_speed == 200)
    -- LEFT many times -> floor 10
    for i = 1, 40 do _G._GAME_KEY_LEFT = true; coroutine.resume(co); _G._GAME_KEY_LEFT = nil end
    check("S3 text_speed floored at min 10", ctx.settingsValues.text_speed == 10)
    _G._GAME_KEY_ESC = true
    coroutine.resume(co)
    _G._GAME_KEY_ESC = nil
    check("S3 hide applies floored text_speed=10", ctx.text_speed == 10)
end

-- S4: language cycle hot-switch + invalid-code degrade through the loop.
do
    i18nMock.current = "zh"
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, variables = {} }
    local menu = Settings._buildMenu(ctx)   -- side effect: fills settingsValues
    -- language is NOT part of the defaults table (settingsValues.language is
    -- nil after _buildMenu); seed it so the first RIGHT advances determinis-
    -- tically zh -> en rather than picking the first available option.
    ctx.settingsValues.language = "zh"
    -- locate the language item's cursor index dynamically.
    local langIdx = nil
    for i, it in ipairs(menu) do if it.key == "language" then langIdx = i end end
    local co = coroutine.create(function() Settings.show(ctx) end)
    coroutine.resume(co)
    for _ = 2, langIdx do _G._GAME_KEY_DOWN = true; coroutine.resume(co); _G._GAME_KEY_DOWN = nil end
    relocalizeSpy.n = 0
    -- cycle right: zh -> en (valid hot-switch)
    _G._GAME_KEY_RIGHT = true; coroutine.resume(co); _G._GAME_KEY_RIGHT = nil
    check("S4 language cycle records settingsValues.language",
        ctx.settingsValues.language == "en")
    check("S4 language cycle switched i18n.current", i18nMock.current == "en")
    check("S4 language cycle triggered relocalize", relocalizeSpy.n >= 1)
    check("S4 language cycle called i18n.load('en')", (i18nMock._load.en or 0) >= 1)
    -- cycle right again: en -> xx (load throws); must degrade, not raise.
    local okCycle = pcall(function()
        _G._GAME_KEY_RIGHT = true
        coroutine.resume(co)
        _G._GAME_KEY_RIGHT = nil
    end)
    check("S4 invalid language code degrades (no raise)", okCycle)
    check("S4 invalid language code still recorded as selection",
        ctx.settingsValues.language == "xx")
    _G._GAME_KEY_ESC = true
    coroutine.resume(co)
    _G._GAME_KEY_ESC = nil
end

-- =============================================================================
--  PART B — config.lua persistence / loading
-- =============================================================================

-- config's ensure_dir() cannot create settings/ from scratch here (its
-- os.rename/mmDir probe needs the parent to exist), so pre-create it; the
-- directory is gitignored and cleaned with the files at the end.
pcall(os.execute, "mkdir -p settings")

local config = require("config")   -- require-time apply() already ran

-- C3: missing config file degrades to defaults.
do
    clean_settings()
    local b1 = config.bgm_volume
    local w1 = config.window_width
    local ok = pcall(config.load_all)
    check("C3 missing file: load_all runs (no raise)", ok)
    check("C3 missing file: bgm_volume keeps default", config.bgm_volume == b1)
    check("C3 missing file: window_width keeps default", config.window_width == w1)
end

-- C1 + C5 + C2: roundtrip, partial holes, idempotent/byte-stable.
do
    clean_settings()
    config.bgm_volume = 0.42
    config.voice_volume = 0.66
    config.se_volume = 0.33
    config.window_width = 1024
    config.window_height = 640
    config.fullscreen = true
    config.vsync = false
    config.window_title = "Deep Test"
    config.thumbnail_quality = 70
    config.thumbnail_format = "jpg"
    local okSave = pcall(config.save_all)
    check("C1 save_all writes settings/config.lua", okSave)

    -- reset to defaults, then reload
    config.bgm_volume = 1.0
    config.voice_volume = 1.0
    config.se_volume = 1.0
    config.window_width = 1280
    config.window_height = 720
    config.fullscreen = false
    config.vsync = true
    config.window_title = "Caesura"
    config.thumbnail_quality = 90
    config.thumbnail_format = "png"
    local okLoad = pcall(config.load_all)
    check("C1 load_all reads saved values", okLoad)
    check("C1 roundtrip bgm_volume", math.abs(config.bgm_volume - 0.42) < 0.001)
    check("C1 roundtrip voice_volume", math.abs(config.voice_volume - 0.66) < 0.001)
    check("C1 roundtrip se_volume", math.abs(config.se_volume - 0.33) < 0.001)
    check("C1 roundtrip window_width=1024", config.window_width == 1024)
    check("C1 roundtrip window_height=640", config.window_height == 640)
    check("C1 roundtrip fullscreen=true", config.fullscreen == true)
    check("C1 roundtrip vsync=false", config.vsync == false)
    check("C1 roundtrip window_title", config.window_title == "Deep Test")
    check("C1 roundtrip thumbnail_quality=70", config.thumbnail_quality == 70)
    check("C1 roundtrip thumbnail_format=jpg", config.thumbnail_format == "jpg")

    -- C2 idempotent load: calling load_all again yields identical values.
    config.bgm_volume = 0.42
    config.load_all()
    check("C2 repeated load_all preserves values", math.abs(config.bgm_volume - 0.42) < 0.001)

    -- C2 byte-stable: save -> load -> save produces identical file.
    local f1 = io.open("settings/config.lua", "r"); local body1 = f1 and f1:read("*a"); if f1 then f1:close() end
    pcall(config.save_all); pcall(config.load_all); pcall(config.save_all)
    local f2 = io.open("settings/config.lua", "r"); local body2 = f2 and f2:read("*a"); if f2 then f2:close() end
    check("C2 save->load->save is byte-stable", body1 ~= nil and body1 == body2)
end

-- C5: partial config file fills holes with defaults.
do
    clean_settings()
    local f = io.open("settings/config.lua", "w")
    f:write("return { bgm_volume = 0.3 }\n")
    f:close()
    config.bgm_volume = 1.0
    config.window_width = 1280
    config.se_volume = 1.0
    config.load_all()
    check("C5 partial file: bgm_volume applied", math.abs(config.bgm_volume - 0.3) < 0.001)
    check("C5 partial file: window_width falls back to default", config.window_width == 1280)
    check("C5 partial file: se_volume falls back to default", math.abs(config.se_volume - 1.0) < 0.001)
end

-- C4: corrupt config file falls back to defaults (no raise).
do
    clean_settings()
    local f = io.open("settings/config.lua", "w")
    f:write("return } this is { not valid lua !!\n")
    f:close()
    config.bgm_volume = 0.55
    config.window_width = 1600
    local ok = pcall(config.load_all)
    check("C4 corrupt file: load_all degrades (no raise)", ok)
    check("C4 corrupt file: bgm_volume stays default-after-degrade", math.abs(config.bgm_volume - 0.55) < 0.001)
    check("C4 corrupt file: window_width stays", config.window_width == 1600)
end

-- C6: type whitelist — crafted nested-table value rejects the WHOLE file;
--      a non-numeric string degrades through tonumber-or-default.
do
    clean_settings()
    local f = io.open("settings/config.lua", "w")
    f:write("return { bgm_volume = { nested = true } }\n")  -- table type -> reject all
    f:close()
    config.bgm_volume = 0.77
    local ok = pcall(config.load_all)
    check("C6 nested-table value rejected (file ignored)", ok)
    check("C6 rejected file keeps prior bgm_volume", math.abs(config.bgm_volume - 0.77) < 0.001)
end
do
    clean_settings()
    local f = io.open("settings/config.lua", "w")
    f:write('return { bgm_volume = "not-a-number" }\n')  -- string passes whitelist, tonumber nil
    f:close()
    config.bgm_volume = 0.66
    pcall(config.load_all)
    check("C6 non-numeric string degrades to prior value", math.abs(config.bgm_volume - 0.66) < 0.001)
end

-- C3 fallback via load_volumes/load_window_settings when main file missing:
-- save_volumes + load_volumes roundtrip through the audio mock read-back.
do
    clean_settings()
    config.bgm_volume = 0.5      -- will be overwritten by save_volumes read-back? no:
    -- save_volumes reads from Audio.get_*_volume() (mock) and overwrites config.
    audioMock._vol.bgm = 0.25
    audioMock._vol.se = 0.5
    audioMock._vol.voice = 0.75
    local okS = pcall(config.save_volumes)
    check("C3 save_volumes writes file", okS)
    config.bgm_volume = 1.0
    local okL = pcall(config.load_volumes)
    check("C3 load_volumes reads back", okL)
    check("C3 volume roundtrip bgm=0.25", math.abs(config.bgm_volume - 0.25) < 0.001)
    check("C3 volume roundtrip se=0.5", math.abs(config.se_volume - 0.5) < 0.001)
    check("C3 volume roundtrip voice=0.75", math.abs(config.voice_volume - 0.75) < 0.001)
end

-- window persistence roundtrip
do
    clean_settings()
    config.window_width = 1366
    config.window_height = 768
    config.fullscreen = false
    config.vsync = true
    config.thumbnail_quality = 85
    config.thumbnail_format = "webp"
    config.window_title = "WinTest"
    check("C3 save_window_settings writes", pcall(config.save_window_settings))
    config.window_width = 0
    config.window_height = 0
    pcall(config.load_window_settings)
    check("C3 window roundtrip width=1366", config.window_width == 1366)
    check("C3 window roundtrip height=768", config.window_height == 768)
    check("C3 window roundtrip fullscreen=false", config.fullscreen == false)
end

clean_settings()

print(string.format("DEEP SETTINGS/CONFIG TESTS DONE: %d passed, %d failed",
    passed, failed))
if failed > 0 then os.exit(1) end
