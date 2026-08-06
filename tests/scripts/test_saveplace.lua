-- test_saveplace.lua — [saveplace]/[loadplace] bookmark chain (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local System = require("system")
local KAG = require("kag")

-- saveplace captures the REAL position (scene + token_index)
local ctx = { f = {}, tf = { score = 42 }, sf = {}, mp = {}, variables = {},
    current_scene = "scripts/demo_story.ks", token_index = 77,
    dialog_index = 3, text_state = { line = 2 } }
System.saveplace(ctx)
local pd = System._placeData
check("bookmark scene", pd.scene == "scripts/demo_story.ks")
check("bookmark index", pd.index == 77)
check("bookmark tf copied", pd.tf.score == 42)
check("bookmark text_state", pd.text_state.line == 2)

-- loadplace routes through the runner's jump path
local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    current_scene = "other.ks", token_index = 1, stop_flag = false }
local ok = System.loadplace(ctx2)
check("loadplace returns true", ok == true)
check("loadplace sets pendingJump", ctx2._pendingJump ~= nil
      and ctx2._pendingJump.scene == "scripts/demo_story.ks"
      and ctx2._pendingJump.index == 77)
check("loadplace sets stop_flag", ctx2.stop_flag == true)
check("loadplace restores tf", ctx2.tf.score == 42)

-- no bookmark -> false
System._placeData = nil
local ctx3 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    current_scene = "x.ks", token_index = 1 }
local ok3 = System.loadplace(ctx3)
check("no bookmark returns false", ok3 == false)

-- KAG.saveplace/loadplace route through System
pcall(KAG.saveplace, ctx, {})
pcall(KAG.loadplace, ctx2, {})
check("KAG saveplace routes", System._placeData ~= nil
      and System._placeData.index == 77)
check("KAG loadplace routes", ctx2._pendingJump ~= nil)

if failed > 0 then os.exit(1) end
print("SAVEPLACE TESTS DONE")
