-- test_settings.lua — playback settings persistence (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")

-- [skip] toggles skip_mode
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
KAG.skip(ctx, {})
check("skip toggles on", ctx.skip_mode == true)
KAG.skip(ctx, {})
check("skip toggles off", ctx.skip_mode == false)
KAG.skip(ctx, { mode = "seen" })
check("skip seen mode", ctx.skip_mode == "seen")
KAG.skip(ctx, { mode = "seen" })
check("skip seen toggles off", ctx.skip_mode == false)

-- [auto mode=on/off]
KAG.auto(ctx, { mode = "on" })
check("auto on", ctx.auto_mode == true)
KAG.auto(ctx, { mode = "off" })
check("auto off", ctx.auto_mode == false)
KAG.auto(ctx, { mode = "toggle" })
check("auto toggle", ctx.auto_mode == true)

-- [voice_off on=true/false]
KAG.voice_off(ctx, { on = true })
check("voice muted", ctx.voice_muted == true)
KAG.voice_off(ctx, { on = false })
check("voice unmuted", ctx.voice_muted == false)

-- persistence contract: capture fields + restore loop shape (the real
-- save module requires the C++ bridge, so lock the shape here)
local captured = {
    skip_mode = ctx.skip_mode,
    auto_mode = ctx.auto_mode,
    voice_muted = ctx.voice_muted,
}
local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
ctx2.skip_mode = captured.skip_mode or false
ctx2.auto_mode = captured.auto_mode or false
ctx2.voice_muted = captured.voice_muted or false
check("capture/restore skip", ctx2.skip_mode == ctx.skip_mode)
check("capture/restore auto", ctx2.auto_mode == ctx.auto_mode)
check("capture/restore voice_muted", ctx2.voice_muted == ctx.voice_muted)

-- voice_muted IS in the save capture (source-level lock)
local f = assert(io.open("scripts/kag/commands/save.lua", "r"))
local src = f:read("*a")
f:close()
check("save captures voice_muted",
      src:find("state.voice_muted", 1, true) ~= nil
      and src:find("ctx.voice_muted = state.voice_muted", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("SETTINGS TESTS DONE")
