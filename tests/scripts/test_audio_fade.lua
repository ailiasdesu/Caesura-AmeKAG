-- test_audio_fade.lua — [fadebgm]/[xfadebgm] contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local Audio = package.loaded["kag.commands.audio"] or require("kag.commands.audio")
local calls = {}
local real_backend = _G._CAESURA_BACKEND
_G._CAESURA_BACKEND = { render = function() return true end,
    audio = function(cmd, ...)
        if cmd == "fade_volume" then calls[#calls + 1] = { "fade", ... } end
        if cmd == "xfade" then calls[#calls + 1] = { "xfade", ... } end
        return true
    end,
}
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    viewport = { width = 1280, height = 720 } }

-- fadebgm delegates ms -> seconds
Audio.fadebgm(ctx, { volume = 0.3, time = 2000 })
check("fadebgm delegates", calls[1] and calls[1][1] == "fade"
      and calls[1][2] == "bgm" and calls[1][3] == 0.3 and calls[1][4] == 2.0)

-- xfadebgm routes file + seconds
calls = {}
Audio.xfadebgm(ctx, { storage = "next.ogg", time = 3000 })
check("xfadebgm delegates", calls[1] and calls[1][1] == "xfade"
      and calls[1][2] == "bgm" and calls[1][3] == "next.ogg" and calls[1][4] == 3.0)

-- xfadebgm with NO file: no backend call (audit fix)
calls = {}
Audio.xfadebgm(ctx, { time = 1000 })
check("xfade no-file safe", #calls == 0)

-- schema clamps
local schema = require("kag.schema")
local c = schema.coerce("xfadebgm", { storage = "a.ogg", time = "999999" }, {})
check("xfade time clamped", c.time == 30000)
local c2 = schema.coerce("fadebgm", { volume = "9", time = "1" }, {})
check("fadebgm volume clamped", c2.volume == 1.5)

-- playse volume schema clamp
local c3 = schema.coerce("playse", { storage = "s.wav", volume = "9" }, {})
check("playse volume clamped", c3.volume == 1.5)

_G._CAESURA_BACKEND = real_backend

-- bare file (audit): [xfadebgm next.ogg] -> params[1]
-- (re-arm the mock: the earlier restore at line 48 removed it)
_G._CAESURA_BACKEND = { render = function() return true end,
    audio = function(cmd, ...)
        calls[#calls + 1] = { cmd, ... }
        return true end }
calls = {}
Audio.xfadebgm(ctx, { "next.ogg", time = 1000 })
check("xfade bare file", calls[1] and calls[1][3] == "next.ogg")
-- typo'd named param: params[1] is a pair table -- must not crash
calls = {}
Audio.xfadebgm(ctx, { { "storag", "x.ogg" }, time = 1000 })
check("xfade typo pair safe", #calls == 0)
_G._CAESURA_BACKEND = real_backend

if failed > 0 then os.exit(1) end
print("AUDIO FADE TESTS DONE")
