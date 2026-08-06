-- test_sprite_family.lua — [sprite_*] family contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local schema = require("kag.schema")

-- sprite_fade: to 0..255, time 0..30000
local sf = schema.coerce("sprite_fade", { speaker = "A", to = "999", time = "-1" }, {})
check("sprite_fade to clamped", sf.to == 255)
check("sprite_fade time clamped", sf.time == 0)

-- sprite_scale: scale 0.1..4.0
local ss = schema.coerce("sprite_scale", { speaker = "A", scale = "99" }, {})
check("sprite_scale clamped", ss.scale == 4.0)
local ss2 = schema.coerce("sprite_scale", { speaker = "A", scale = "-5" }, {})
check("sprite_scale min clamped", ss2.scale == 0.1)

-- handlers registered
check("sprite_fade registered", type(KAG.sprite_fade) == "function")
check("sprite_move registered", type(KAG.sprite_move) == "function")
check("sprite_scale registered", type(KAG.sprite_scale) == "function")
check("sprite_swap registered", type(KAG.sprite_swap) == "function")

-- source-level: _char_ layer naming + missing-layer defense + cancel
local f = assert(io.open("scripts/kag/commands/text.lua", "r"))
local src = f:read("*a")
f:close()
check("char layer naming", src:find('"_char_" .. (params.speaker or "")', 1, true) ~= nil)
check("missing-layer defense", src:find("no sprite layer", 1, true) ~= nil)
check("cancel pattern", src:find("operation <close> = require(\"kag.operation\").start(ctx)", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("SPRITE FAMILY TESTS DONE")
