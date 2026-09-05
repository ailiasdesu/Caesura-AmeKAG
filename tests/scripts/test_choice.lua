-- Choice button hit-test tests: the engine dispatches clicks without
-- coordinates (coalesced), so the choice handler reads _GAME_MOUSE_X/Y.
local results = {}  -- file scope: runner shares globals
local check = function(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
        results[#results + 1] = cond
end

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local TextCommands = require("kag.commands.text")
local TextScene = require("kag.text_scene")

local function start_choice(ctx, command)
    local co = coroutine.create(function()
        (command or TextCommands.endbutton)(ctx, {})
    end)
    local ok, err = coroutine.resume(co)
    assert(ok, tostring(err))
    return co
end

-- Minimal ctx
local ctx = {
    f = {}, sf = {}, tf = {}, mp = {},
    text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
    backlog = {}, layers = {},
    _choiceButtons = {
        { text = "Go left", target = "*left" },
        { text = "Go right", target = "*right" },
    },
}

-- Keep the actual command suspended while exercising its installed handler.
local choiceCo = start_choice(ctx)
check("endbutton installs choice mode", ctx._choiceMode == true)
check("choice buttons active", ctx._choiceButtonsActive ~= nil and #ctx._choiceButtonsActive == 2)
check("button regions assigned", ctx._choiceButtonsActive[1].y ~= nil)

-- Simulate a click at the second button: hit-test via the installed handler
local handler = _G._KAG_onClick
check("click handler installed", type(handler) == "function")
_G._GAME_MOUSE_X = 100
_G._GAME_MOUSE_Y = ctx._choiceButtonsActive[2].y + 5  -- inside button 2 region
pcall(handler)
check("choice 2 selected", ctx._selectedChoice ~= nil and ctx._selectedChoice.target == "*right")
check("choice mode cleared", ctx._choiceMode == false)
check("waiting_input cleared", ctx.waiting_input == false)
check("handler restored", _G._KAG_onClick ~= handler or _G._KAG_onClick == nil)
assert(coroutine.resume(choiceCo))
check("selected choice commits and releases its UI", ctx._pendingJump == "*right"
      and #TextScene.get_state(ctx).draws == 0)
assert(coroutine.close(choiceCo))

-- Click far outside any button must NOT select
local ctx2 = {
    f = {}, sf = {}, tf = {}, mp = {},
    text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
    backlog = {}, layers = {},
    _choiceButtons = { { text = "Only", target = "*only" } },
}
local choiceCo2 = start_choice(ctx2)
_G._GAME_MOUSE_X = 700   -- right of the 600 hit-box
_G._GAME_MOUSE_Y = 500
local h2 = _G._KAG_onClick
pcall(h2)
check("out-of-box click not selected", ctx2._selectedChoice == nil)
assert(coroutine.close(choiceCo2))

-- bare target (audit): [button *route_a text="A"] -- the bare arg is
-- the JUMP TARGET (*route_a); text= is the displayed label
local KAG2 = require("kag")
local ctxB = { f = {}, tf = {}, sf = {}, mp = {}, variables = {}, _choiceButtons = {} }
pcall(KAG2.button, ctxB, { "*route_a", text = "A" })
check("bare target collected", ctxB._choiceButtons[1]
      and ctxB._choiceButtons[1].target == "*route_a")

-- ---------------------------------------------------------------------------
-- [button cond=...] — conditional choices (Ren'Py menu `if` parity):
-- false choices are dropped at [endbutton]; all-hidden blocks dissolve.
-- ---------------------------------------------------------------------------
local ctxC = {
    f = { has_key = true }, sf = {}, tf = {}, mp = {},
    text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
    backlog = {}, layers = {},
    _choiceButtons = {
        { text = "Open door", target = "*door", cond = "f.has_key == true" },
        { text = "Force door", target = "*force", cond = "f.has_key == false" },
        { text = "Leave", target = "*leave" },
    },
}
local choiceCoC = start_choice(ctxC)
check("cond filters false choices", ctxC._choiceButtonsActive ~= nil
      and #ctxC._choiceButtonsActive == 2)
check("cond keeps true choice", ctxC._choiceButtonsActive[1]
      and ctxC._choiceButtonsActive[1].target == "*door")
check("cond keeps unconditional choice", ctxC._choiceButtonsActive[2]
      and ctxC._choiceButtonsActive[2].target == "*leave")
local hC = _G._KAG_onClick
_G._GAME_MOUSE_X = 100
_G._GAME_MOUSE_Y = ctxC._choiceButtonsActive[1].y + 5
pcall(hC)
check("cond choice clickable", ctxC._selectedChoice ~= nil
      and ctxC._selectedChoice.target == "*door")
assert(coroutine.resume(choiceCoC))
assert(coroutine.close(choiceCoC))

-- all hidden: the block dissolves without entering choice mode
local ctxD = {
    f = { has_key = false }, sf = {}, tf = {}, mp = {},
    text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
    backlog = {}, layers = {},
    _choiceButtons = {
        { text = "Open door", target = "*door", cond = "f.has_key == true" },
    },
}
local choiceCoD = start_choice(ctxD)
check("all-hidden block dissolves", ctxD._choiceMode == nil
      and ctxD._choiceButtons == nil)
assert(coroutine.close(choiceCoD))

-- TJS short-circuit form: f.has_key && f.other (both must hold)
local ctxE = {
    f = { has_key = true, other = false }, sf = {}, tf = {}, mp = {},
    text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
    backlog = {}, layers = {},
    _choiceButtons = {
        { text = "Combo", target = "*c", cond = "f.has_key && f.other" },
        { text = "Fallback", target = "*f" },
    },
}
local choiceCoE = start_choice(ctxE)
check("cond TJS && short-circuit", ctxE._choiceButtonsActive ~= nil
      and #ctxE._choiceButtonsActive == 1
      and ctxE._choiceButtonsActive[1].target == "*f")
assert(coroutine.close(choiceCoE))

-- ---- round 59: [button cond] edge cases --------------------------------
-- C1. numeric inequality + unconditional survive (raw-TJS path)
do
    local ctxF = {
        f = { hp = 5 }, sf = {}, tf = {}, mp = {},
        text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
        textCursorX = 32, textCursorY = 580, backlog = {}, layers = {},
        _choiceButtons = {
            { text = "A", target = "*a", cond = "f.hp > 10" },
            { text = "B", target = "*b", cond = "f.hp != 0" },
            { text = "C", target = "*c" },
        },
    }
    local co = start_choice(ctxF)
    check("cond numeric inequality filters", ctxF._choiceButtonsActive ~= nil
          and #ctxF._choiceButtonsActive == 2
          and ctxF._choiceButtonsActive[1].target == "*b"
          and ctxF._choiceButtonsActive[2].target == "*c")
    assert(coroutine.close(co))
end

-- C2. missing var cond is falsy -> hidden -> all-hidden dissolves
do
    local ctxG = {
        f = {}, sf = {}, tf = {}, mp = {},
        text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
        textCursorX = 32, textCursorY = 580, backlog = {}, layers = {},
        _choiceButtons = {
            { text = "A", target = "*a", cond = "f.nope" },
        },
    }
    local co = start_choice(ctxG)
    check("cond missing var hidden, block dissolves", ctxG._choiceMode == nil
          and ctxG._choiceButtons == nil)
    assert(coroutine.close(co))
end

-- C3. empty-string cond treated as unconditional
do
    local ctxH = {
        f = {}, sf = {}, tf = {}, mp = {},
        text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
        textCursorX = 32, textCursorY = 580, backlog = {}, layers = {},
        _choiceButtons = {
            { text = "A", target = "*a", cond = "" },
            { text = "B", target = "*b" },
        },
    }
    local co = start_choice(ctxH)
    check("cond empty string kept as unconditional",
          ctxH._choiceButtonsActive ~= nil and #ctxH._choiceButtonsActive == 2)
    assert(coroutine.close(co))
end

-- C4. combined TJS cond (== && !=) through the raw-TJS path
do
    local ctxI = {
        f = { a = 1, b = 2 }, sf = {}, tf = {}, mp = {},
        text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
        textCursorX = 32, textCursorY = 580, backlog = {}, layers = {},
        _choiceButtons = {
            { text = "A", target = "*a", cond = "f.a == 1 && f.b != 3" },
            { text = "B", target = "*b", cond = "f.a == 2 && f.b != 3" },
            { text = "C", target = "*c" },
        },
    }
    local co = start_choice(ctxI)
    check("cond combined TJS keeps only true", ctxI._choiceButtonsActive ~= nil
          and #ctxI._choiceButtonsActive == 2
          and ctxI._choiceButtonsActive[1].target == "*a")
    assert(coroutine.close(co))
end

-- C5. endselect parity with endbutton
do
    local ctxJ = {
        f = { ok = false }, sf = {}, tf = {}, mp = {},
        text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
        textCursorX = 32, textCursorY = 580, backlog = {}, layers = {},
        _choiceButtons = {
            { text = "A", target = "*a", cond = "f.ok" },
            { text = "B", target = "*b" },
        },
    }
    local co = start_choice(ctxJ, TextCommands.endselect)
    check("endselect filters like endbutton", ctxJ._choiceButtonsActive ~= nil
          and #ctxJ._choiceButtonsActive == 1
          and ctxJ._choiceButtonsActive[1].target == "*b")
    assert(coroutine.close(co))
end

-- C6. endbutton with no staged choices is a no-op
do
    local ctxK = { f = {}, sf = {}, tf = {}, mp = {}, _choiceButtons = {} }
    local co = start_choice(ctxK)
    check("endbutton no choices no-op", ctxK._choiceMode == nil
          and ctxK._choiceButtons == nil)
    assert(coroutine.close(co))
end

-- ---------------------------------------------------------------------------
-- round 74: [button] without [endbutton] — staged options linger but never
-- block or enter choice mode (headless tolerance); a later [endbutton]
-- can still render them.
-- ---------------------------------------------------------------------------
do
    local ctxL = { f = {}, sf = {}, tf = {}, mp = {}, _choiceButtons = nil,
        current_scene = "t.ks", variables = {} }
    pcall(KAG2.button, ctxL, { text = "Orphaned", target = "*o" })
    check("bare [button] stages, no endbutton", type(ctxL._choiceButtons) == "table"
          and #ctxL._choiceButtons == 1)
    check("[button] no-endbutton no block", ctxL._choiceMode == nil
          and ctxL.waiting_input == nil
          and ctxL._choiceButtonsActive == nil)
    local co = start_choice(ctxL)
    check("staged [button] renders on later [endbutton]",
          ctxL._choiceButtonsActive ~= nil and #ctxL._choiceButtonsActive == 1)
    assert(coroutine.close(co))
end

-- round 74: cond-hidden choices renumber 1-based in the visible block
do
    local ctxM = {
        f = { has = true }, sf = {}, tf = {}, mp = {},
        text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
        textCursorX = 32, textCursorY = 580, backlog = {}, layers = {},
        _choiceButtons = {
            { text = "A", target = "*a", cond = "f.has" },   -- shown
            { text = "B", target = "*b", cond = "f.nope" },  -- hidden
            { text = "C", target = "*c" },                   -- shown
        },
    }
    local co = start_choice(ctxM)
    check("cond-hidden renumber starts at 1", ctxM._choiceButtonsActive ~= nil
          and #ctxM._choiceButtonsActive == 2
          and ctxM._choiceButtonsActive[1].index == 1
          and ctxM._choiceButtonsActive[2].index == 2
          and ctxM._choiceButtonsActive[2].target == "*c")
    assert(coroutine.close(co))
end

print("CHOICE TESTS DONE")
