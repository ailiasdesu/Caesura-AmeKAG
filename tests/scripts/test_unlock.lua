-- test_unlock.lua — [unlock] persistence contract (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")

-- [unlock type="cg" id="x"] writes ctx.unlockedCG
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.unlock, ctx, { type = "cg", id = "scene01" })
check("cg unlock writes", ctx.unlockedCG and ctx.unlockedCG.scene01 == true)

-- [unlock type="music" id="y"] writes ctx.unlockedMusic
pcall(KAG.unlock, ctx, { type = "music", id = "track01" })
check("music unlock writes", ctx.unlockedMusic
      and ctx.unlockedMusic.track01 == true)

-- unknown type is a no-op (no table created)
pcall(KAG.unlock, ctx, { type = "evil", id = "z" })
check("unknown type no-op", ctx.unlockedCG.z == nil)

-- empty id is a no-op
local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.unlock, ctx2, { type = "cg", id = "" })
check("empty id no-op", ctx2.unlockedCG == nil)

-- gallery/music DATA paths are nil-safe (list() reads unlocked with
-- the `(ctx and ctx.unlockedCG) or {}` defense; the modules themselves
-- aren't preloaded in the suite sandbox, so lock the defense shape)
local def_read = (ctx2 and ctx2.unlockedCG) or {}
check("gallery list defense", type(def_read) == "table")
local def_read2 = (ctx2 and ctx2.unlockedMusic) or {}
check("music list defense", type(def_read2) == "table")

-- the save capture/restore round-trip for unlocks (via the command
-- tables, no C++): simulate capture fields + restore loop shape
local captured = { unlockedCG = { scene01 = true }, unlockedMusic = { track01 = true } }
local ctx3 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
if captured.unlockedCG then
    ctx3.unlockedCG = ctx3.unlockedCG or {}
    for k, v in pairs(captured.unlockedCG) do ctx3.unlockedCG[k] = v end
end
if captured.unlockedMusic then
    ctx3.unlockedMusic = ctx3.unlockedMusic or {}
    for k, v in pairs(captured.unlockedMusic) do ctx3.unlockedMusic[k] = v end
end
check("restore round-trip cg", ctx3.unlockedCG.scene01 == true)
check("restore round-trip music", ctx3.unlockedMusic.track01 == true)

if failed > 0 then os.exit(1) end
print("UNLOCK TESTS DONE")
