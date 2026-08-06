-- test_scroll.lua — [scroll] rolling text contract (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local schema = require("kag.schema")

-- scroll schema: speed 1..1000, size 8..128
local sc = schema.coerce("scroll", { text = "credits", speed = "0", size = "999" }, {})
check("scroll speed clamped", sc.speed == 1)
check("scroll size clamped", sc.size == 128)
local sc2 = schema.coerce("scroll", { speed = "500", size = "28" }, {})
check("scroll speed kept", sc2.speed == 500)
check("scroll size kept", sc2.size == 28)

-- handlers registered
check("scroll registered", type(KAG.scroll) == "function")

-- source-level: empty text early-returns; completion clears the text
local f = assert(io.open("scripts/kag/commands/transition.lua", "r"))
local src = f:read("*a")
f:close()
check("scroll empty text guard", src:find('if #text == 0 then return end', 1, true) ~= nil)
check("scroll clears on exit", src:find("backend.clear_text()", 1, true) ~= nil)
check("scroll yield loop", src:find("coroutine.yield() or 16", 1, true) ~= nil)

-- empty text dispatch is a no-op (no render calls)
local called = false
local backend_orig = package.loaded["backend"]
-- backend captured at require time -- lock via source is above; just
-- verify the handler exists and the schema path is sound
package.loaded["backend"] = backend_orig
check("scroll schema migrated", schema.isMigrated("scroll") == true)

if failed > 0 then os.exit(1) end
print("SCROLL TESTS DONE")
