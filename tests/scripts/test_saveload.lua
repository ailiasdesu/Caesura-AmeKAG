-- test_saveload.lua — [saveload] menu routing + slot bounds (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local SaveCommands = KAG

-- save/load schemas clamp slot to 0..99
local schema = require("kag.schema")
local ps = schema.coerce("save", { slot = "500" }, {})
check("save slot clamped", ps.slot == 99)
local pl = schema.coerce("load", { slot = "-5" }, {})
-- min is -2 (system slots flow through); deeper negatives clamp to -2
check("load slot min -2", pl.slot == -2)

-- [saveload] is REGISTERED (audit fix: it existed in SaveCommands but
-- kag.lua never bound it -- .ks scripts hit nil)
check("saveload registered", type(KAG.saveload) == "function")
check("saveplace registered", type(KAG.saveplace) == "function")
check("loadplace registered", type(KAG.loadplace) == "function")

-- routing contract: action == "save" -> save command, else load (the
-- runtime path needs the C++ KAG bindings, so lock the source shape)
local f = assert(io.open("scripts/kag/commands/save.lua", "r"))
local src = f:read("*a")
f:close()
check("saveload save branch",
      src:find('chosen.action == "save"', 1, true) ~= nil
      and src:find('SaveCommands.save(ctx, { slot = chosen.slot })', 1, true) ~= nil)
check("saveload load branch",
      src:find('SaveCommands.load(ctx, { slot = chosen.slot })', 1, true) ~= nil)
check("saveload no-choice guard",
      src:find("if chosen then", 1, true) ~= nil)

-- visual/text state round-trip (audit: capture + restore font/style)
local Save = package.loaded["kag.commands.save"] or require("kag.commands.save")
local ctxV = { f = { hero = 1 }, sf = {}, tf = {}, mp = {}, variables = {},
    token_index = 5, current_scene = "s.ks", backlog = {},
    seen_scenes = {}, seen_endings = {}, saveDescription = "v",
    text_state = { font_face = "serif", font_size = 30, font_color = "255,0,0" },
    textbox_style = { x = 0, y = 520, w = 1280, h = 200,
        color = "0,0,0", opacity = 200, visible = true } }
local captured = Save.capture_state(ctxV)
check("text_state captured", captured.text_state
      and captured.text_state.font_face == "serif"
      and captured.text_state.font_size == 30)
check("textbox_style captured", captured.textbox_style
      and captured.textbox_style.w == 1280)
-- restore is inline in load(); drive it end-to-end via the mock
local loaded_state = captured
local kag_backup = _G.KAG
_G.KAG = { load_game = function() return loaded_state, {} end,
    save_game = function() return true end }
local ctxR = { f = {}, sf = {}, tf = {}, mp = {}, variables = {},
    tokens = {}, token_index = 1 }
pcall(Save.load, ctxR, { slot = 0 })
_G.KAG = kag_backup
check("font restored", ctxR.text_state and ctxR.text_state.font_face == "serif"
      and ctxR.text_state.font_size == 30)
check("style restored", ctxR.textbox_style and ctxR.textbox_style.w == 1280
      and ctxR.textbox_style.opacity == 200)
-- crafted non-table visual state cannot crash load
_G.KAG = { load_game = function() return { text_state = "evil", textbox_style = 42 }, {} end }
local ctxC = { f = {}, sf = {}, tf = {}, mp = {}, variables = {}, tokens = {} }
local okC = pcall(Save.load, ctxC, { slot = 0 })
_G.KAG = kag_backup
check("crafted visual safe", okC)
-- crafted FIELD types also safe (review warn: h string / opacity table
-- used to flow into ctx.textbox_style and crash the next [cl] rebuild)
_G.KAG = { load_game = function()
    return { textbox_style = { w = 1280, h = "wide", x = 0, y = 520, opacity = {} } }, {} end }
local ctxD = { f = {}, sf = {}, tf = {}, mp = {}, variables = {}, tokens = {} }
local okD = pcall(Save.load, ctxD, { slot = 0 })
_G.KAG = kag_backup
check("crafted fields safe", okD and ctxD.textbox_style == nil)

if failed > 0 then os.exit(1) end
print("SAVELOAD TESTS DONE")
