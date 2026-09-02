-- test_layfade_contract.lua — [layfade] contract unification (M1-F).
-- The schema declares `to` (0..255 byte, default 255) but the handler
-- only read opacity/alpha and bailed with "opacity required" --
-- [layfade layer="bg" to=0 time=200] (showcase:48) was a silent no-op.
-- This test locks: schema to/opacity/alpha typing, the handler reading
-- `to` (byte unit, NO fraction scaling -- to=1 stays 1), the legacy
-- opacity/alpha hybrid path unchanged (<=1 fraction, >1 byte), and the
-- all-absent behavior (= schema default 255, real fade to opaque).
-- Style: real layer tree + Schema.coerce, like test_fadeout.lua.
package.path = "scripts/?.lua;scripts/?/init.lua;scripts/kag/?.lua;scripts/kag/commands/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local Schema   = require("kag.schema")
local LayerCmds = require("kag.commands.layer")
local layers   = require("layers")
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }

local idx = 0
local function fresh_node(initialOpacity)
    idx = idx + 1
    return layers.add_layer(nil, {
        name = "_lfx" .. idx, layer_type = 0,
        x = 0, y = 0, w = 10, h = 10,
        visible = true, opacity = initialOpacity or 255 })
end

-- ---- 1. schema: to (0..255, default 255), opacity/alpha declared ----
local c0 = Schema.coerce("layfade", {}, ctx)
check("schema to default 255", c0.to == 255)
local cOp = Schema.coerce("layfade", { opacity = "0.5" }, ctx)
check("schema opacity typed number", cOp.opacity == 0.5)
local cAl = Schema.coerce("layfade", { alpha = "128" }, ctx)
check("schema alpha typed number", cAl.alpha == 128)
local cTo0 = Schema.coerce("layfade", { to = "0" }, ctx)
check("schema to=0 valid string", cTo0.to == 0)

-- ---- 2. to=0: real fade to transparent (showcase: the broken case) ----
local n1 = fresh_node(255)
local p1 = Schema.coerce("layfade", { layer = n1.name, to = "0", time = 1 }, ctx)
local ok1 = pcall(LayerCmds.layfade, ctx, p1)
check("to=0 fades to opacity 0", ok1 and n1.opacity == 0)

-- ---- 3. to=255: real fade to opaque (from transparent) ----
local n2 = fresh_node(0)
local p2 = Schema.coerce("layfade", { layer = n2.name, to = "255", time = 1 }, ctx)
local ok2 = pcall(LayerCmds.layfade, ctx, p2)
check("to=255 fades to opacity 255", ok2 and n2.opacity == 255)

-- ---- 4. to byte unit: to=1 stays 1 (no fraction scaling) ----
local n2b = fresh_node(255)
local p2b = Schema.coerce("layfade", { layer = n2b.name, to = "1", time = 1 }, ctx)
local ok2b = pcall(LayerCmds.layfade, ctx, p2b)
check("to=1 stays byte 1", ok2b and n2b.opacity == 1)

-- ---- 5. opacity/alpha legacy hybrid path unchanged ----
local n3 = fresh_node(255)
local ok3 = pcall(LayerCmds.layfade, ctx, { layer = n3.name, opacity = 0.5, time = 1 })
check("opacity=0.5 fraction -> 128", ok3 and n3.opacity == 128)
local n4 = fresh_node(255)
local ok4 = pcall(LayerCmds.layfade, ctx, { layer = n4.name, opacity = 128, time = 1 })
check("opacity=128 byte passthrough", ok4 and n4.opacity == 128)
local n5 = fresh_node(255)
local ok5 = pcall(LayerCmds.layfade, ctx, { layer = n5.name, alpha = 0, time = 1 })
check("alpha=0 alias", ok5 and n5.opacity == 0)
local n6 = fresh_node(255)
local ok6 = pcall(LayerCmds.layfade, ctx,
    { layer = n6.name, opacity = "0.5", time = 1 })
check("opacity string param safe", ok6 and n6.opacity == 128)

-- ---- 6. all three absent: schema default 255 = real fade to opaque ----
local n7 = fresh_node(0)
local p7 = Schema.coerce("layfade", { layer = n7.name, time = 1 }, ctx)
local ok7 = pcall(LayerCmds.layfade, ctx, p7)
check("all absent -> schema default 255", ok7 and p7.to == 255 and n7.opacity == 255)

-- ---- 7. bare call without coerce: guard stays (node untouched) ----
local n8 = fresh_node(255)
local ok8 = pcall(LayerCmds.layfade, ctx, { layer = n8.name, time = 1 })
check("bare call guard: no crash, no fade", ok8 and n8.opacity == 255)

if failed > 0 then os.exit(1) end
print("LAYFADE CONTRACT TESTS DONE (" .. passed .. " passed)")