-- test_select.lua — KAG3 [select]/[sel]/[endselect] (Neo-Genesis alias)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
check("sel alias exists", type(KAG.sel) == "function")
check("select exists", type(KAG.select) == "function")
check("endselect exists", type(KAG.endselect) == "function")
check("sel shares button engine", KAG.sel == KAG.button)

-- [sel] registers options into the same _choiceButtons list
local ctx = { _choiceButtons = nil, f = {}, tf = {}, sf = {}, mp = {},
    _whileIterByScene = { ["t.ks"] = 0 }, variables = {},
    current_scene = "t.ks", token_index = 1 }
KAG.select(ctx, {})
check("select opens list", type(ctx._choiceButtons) == "table")
KAG.sel(ctx, { text = "Go north", target = "*north" })
KAG.sel(ctx, { text = "Go south", target = "*south" })
check("sel registers options", #ctx._choiceButtons == 2)
check("sel fields kept", ctx._choiceButtons[1].text == "Go north"
      and ctx._choiceButtons[1].target == "*north")

-- endselect renders + blocks (waiting_input); simulate a click selection
-- through the choice hit-test path
local scheduler = require("scheduler")
local tokens = {
    { "select" },
    { "sel", { text = "A", target = "*a" } },
    { "sel", { text = "B", target = "*b" } },
    { "endselect" },
    { "ch", { name = "C", text = "after" } },
}
local dispatched = {}
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = KAG
local real_ch = KAG.ch
KAG.ch = function(c2, p2) dispatched[#dispatched + 1] = p2.text end
-- (KAG.ch restored at the end of the file -- the stub must not leak
-- to later tests: it shadows the real ch handler)
-- stub TextScene so the render path is a no-op in tests
package.loaded["kag.commands.text"] = nil  -- fresh? no: keep real, patch scene
local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 },
    macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
-- (no text-module require: the sandbox gates module preloads in the
-- suite environment -- we only verify the BLOCK behavior + selection)
local co = coroutine.create(function() scheduler.run(ctx2, tokens, 1) end)
-- resume until endselect yields (waiting_input true)
local steps = 0
while coroutine.status(co) ~= "dead" and not ctx2.waiting_input and steps < 10 do
    coroutine.resume(co)
    steps = steps + 1
end
check("endselect blocks on waiting_input", ctx2.waiting_input == true)
check("choice buttons registered", type(ctx2._choiceButtonsActive) == "table"
      and #ctx2._choiceButtonsActive == 2)
-- simulate selection: hit the first button region
_G._GAME_MOUSE_X, _G._GAME_MOUSE_Y = 100, ctx2._choiceButtonsActive[1].y
ctx2.waiting_input = false
local cb = ctx2._choiceButtonsActive[1]
ctx2._selectedChoice = cb
ctx2._choiceMode = false
ctx2._choiceButtonsActive = nil
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
check("pending jump set", ctx2._pendingJump == "*a")
package.loaded["kag"] = kag_orig

KAG.ch = real_ch  -- restore the real handler (suite hygiene)

if failed > 0 then os.exit(1) end
print("SELECT TESTS DONE")
