-- =============================================================================
--  test_kag_commands.lua -- Comprehensive KAG command unit tests
--  Covers: button, text, audio, layer, transition, system, save, vfx, video
-- =============================================================================

package.path = "scripts/?.lua;scripts/?/init.lua;" .. package.path

-- Mock C++ bindings
_G.KAG = {
    play_bgm=function()return true end, play_voice=function()return true end,
    play_se=function()return true end, stop_bgm=function()return true end,
    stop_voice=function()return true end, stop_se=function()return true end,
    set_bus_volume=function()return true end, get_bus_volume=function()return 0.8 end,
    flush_wave_cache=function()return true end, show_text=function()return true end,
    show_image=function()return true end, clear_screen=function()return true end,
    wait_click=function()return true end, render_text=function()return true end,
    render_ruby=function()return true end, clear_text=function()return true end,
    set_font=function()return true end, line_height=function()return 24 end,
    set_listener=function()return true end, is_voice_playing=function()return false end,
    is_bgm_playing=function()return false end, get_active_voices=function()return 0 end,
    log=function()end, clear_text_layer=function()return true end,
    set_bgm_volume=function()return true end, set_se_volume=function()return true end,
    set_voice_volume=function()return true end, audio_get_position=function()return 0 end,
    audio_get_length=function()return 0 end, audio_fade_volume=function()return true end,
    quake=function()return true end, save_game=function()return true end,
    load_game=function()return nil,"mock"end, list_saves=function()return{}end,
    play_se_3d=function()return true end, set_global_volume=function()end,
    get_global_volume=function()return 1.0 end, replay_voice=function()end,
}
_G.Render = {}
_G.DevCore = {}
_G.Engine = {select_platform_backend=function()return true end}

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
        print(string.format("  [PASS] %s", name))
    else
        failed = failed + 1
        print(string.format("  [FAIL] %s  -- %s", name, detail or ""))
    end
end

local function make_ctx()
    return {
        f = {}, sf = {}, tf = {},
        layers = {}, backlog = {}, active_operations = {},
        tokens = {}, token_index = 1, call_stack = {},
        waiting_input = false, _choiceMode = false,
        _selectedChoice = nil, _pendingJump = nil,
    }
end

print("\n=== KAG Commands Test Suite ===\n")

-- ¨T¨T¨T Layer Commands ¨T¨T¨T
print("--- Layer Commands ---")
do
    local LayerCommands = require("kag.commands.layer")
    local ctx = make_ctx()
    
    check("bg handler exists", type(LayerCommands.bg) == "function")
    check("fg handler exists", type(LayerCommands.fg) == "function")
    check("cl handler exists", type(LayerCommands.cl) == "function")
    check("image handler exists", type(LayerCommands.image) == "function")
    check("position handler exists", type(LayerCommands.position) == "function")
    check("layopt handler exists", type(LayerCommands.layopt) == "function")
end

-- ¨T¨T¨T Text Commands ¨T¨T¨T
print("\n--- Text Commands ---")
do
    local TextCommands = require("kag.commands.text")
    local ctx = make_ctx()
    
    check("ch handler exists", type(TextCommands.ch) == "function")
    check("text handler exists", type(TextCommands.text) == "function")
    check("l handler exists", type(TextCommands.l) == "function")
    check("r handler exists", type(TextCommands.r) == "function")
    check("er handler exists", type(TextCommands.er) == "function")
    check("p handler exists", type(TextCommands.p) == "function")
    check("ruby handler exists", type(TextCommands.ruby) == "function")
    check("font handler exists", type(TextCommands.font) == "function")
    check("skip handler exists", type(TextCommands.skip) == "function")
    check("reset handler exists", type(TextCommands.reset) == "function")
    check("pt handler exists", type(TextCommands.pt) == "function")
    
    -- [button] / [endbutton]
    check("button handler exists", type(TextCommands.button) == "function")
    check("endbutton handler exists", type(TextCommands.endbutton) == "function")
end

-- ¨T¨T¨T Audio Commands ¨T¨T¨T
print("\n--- Audio Commands ---")
do
    local AudioCommands = require("kag.commands.audio")
    
    check("playbgm handler exists", type(AudioCommands.playbgm) == "function")
    check("stopbgm handler exists", type(AudioCommands.stopbgm) == "function")
    check("playse handler exists", type(AudioCommands.playse) == "function")
    check("playvoice handler exists", type(AudioCommands.playvoice) == "function")
    check("fadebgm handler exists", type(AudioCommands.fadebgm) == "function")
    check("xfadebgm handler exists", type(AudioCommands.xfadebgm) == "function")
    check("setbgmvolume handler exists", type(AudioCommands.setbgmvolume) == "function")
    check("waitsound handler exists", type(AudioCommands.waitsound) == "function")
    check("stopse handler exists", type(AudioCommands.stopse) == "function")
    check("playbgmstop handler exists", type(AudioCommands.playbgmstop) == "function")
end

-- ¨T¨T¨T System Commands ¨T¨T¨T
print("\n--- System Commands ---")
do
    local SystemCommands = require("kag.commands.system")
    
    check("wait handler exists", type(SystemCommands.wait) == "function")
    check("emb handler exists", type(SystemCommands.emb) == "function")
    check("eval handler exists", type(SystemCommands.eval) == "function")
    check("history handler exists", type(SystemCommands.history) == "function")
end

-- ¨T¨T¨T Transition Commands ¨T¨T¨T
print("\n--- Transition Commands ---")
do
    local TransCommands = require("kag.commands.transition")
    
    check("trans handler exists", type(TransCommands.trans) == "function")
    check("move handler exists", type(TransCommands.move) == "function")
    check("quake handler exists", type(TransCommands.quake) == "function")
    check("fade handler exists", type(TransCommands.fade) == "function")
end

-- ¨T¨T¨T VFX Commands ¨T¨T¨T
print("\n--- VFX Commands ---")
do
    local VFXCommands = require("kag.commands.vfx")
    check("vfx handler exists", type(VFXCommands.vfx) == "function")
end

-- ¨T¨T¨T Video Commands ¨T¨T¨T
print("\n--- Video Commands ---")
do
    local VideoCommands = require("kag.commands.video")
    check("video handler exists", type(VideoCommands.video) == "function")
    check("stopvideo handler exists", type(VideoCommands.stopvideo) == "function")
end

-- ¨T¨T¨T Save Commands ¨T¨T¨T
print("\n--- Save Commands ---")
do
    local SaveCommands = require("kag.commands.save")
    check("save handler exists", type(SaveCommands.save) == "function")
    check("load handler exists", type(SaveCommands.load) == "function")
    check("listsaves handler exists", type(SaveCommands.listsaves) == "function")
end

-- ¨T¨T¨T Resource Commands ¨T¨T¨T
print("\n--- Resource Commands ---")
do
    local ResourceCommands = require("kag.commands.resource")
    check("preload handler exists", type(ResourceCommands.preload) == "function")
    check("flush_cache exists", type(ResourceCommands.flush_cache) == "function")
end

-- ¨T¨T¨T KAG Module Integration ¨T¨T¨T
print("\n--- KAG Module Integration ---")
do
    local KAG = require("kag")
    
    -- All key commands should be registered
    check("KAG.bg registered", type(KAG.bg) == "function")
    check("KAG.ch registered", type(KAG.ch) == "function")
    check("KAG.playbgm registered", type(KAG.playbgm) == "function")
    check("KAG.video registered", type(KAG.video) == "function")
    check("KAG.save registered", type(KAG.save) == "function")
    check("KAG.trans registered", type(KAG.trans) == "function")
    check("KAG.vfx registered", type(KAG.vfx) == "function")
    check("KAG.preload registered", type(KAG.preload) == "function")
    
    -- Legacy aliases
    check("KAG.showtext alias", type(KAG.showtext) == "function")
    check("KAG.clearscreen alias", type(KAG.clearscreen) == "function")
    check("KAG.cancel exists", type(KAG.cancel) == "function")
end

-- ¨T¨T¨T Results ¨T¨T¨T
print("\n" .. string.rep("=", 50))
print(string.format("  KAG Commands: %d passed, %d failed, %d total",
    passed, failed, passed + failed))
print(string.rep("=", 50))
if failed > 0 then os.exit(1) end
