-- test_scroll.lua — [scroll] multi-line ED credits (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local tokenizer = require("tokenizer")
local toks = tokenizer.parse('[scroll text="L1\nL2\nL3" speed=60]')
check("scroll parses", toks[1].cmd == "scroll")

-- behavior: 3 lines render with the REAL contract (text, x, y, r, g, b, a)
local renders = {}
local backend_backup = _G._CAESURA_BACKEND
_G._CAESURA_BACKEND = { render = function(cmd, ...)
    if cmd == "render_text" then renders[#renders + 1] = { ... } end
    if cmd == "text_set_font" then return true end
    if cmd == "clear_text" then return true end
    return true end }
local T = package.loaded["kag.commands.transition"] or require("kag.commands.transition")
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    viewport = { width = 1280, height = 720 } }
local co = coroutine.create(function()
    T.scroll(ctx, { text = "L1\nL2\nL3", speed = 5000, size = 28, color = "red" })
end)
local r1 = coroutine.resume(co)
local r2 = coroutine.resume(co, 16)
check("scroll coroutine", r1 and r2)
check("three lines rendered", #renders == 3)
if #renders == 3 then
    -- x is the fixed 32; y increases by lineHeight (28+10); color 255,0,0
    check("x fixed 32", renders[1][2] == 32 and renders[2][2] == 32)
    check("y stride", renders[2][3] > renders[1][3])
    check("color parsed", renders[1][4] == 255 and renders[1][5] == 0
          and renders[1][6] == 0 and renders[1][7] == 255)
end
_G._CAESURA_BACKEND = backend_backup

-- source locks: color resolution + font set + 7-arg render
local f = assert(io.open("scripts/kag/commands/transition.lua", "r"))
local src = f:read("*a")
f:close()
check("color parsed", src:find('color:match("(%d+),%s*(%d+),%s*(%d+)")', 1, true) ~= nil)
check("font set", src:find("pcall(backend.text_set_font", 1, true) ~= nil)
check("render 7-arg", src:find("render_text(line, 32, y + (i - 1) * lineHeight,", 1, true) ~= nil)
check("multi-line split", src:find("gmatch(\"(.-)", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("SCROLL TESTS DONE")
