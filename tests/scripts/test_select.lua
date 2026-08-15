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
-- NOTE: do NOT clear package.loaded["kag.commands.text"] here -- the
-- suite sandbox gates module preloads, so a cleared cache would make
-- every later require("kag.commands.text") fail with "not preloaded"
-- (this poisoned test_textbox until removed). We only verify BLOCK
-- behavior + selection; the render path stays real.
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

-- ===========================================================================
-- round 74: menu-system finalization semantics
-- ===========================================================================
local scheduler = require("scheduler")

-- S1. option numbering starts at 1 (via the rendered active block)
do
    local ctxN = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
        _whileIterByScene = { ["t.ks"] = 0 }, macros = nil, macro_args = nil,
        current_scene = "t.ks", token_index = 1 }
    local tokensN = {
        { "select" },
        { "sel", { text = "Alpha", target = "*a" } },
        { "sel", { text = "Beta",  target = "*b" } },
        { "endselect" },
    }
    local coN = coroutine.create(function() scheduler.run(ctxN, tokensN, 1) end)
    local stepsN = 0
    while coroutine.status(coN) ~= "dead" and not ctxN.waiting_input and stepsN < 10 do
        coroutine.resume(coN); stepsN = stepsN + 1
    end
    check("option numbering 1-based", ctxN._choiceButtonsActive ~= nil
          and #ctxN._choiceButtonsActive == 2
          and ctxN._choiceButtonsActive[1].index == 1
          and ctxN._choiceButtonsActive[2].index == 2)
    -- finish the yoked endselect (pick the first option)
    ctxN.waiting_input = false
    ctxN._selectedChoice = ctxN._choiceButtonsActive[1]
    ctxN._choiceMode = false
    ctxN._choiceButtonsActive = nil
    while coroutine.status(coN) ~= "dead" do coroutine.resume(coN) end
end

-- S2. [sel] and [button] share one staging list; [endselect] draws both
do
    local ctxQ = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
        _whileIterByScene = { ["t.ks"] = 0 }, macros = nil, macro_args = nil,
        current_scene = "t.ks", token_index = 1 }
    local tokensQ = {
        { "select" },
        { "sel", { text = "Via-sel", target = "*a" } },
        { "button", { text = "Via-button", target = "*b" } },
        { "endselect" },
    }
    local coQ = coroutine.create(function() scheduler.run(ctxQ, tokensQ, 1) end)
    local stepsQ = 0
    while coroutine.status(coQ) ~= "dead" and not ctxQ.waiting_input and stepsQ < 10 do
        coroutine.resume(coQ); stepsQ = stepsQ + 1
    end
    check("sel+button mixed staged", ctxQ._choiceButtonsActive ~= nil
          and #ctxQ._choiceButtonsActive == 2
          and ctxQ._choiceButtonsActive[1].target == "*a"
          and ctxQ._choiceButtonsActive[2].target == "*b")
    ctxQ.waiting_input = false
    ctxQ._selectedChoice = ctxQ._choiceButtonsActive[1]
    ctxQ._choiceMode = false
    ctxQ._choiceButtonsActive = nil
    while coroutine.status(coQ) ~= "dead" do coroutine.resume(coQ) end
end

-- S3. empty [select] (no options before endselect) dissolves safely
do
    local ctxO = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
        _whileIterByScene = { ["t.ks"] = 0 }, macros = nil, macro_args = nil,
        current_scene = "t.ks", token_index = 1 }
    local tokensO = {
        { "select" },
        { "endselect" },
        { "ch", { name = "C", text = "after-empty" } },
    }
    local coO = coroutine.create(function() scheduler.run(ctxO, tokensO, 1) end)
    local stepsO = 0
    while coroutine.status(coO) ~= "dead" and stepsO < 10 do
        coroutine.resume(coO); stepsO = stepsO + 1
    end
    check("empty [select] no block", coroutine.status(coO) == "dead"
          and ctxO.waiting_input == nil and ctxO._choiceMode == nil)
end

-- S4. missing [endselect] tolerance: staged options linger, flow continues
do
    local ctxP = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
        _whileIterByScene = { ["t.ks"] = 0 }, macros = nil, macro_args = nil,
        current_scene = "t.ks", token_index = 1 }
    local tokensP = {
        { "select" },
        { "sel", { text = "Loose", target = "*l" } },
        { "ch", { name = "C", text = "still-running" } },
    }
    local coP = coroutine.create(function() scheduler.run(ctxP, tokensP, 1) end)
    local stepsP = 0
    while coroutine.status(coP) ~= "dead" and stepsP < 10 do
        coroutine.resume(coP); stepsP = stepsP + 1
    end
    check("missing [endselect] tolerated", coroutine.status(coP) == "dead"
          and ctxP.waiting_input == nil
          and type(ctxP._choiceButtons) == "table"
          and #ctxP._choiceButtons == 1)
end

-- S5. [sel x="tf.result"] writes the chosen target into tf when selected
do
    local ctxR = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
        _whileIterByScene = { ["t.ks"] = 0 }, macros = nil, macro_args = nil,
        current_scene = "t.ks", token_index = 1 }
    local tokensR = {
        { "select" },
        { "sel", { text = "Route A", target = "*a", x = "tf.result" } },
        { "sel", { text = "Route B", target = "*b", x = "tf.result" } },
        { "endselect" },
    }
    local coR = coroutine.create(function() scheduler.run(ctxR, tokensR, 1) end)
    local stepsR = 0
    while coroutine.status(coR) ~= "dead" and not ctxR.waiting_input and stepsR < 10 do
        coroutine.resume(coR); stepsR = stepsR + 1
    end
    check("sel x= active block staged", ctxR._choiceButtonsActive ~= nil
          and #ctxR._choiceButtonsActive == 2)
    ctxR.waiting_input = false
    ctxR._selectedChoice = ctxR._choiceButtonsActive[2]
    ctxR._choiceMode = false
    ctxR._choiceButtonsActive = nil
    while coroutine.status(coR) ~= "dead" do coroutine.resume(coR) end
    check("sel x= writes tf.result", ctxR.tf.result == "*b")
    check("sel x= plus jump", ctxR._pendingJump == "*b")
end

-- S6. [sel x=...] bare key writes into f scope
do
    local ctxS = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
        _whileIterByScene = { ["t.ks"] = 0 }, macros = nil, macro_args = nil,
        current_scene = "t.ks", token_index = 1 }
    local tokensS = {
        { "select" },
        { "sel", { text = "Only", target = "*o", x = "picked" } },
        { "endselect" },
    }
    local coS = coroutine.create(function() scheduler.run(ctxS, tokensS, 1) end)
    local stepsS = 0
    while coroutine.status(coS) ~= "dead" and not ctxS.waiting_input and stepsS < 10 do
        coroutine.resume(coS); stepsS = stepsS + 1
    end
    ctxS.waiting_input = false
    ctxS._selectedChoice = ctxS._choiceButtonsActive[1]
    ctxS._choiceMode = false
    ctxS._choiceButtonsActive = nil
    while coroutine.status(coS) ~= "dead" do coroutine.resume(coS) end
    check("sel x= bare key -> f", ctxS.f.picked == "*o")
end

KAG.ch = real_ch  -- restore the real handler (suite hygiene)

if failed > 0 then os.exit(1) end
print("SELECT TESTS DONE")
