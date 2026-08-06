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
check("load slot clamped", pl.slot == 0)

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

if failed > 0 then os.exit(1) end
print("SAVELOAD TESTS DONE")
