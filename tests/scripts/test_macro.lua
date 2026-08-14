-- =============================================================================
--  Caesura (AmeKAG) �?tests/scripts/test_macro.lua
--  U1.5: macro record, erase, and expansion tests
--  Run: external\lua\lua.exe tests/scripts/test_macro.lua
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;scripts/kag/commands/?.lua;" .. package.path

-- [FIX] Mock engine globals for standalone lua.exe (no C++ bindings available)
-- These mocks prevent nil-index errors when KAG commands call Render/layers/backend/KAG
if _G.Render == nil then _G.Render = {} end
-- Ensure Render has all methods backend.lua tries to call
_G.Render.create_viewport = function(w, h) return 0 end
_G.Render.destroy_viewport = function(vpId) end
_G.Render.draw_viewport = function(vpId, x, y, w, h) end
_G.Render.fill_viewport = function(handleId, r, g, b, a) end
_G.Render.load_texture = function(file) return 0 end
_G.Render.destroy_texture = function(id) end
_G.Render.create_solid_texture = function(r, g, b, a) return 0 end
_G.Render.submit_batch = function(commands) end
_G.Render.submit_blend = function(cmd) end
_G.Render.submit_transition = function(cmd) end
_G.Render.submit_vfx = function(cmd) end
_G.Render.stretch_blt = function(id, x, y, w, h) end
_G.Render.affine_blt = function(id, x, y, w, h) end
_G.Render.text_set_font = function(face, size, color) end
_G.Render.text_reset_state = function() end
_G.Render.video_play = function(file) return 0 end
_G.Render.video_stop = function() end
_G.Render.video_get_texture = function() return 0 end
_G.Render.video_is_playing = function() return false end
_G.Render.video_has_ended = function() return false end
_G.Render.video_get_size = function() return 0, 0 end
_G.Render.video_pause = function() end
_G.Render.video_resume = function() end
_G.Render.load_texture_async = function(path) return 0 end
_G.Render.cancel_async_loads = function() end
_G.Render.get_resolution = function() return 1280, 720 end

-- Mock KAG global (backend.lua fallback path for clear_text etc.)
if _G.KAG == nil then _G.KAG = {} end
_G.KAG.clear_text = function() end
_G.KAG.render_text = function() end
_G.KAG.render_ruby = function() end
_G.KAG.set_font = function() end
_G.KAG.line_height = function() return 24 end
_G.KAG.show_text = function() end
_G.KAG.show_image = function() end
_G.KAG.clear_screen = function() end
_G.KAG.wait_click = function() end
_G.KAG.play_bgm = function() end
_G.KAG.stop_bgm = function() end
_G.KAG.play_voice = function() end
_G.KAG.stop_voice = function() end
_G.KAG.play_se = function() end
_G.KAG.stop_se = function() end
_G.KAG.play_se_3d = function() end
_G.KAG.set_bus_volume = function() end
_G.KAG.set_bgm_volume = function() end
_G.KAG.set_se_volume = function() end
_G.KAG.set_voice_volume = function() end
_G.KAG.set_global_volume = function() end
_G.KAG.get_global_volume = function() return 0.8 end
_G.KAG.get_bus_volume = function() return 0.8 end
_G.KAG.replay_voice = function() end
_G.KAG.flush_wave_cache = function() end
_G.KAG.set_listener = function() end
_G.KAG.is_voice_playing = function() return false end
_G.KAG.is_bgm_playing = function() return false end
_G.KAG.is_se_playing = function() return false end
_G.KAG.get_active_voices = function() return 0 end
_G.KAG.audio_get_position = function() return 0 end
_G.KAG.audio_get_length = function() return 0 end
_G.KAG.audio_fade_volume = function() end
_G.KAG.quake = function() end
_G.KAG.log = function() end

if _G.layers == nil then
    _G.layers = {
        get = function() return nil end,
        add_layer = function() return {id=1} end,
        set_z = function() end,
        remove = function() end,
      required = function() return {} end,
    }
end
if type(_G.backend) ~= "table" then
    _G.backend = {
        clear_text = function() end,
        render_text = function() end,
        line_height = function() return 24 end,
        set_input_focus = function() end,
        get_input_focus = function() return "KAG" end,
        create_solid_texture = function() return 0 end,
        destroy_texture = function() end,
        draw_viewport = function() end,
        fill_viewport = function() end,
        create_viewport = function() return 0 end,
        destroy_viewport = function() end,
        load_texture = function() return 0 end,
        submit_batch = function() end,
        submit_blend = function() end,
        submit_transition = function() end,
        submit_vfx = function() end,
        stretch_blt = function() end,
        affine_blt = function() end,
        video_play = function() end,
        video_stop = function() end,
        video_update = function() end,
        video_get_texture = function() return 0 end,
        video_is_playing = function() return false end,
        video_has_ended = function() return false end,
        video_get_size = function() return 0, 0 end,
        video_pause = function() end,
        video_resume = function() end,
        load_texture_async = function() end,
        cancel_async_loads = function() end,
        audio_play = function() return 0 end,
        audio_stop = function() end,
        audio_fade_volume = function() end,
        audio_set_bus_volume = function() end,
        audio_get_bus_volume = function() return 0.8 end,
        audio_set_listener = function() end,
        audio_xfade = function() end,
        play_bgm = function() end,
        stop_bgm = function() end,
        play_voice = function() end,
        stop_voice = function() end,
        play_se = function() end,
        stop_se = function() end,
        set_global_volume = function() end,
        get_global_volume = function() return 0.8 end,
        is_voice_playing = function() return false end,
        is_bgm_playing = function() return false end,
        is_se_playing = function() return false end,
        audio_get_position = function() return 0 end,
        audio_get_length = function() return 0 end,
    }
end

local scheduler = require("scheduler")
local tokenizer = require("tokenizer")
local passed, failed = 0, 0

local function assert_eq(actual, expected, msg)
    if actual == expected then
        passed = passed + 1
    else
        failed = failed + 1
        print(string.format("FAIL: %s (expected %s, got %s)", msg, tostring(expected), tostring(actual)))
    end
end

local function run_script(script_text)
    local tokens = tokenizer.parse(script_text)
    local ctx = {}
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do
        local ok, err = coroutine.resume(co)
        if not ok then
            print("  Scheduler error:", err)
            return nil
        end
    end
    return ctx
end

-- ══════════════════════════════════════════════════════════════════════════�?-- Test 1: macro records body
-- ══════════════════════════════════════════════════════════════════════════�?
do
    local ctx = run_script([=[
[macro name="greet"]
[text text="Hello!"]
[p]
[endmacro]
]=])
    if ctx then
        assert_eq(ctx.macros ~= nil, true, "macros should exist")
        if ctx.macros then
            assert_eq(ctx.macros["greet"] ~= nil, true, "macro greet should be recorded")
            if ctx.macros["greet"] then
                assert_eq(#ctx.macros["greet"], 2, "macro body should have 2 tokens")
                assert_eq(ctx.macros["greet"][1][1], "text", "first body token should be text")
                assert_eq(ctx.macros["greet"][2][1], "p", "second body token should be p")
            end
        end
    else
        failed = failed + 1
        print("FAIL: scheduler crashed on macro recording")
    end
end

-- ══════════════════════════════════════════════════════════════════════════�?-- Test 2: erasemacro removes macro
-- ══════════════════════════════════════════════════════════════════════════�?
do
    local ctx = run_script([=[
[macro name="temp"]
[text text="body"]
[endmacro]
[erasemacro name="temp"]
]=])
    if ctx then
        assert_eq(ctx.macros ~= nil, true, "macros should exist after recording")
        assert_eq(ctx.macros["temp"], nil, "macro temp should be erased")
    else
        failed = failed + 1
        print("FAIL: scheduler crashed on erasemacro")
    end
end

-- ══════════════════════════════════════════════════════════════════════════�?-- Test 3: macro expansion executes body
-- ══════════════════════════════════════════════════════════════════════════�?
do
    local ctx = run_script([=[
[macro name="doneflag"]
[eval exp="done = true"]
[endmacro]
[doneflag]
[setflag]
]=])
    if ctx then
        assert_eq(ctx.macros ~= nil, true, "macros should exist")
        if ctx.macros then
            assert_eq(ctx.macros["doneflag"] ~= nil, true, "macro doneflag should be recorded")
        end
        assert_eq(ctx.backlog and #ctx.backlog == 1, true, "macro expansion should create backlog entry")
    else
        failed = failed + 1
        print("FAIL: scheduler crashed on macro expansion")
    end
end

-- ══════════════════════════════════════════════════════════════════════════�?-- Test 4: macro without name is not recorded
-- ══════════════════════════════════════════════════════════════════════════�?
do
    local ctx = run_script([=[
[macro]
[text text="no name"]
[endmacro]
]=])
    if ctx then
        local empty = (ctx.macros == nil) or (next(ctx.macros) == nil)
        assert_eq(empty, true, "no macro should be recorded without name")
    else
        failed = failed + 1
        print("FAIL: scheduler crashed on unnamed macro")
    end
end

-- ══════════════════════════════════════════════════════════════════════════�?

-- ══════════════════════════════════════════════════════════════════════════
--  Neo-Genesis: parameterized macro args (%who% substitution)
-- ══════════════════════════════════════════════════════════════════════════
do
    local scheduler = require("scheduler")
    local tokens = {
        { "macro", { name = "say_hi", args = "who,what" } },
        { "ch", { name = "%who%", text = "%what%" } },
        { "endmacro" },
        { "say_hi", { who = "Sakura", what = "Hello!" } },
        { "say_hi", { who = "Kaito", what = "Yo!" } },
    }
    local dispatched = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(ctx, params)
            dispatched[#dispatched + 1] = { k, params }
        end
    end})
    local ctx = {
        macros = nil, macro_args = nil, f = {},
        current_scene = "test.ks", token_index = 1,
    }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig

    local ok = true
    if #dispatched ~= 2 then ok = false
    elseif dispatched[1][2].name ~= "Sakura" or dispatched[1][2].text ~= "Hello!" then ok = false
    elseif dispatched[2][2].name ~= "Kaito" or dispatched[2][2].text ~= "Yo!" then ok = false
    elseif ctx.macros.say_hi[1][2].name ~= "%who%" then ok = false
    end
    if ok then passed = passed + 1 else
        failed = failed + 1
        print("FAIL: parameterized macro args")
    end
end

-- Results
-- ══════════════════════════════════════════════════════════════════════════�?
print(string.format("\nMacro tests: %d passed, %d failed", passed, failed))
if failed > 0 then
    error("macro: " .. failed .. " checks failed")
end
