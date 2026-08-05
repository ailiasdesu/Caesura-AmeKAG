-- Choice button hit-test tests: the engine dispatches clicks without
-- coordinates (coalesced), so the choice handler reads _GAME_MOUSE_X/Y.
local results = {}  -- file scope: runner shares globals
local check = function(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
        results[#results + 1] = cond
end

local TextCommands = require("kag.commands.text")
local TextScene = require("kag.text_scene")

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

-- endbutton installs the choice click handler (its trailing yield raises
-- outside a coroutine; the install itself succeeded -- assert the state)
pcall(function() TextCommands.endbutton(ctx, {}) end)
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

-- Click far outside any button must NOT select
local ctx2 = {
    f = {}, sf = {}, tf = {}, mp = {},
    text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
    backlog = {}, layers = {},
    _choiceButtons = { { text = "Only", target = "*only" } },
}
pcall(function() TextCommands.endbutton(ctx2, {}) end)
_G._GAME_MOUSE_X = 700   -- right of the 600 hit-box
_G._GAME_MOUSE_Y = 500
local h2 = _G._KAG_onClick
pcall(h2)
check("out-of-box click not selected", ctx2._selectedChoice == nil)

print("CHOICE TESTS DONE")
