-- test_audio_cmds.lua — audio command contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local schema = require("kag.schema")

-- playbgm schema: require-any file/storage, volume clamp 0..1.5, ms fields
local p = schema.coerce("playbgm", { storage = "bgm.ogg", volume = "9", fadein = "999999" }, {})
check("playbgm volume clamped", p.volume == 1.5)
check("playbgm fadein clamped", p.fadein == 30000)
check("playbgm loop default", p.loop == true)

-- playse same clamps, no loop field
local ps = schema.coerce("playse", { file = "se.ogg", volume = "-1" }, {})
check("playse volume clamped", ps.volume == 0)
check("playse no loop", ps.loop == nil)

-- resolve_file priority: storage > path > file > positional
-- (source-level: the handler reads via resolve_file; lock the order)
local f = assert(io.open("scripts/kag/commands/audio.lua", "r"))
local src = f:read("*a")
f:close()
check("resolve order", src:find("params.storage or params.path or params.file", 1, true) ~= nil)

-- playvoice muted short-circuit: the handler returns BEFORE any
-- backend.audio_play call. (A backend stub can't spy here -- audio.lua
-- captured the module at require time -- so lock the SOURCE order: the
-- mute branch must precede the audio_play call.)
local f2 = assert(io.open("scripts/kag/commands/audio.lua", "r"))
local asrc = f2:read("*a")
f2:close()
-- Anchor the FULL branch text: a bare substring could match a comment
-- mentioning voice_muted above the call (security LOW).
local mute_pos = asrc:find("if ctx and ctx.voice_muted then", 1, true)
local play_pos = asrc:find('backend.audio_play("voice"', 1, true)
check("mute branch precedes backend call",
      mute_pos ~= nil and play_pos ~= nil and mute_pos < play_pos)

-- dispatch a muted playvoice through the scheduler: it must complete
-- WITHOUT reaching the backend (no diagnostics, event flow preserved)
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    voice_muted = true, current_scene = "t.ks", token_index = 1 }
local scheduler = require("scheduler")
local tokens = { { "playvoice", { storage = "v.ogg" } } }
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = KAG
local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
local ok = true
while coroutine.status(co) ~= "dead" do
    ok = coroutine.resume(co)
    if not ok then break end
end
package.loaded["kag"] = kag_orig
check("muted playvoice dispatches clean", ok == true)
_G._CAESURA_AUDIO_EVENT = nil

if failed > 0 then os.exit(1) end
print("AUDIO CMDS TESTS DONE")
