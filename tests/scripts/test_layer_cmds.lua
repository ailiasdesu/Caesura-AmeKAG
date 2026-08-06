-- test_layer_cmds.lua — [bg]/[fg]/[moveto]/[position] contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local schema = require("kag.schema")

-- bg/fg require storage or file
local b = schema.coerce("bg", { storage = "bg01.png" }, {})
check("bg storage kept", b.storage == "bg01.png")

-- moveto registered (KAG3 alias)
check("moveto registered", type(KAG.moveto) == "function")

-- moveto maps left/top onto x/y (source-level: the alias fields)
local f = assert(io.open("scripts/kag/commands/layer.lua", "r"))
local src = f:read("*a")
f:close()
check("moveto left mapping", src:find("params.left or params.x or 0", 1, true) ~= nil)
check("moveto top mapping", src:find("params.top or params.y or 0", 1, true) ~= nil)
check("moveto delegates set_position", src:find("layers.set_position(layerName, x, y, scale, unit)", 1, true) ~= nil)

-- bg dedup contract: same file skips reload (source-level lock)
check("bg dedup guard", src:find("ctx.layers and ctx.layers.bg == file", 1, true) ~= nil)
check("bg visibility re-applied", src:find("layers.set_layer_visible(node, true)", 1, true) ~= nil)

-- image schema clamps w/h to 8192
local im = schema.coerce("image", { storage = "x.png", w = "99999", h = "-1" }, {})
check("image w clamped", im.w == 8192)
check("image h clamped", im.h == 0)

if failed > 0 then os.exit(1) end
print("LAYER CMDS TESTS DONE")
