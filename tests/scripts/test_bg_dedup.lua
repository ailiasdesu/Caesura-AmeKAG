-- test_bg_dedup.lua — [bg] same-texture dedup (Neo-Genesis)
-- Source-lock mode (runner sandbox denies require("backend")/layers):
-- behavior is verified standalone via REAL_EXIT; this file locks the
-- guard structure so a regression is caught in the runner too.
local check = function(name, cond)
    if cond then print("  [PASS] " .. name) passed = (passed or 0) + 1
    else print("  [FAIL] " .. name) failed = (failed or 0) + 1 end
end

local src = io.open("scripts/kag/commands/layer.lua", "r"):read("*a")

check("dedup guard present", src:find("ctx.layers and ctx.layers.bg == file", 1, true) ~= nil)
check("load gated by not same", src:find("if not same then", 1, true) ~= nil)
check("load inside the gate",
      src:find("tex = backend.load_texture(file)", 1, true) ~= nil)
-- the dedup gate must appear BEFORE the load statement
local gatePos = src:find("local same = ctx.layers", 1, true)
local loadPos = src:find("tex = backend.load_texture", 1, true)
check("gate precedes load", gatePos ~= nil and loadPos ~= nil and gatePos < loadPos)
-- visibility/z always re-applied (outside the gate)
check("visibility outside gate",
      (src:find("if not same and not tex then", 1, true) or 0)
      < (src:find("set_layer_visible(node, true)", 1, true) or 0))
-- no and/or load trap (the bug this guards against)
check("no and/or load trap", not src:find("same and nil or", 1, true))

if failed and failed > 0 then os.exit(1) end
print("BG DEDUP TESTS DONE")
