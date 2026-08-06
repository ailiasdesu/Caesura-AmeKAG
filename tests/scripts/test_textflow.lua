-- test_textflow.lua — [text]/[l]/[r]/[er] state machine (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")

-- [er] clears waiting_input (unblocking the runner)
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    waiting_input = true }
pcall(KAG.er, ctx, {})
check("er clears waiting_input", ctx.waiting_input == false)

-- [r] resets cursor_x to the left margin
local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    textCursorX = 500, textCursorY = 600 }
pcall(KAG.r, ctx2, {})
check("r resets cursor_x", ctx2.textCursorX == 32)

-- [l] advances cursor_y by the line height
local ctx3 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    textCursorX = 32, textCursorY = 600 }
pcall(KAG.l, ctx3, {})
check("l advances cursor_y", ctx3.textCursorY > 600)

-- [text] with no message is a no-op (no waiting_input side effect)
local ctx4 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    waiting_input = false }
pcall(KAG.text, ctx4, {})
check("empty text no-op", ctx4.waiting_input == false)

-- [p] blocks: waiting_input true + coroutine suspended until resumed
local scheduler = require("scheduler")
local tokens = { { "p" }, { "ch", { text = "after" } } }
local dispatched = {}
local real_ch = KAG.ch
KAG.ch = function(c2, p2) dispatched[#dispatched + 1] = p2.text end
local ctx5 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _whileIterByScene = { ["t.ks"] = 0 },
    macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
local co = coroutine.create(function() scheduler.run(ctx5, tokens, 1) end)
coroutine.resume(co)
check("p sets waiting_input", ctx5.waiting_input == true)
check("p blocks advance", #dispatched == 0)
check("p suspends the coroutine", coroutine.status(co) == "suspended")
ctx5.waiting_input = false
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
check("p resumes past", #dispatched == 1 and dispatched[1] == "after")
KAG.ch = real_ch  -- restore (security LOW: the stub must not persist)

-- [text text=...] param path: wraps + backlog (audit)
local Text2 = require("kag.commands.text")
local TextScene2 = require("kag.text_scene")
local wraps = {}
local real_add_wrapped = TextScene2.add_wrapped
TextScene2.add_wrapped = function(ctx, msg, opts)
    wraps[#wraps + 1] = { msg, opts }
    return 1
end
local ctxT = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    backlog = {}, text_state = {}, textCursorX = 32, textCursorY = 580 }
pcall(Text2.text, ctxT, { text = "hello world" })
check("text wraps message", wraps[1] and wraps[1][1] == "hello world")
check("text wraps at y 580", wraps[1] and wraps[1][2].y == 580)
TextScene2.add_wrapped = real_add_wrapped

if failed > 0 then os.exit(1) end
print("TEXTFLOW TESTS DONE")
