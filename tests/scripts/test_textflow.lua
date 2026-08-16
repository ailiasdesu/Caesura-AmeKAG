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

-- [textspeed cps=N] / [cps N] -- KAG3-type typewriter speed (round)
-- Real read point: kag_runner.lua advances ctx.reveal by ctx.text_speed
-- (ms/char) each frame, so the handler must set ctx.text_speed.
local S = require("kag.schema")

-- named [textspeed cps=60] -> floor(1000/60) == 16 ms/char
local ctxT2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.textspeed, ctxT2, S.coerce("textspeed", { cps = 60 }, ctxT2))
check("textspeed cps=60 sets ctx.cps", ctxT2.cps == 60)
check("textspeed cps=60 -> 16 ms/char read point", ctxT2.text_speed == math.floor(1000 / 60))

-- [textspeed] no param -> schema default (50 cps == 20 ms/char)
local ctxD = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.textspeed, ctxD, S.coerce("textspeed", {}, ctxD))
check("textspeed default restores 50 cps", ctxD.cps == 50)
check("textspeed default -> 20 ms/char", ctxD.text_speed == 20)

-- bare positional [cps 50] (alias) == [textspeed cps=50]
local ctxP = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.cps, ctxP, S.coerce("cps", { [1] = "50" }, ctxP))
check("cps 50 positional = 50 cps", ctxP.cps == 50)
check("cps 50 positional -> 20 ms/char", ctxP.text_speed == 20)

-- named [cps cps=25] alias sets the same cps
local ctxA = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.cps, ctxA, S.coerce("cps", { cps = 25 }, ctxA))
check("cps alias cps=25 sets ctx.cps", ctxA.cps == 25)

-- named out-of-range clamps via the contract (cps=0 -> min 1)
local ctxZ = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.textspeed, ctxZ, S.coerce("textspeed", { cps = 0 }, ctxZ))
check("textspeed cps=0 clamps to 1", ctxZ.cps == 1)
check("textspeed cps=0 -> 1000 ms/char", ctxZ.text_speed == 1000)

-- bare positional out-of-range clamps in the handler ([cps -5] -> 1)
local ctxN = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.cps, ctxN, S.coerce("cps", { [1] = "-5" }, ctxN))
check("cps -5 positional clamps to 1", ctxN.cps == 1)

-- named non-numeric errors inside schema.coerce (visible, not silent)
local okBad = pcall(S.coerce, "textspeed", { cps = "abc" }, {})
check("textspeed cps=abc errors (visible)", okBad == false)

-- bare positional non-numeric now rejects inside schema.coerce (round 97:
-- positional values are type-coerced just like named -- consistent with the
-- named [textspeed cps=abc] case above, no silent fallback)
local ctxQ = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
local okQ = pcall(S.coerce, "cps", { [1] = "abc" }, ctxQ)
check("cps abc positional errors (round 97: coerced, consistent with named)", okQ == false)

if failed > 0 then os.exit(1) end
print("TEXTFLOW TESTS DONE")
