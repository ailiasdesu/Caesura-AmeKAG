-- test_reveal.lua — multi-line typewriter reveal offsets (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local TextScene = require("kag.text_scene")
local backend_backup = _G._CAESURA_BACKEND
local rendered = {}
_G._CAESURA_BACKEND = { render = function(cmd, ...)
    if cmd == "render_text" then rendered[#rendered + 1] = { ... } end
    if cmd == "render_ruby" then rendered[#rendered + 1] = { ... } end
    return true end }

local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
local st = TextScene.get_state(ctx)
-- two wrapped lines of 5 chars each
st.draws = {
    { kind = "text", typewriter = true, text = "AAAAA", x = 10, y = 10, r = 255, g = 255, b = 255, a = 255 },
    { kind = "text", typewriter = true, text = "BBBBB", x = 10, y = 30, r = 255, g = 255, b = 255, a = 255 },
}
-- reveal=7: line1 full (5), line2 shows 2
st.reveal_chars = 7
rendered = {}
TextScene.render(ctx)
check("two draws", #rendered == 2)
check("line1 full", rendered[1][1] == "AAAAA")
check("line2 offset-truncated", rendered[2][1] == "BB")

-- reveal=12 (> line1+line2): both full
st.reveal_chars = 12
rendered = {}
TextScene.render(ctx)
check("both full at 12", rendered[1][1] == "AAAAA" and rendered[2][1] == "BBBBB")

-- reveal=3: line1 shows 3, line2 empty
st.reveal_chars = 3
rendered = {}
TextScene.render(ctx)
check("line1 partial", rendered[1][1] == "AAA")
check("line2 empty", rendered[2][1] == "")

-- reveal=nil (no animation): full lines, no truncation
st.reveal_chars = nil
rendered = {}
TextScene.render(ctx)
check("no reveal full", rendered[1][1] == "AAAAA" and rendered[2][1] == "BBBBB")

_G._CAESURA_BACKEND = backend_backup

-- alpha=0 frames still advance consumed (review blocking: the reveal
-- offset must not over-show line 2 while line 1 fades in)
_G._CAESURA_BACKEND = { render = function(cmd, ...)
    if cmd == "render_text" then rendered[#rendered + 1] = { ... } end
    return true end }
local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
local st2 = TextScene.get_state(ctx2)
st2.draws = {
    { kind = "text", typewriter = true, text = "AAAAA", x = 10, y = 10, r = 255, g = 255, b = 255, a = 0 },
    { kind = "text", typewriter = true, text = "BBBBB", x = 10, y = 30, r = 255, g = 255, b = 255, a = 255 },
}
st2.reveal_chars = 7
rendered = {}
TextScene.render(ctx2)
_G._CAESURA_BACKEND = backend_backup
check("alpha-skip advances consumed", rendered[1] and rendered[1][1] == "BB")

if failed > 0 then os.exit(1) end
print("REVEAL TESTS DONE")
