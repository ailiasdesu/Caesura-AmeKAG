-- test_reset_pt.lua — [reset]/[pt] state commands (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local schema = require("kag.schema")

-- pt schema: speed 8..5000
local p = schema.coerce("pt", { speed = "3" }, {})
check("pt speed clamped min", p.speed == 8)
local p2 = schema.coerce("pt", { speed = "99999" }, {})
check("pt speed clamped max", p2.speed == 5000)
local p3 = schema.coerce("pt", { speed = "60" }, {})
check("pt speed kept", p3.speed == 60)

-- handlers registered
check("pt registered", type(KAG.pt) == "function")
check("reset registered", type(KAG.reset) == "function")

-- source-level: reset clears reveal + resets text state
local f = assert(io.open("scripts/kag/commands/text.lua", "r"))
local src = f:read("*a")
f:close()
check("reset clears reveal", src:find("ctx.reveal = nil", 1, true) ~= nil)
check("reset text state", src:find("backend.text_reset_state()", 1, true) ~= nil)
check("reset scene reset", src:find("TextScene.reset(ctx)", 1, true) ~= nil)

-- pt writes text_speed (schema-typed)
check("pt writes text_speed", src:find("ctx.text_speed = params.speed", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("RESET PT TESTS DONE")
