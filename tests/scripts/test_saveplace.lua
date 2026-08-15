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

-- ----------------------------------------------------------------
-- Round 74 (stage D): saveplace/loadplace boundary deepening
-- ----------------------------------------------------------------

-- [saveplace] inside a [call] frame: the bookmark captures the INNER
-- frame's scene + token position, but deliberately does NOT capture the
-- call stack. So a loadplace resume from a call-frame bookmark loses the
-- caller chain (no valid [return] target) -- a documented boundary: the
-- bookmark is an independent in-memory scene point, not a frame-consistent
-- continuation.
do
    local had = System._placeData
    local ctxCall = { f = { inner = 1 }, tf = {}, sf = {}, mp = {}, lf = {},
        variables = {}, current_scene = "scripts/demo_sub.ks", token_index = 9,
        call_stack = { { tokens = { "main" }, index = 3 } } }
    System.saveplace(ctxCall)
    local pd = System._placeData
    check("saveplace-in-call captures inner scene",
        pd and pd.scene == "scripts/demo_sub.ks", tostring(pd and pd.scene))
    check("saveplace-in-call captures inner token", pd and pd.index == 9)
    check("saveplace does NOT capture call_stack",
        pd and pd.call_stack == nil)
    check("saveplace does NOT capture labelMap",
        pd and pd.labelMap == nil)
    if had == nil then System._placeData = nil end
end

-- [loadplace] token-position precision: the resume target preserves the
-- EXACT saved token index (no off-by-one), routed through _pendingJump.
do
    System._placeData = nil
    System.saveplace({ f = {}, tf = {}, sf = {}, mp = {}, lf = {},
        variables = {}, current_scene = "scripts/demo_precise.ks",
        token_index = 11 })
    local lp = { f = {}, tf = {}, sf = {}, mp = {}, lf = {}, variables = {},
        current_scene = "other.ks", token_index = 1, stop_flag = false }
    local ok = System.loadplace(lp)
    check("loadplace exact token precision", ok
        and lp._pendingJump and lp._pendingJump.index == 11,
        'index=' .. tostring(lp._pendingJump and lp._pendingJump.index))
    check("loadplace precise scene", lp._pendingJump
        and lp._pendingJump.scene == "scripts/demo_precise.ks")
    check("loadplace precise sets stop_flag", lp.stop_flag == true)
end

-- Bookmark tf is a DEEP copy: mutating the source ctx.tf after saveplace
-- must not retroactively alter the stored bookmark.
do
    System._placeData = nil
    local srcTf = { state_x = 1 }
    local bs = { f = {}, tf = srcTf, sf = {}, mp = {}, lf = {}, variables = {},
        current_scene = "scripts/demo_copy.ks", token_index = 4 }
    System.saveplace(bs)
    -- mutate the LIVE tf after the bookmark was taken
    srcTf.state_x = 999
    srcTf.extra = true
    check("saveplace tf is a deep copy (no aliasing)",
        System._placeData and System._placeData.tf
        and System._placeData.tf.state_x == 1
        and System._placeData.tf.extra == nil,
        'stored=' .. tostring(System._placeData and System._placeData.tf and System._placeData.tf.state_x))
    -- and a loadplace restores the SNAPSHOT, not the mutated live table
    local lp2 = { f = {}, tf = {}, sf = {}, mp = {}, lf = {}, variables = {},
        current_scene = "other.ks", token_index = 1 }
    System.loadplace(lp2)
    check("loadplace restores snapshot tf (unmutated)",
        lp2.tf and lp2.tf.state_x == 1 and lp2.tf.extra == nil)
end

if failed > 0 then os.exit(1) end
print("SAVEPLACE TESTS DONE")
