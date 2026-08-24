-- test_resolve.lua — text resolve helpers (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- the resolve helpers are locals in text.lua -- lock their contracts
-- via source (backend/TextScene aren't available in the suite sandbox)
local f = assert(io.open("scripts/kag/commands/text.lua", "r"))
local src = f:read("*a")
f:close()

-- line height fallback chain: backend -> font_size -> 24, with the
-- multi-value paren wrap and the <=0 guard
check("lh multi-value wrap", src:find("tonumber((backend.line_height()))", 1, true) ~= nil)
check("lh font fallback", src:find("tonumber(state.font_size)", 1, true) ~= nil)
check("lh default 24", src:find("or 24", 1, true) ~= nil)
check("lh zero guard", src:find("if line_height <= 0 then return 24 end", 1, true) ~= nil)

-- max width: explicit > chars_per_line > viewport remainder
check("mw explicit", src:find("params.max_width", 1, true) ~= nil)
check("mw chars_per_line", src:find("chars_per_line * (tonumber(state.font_size) or 24)", 1, true) ~= nil)
check("mw fallback", src:find("math.max(1, vw - x - 48)", 1, true) ~= nil)

-- color: hex parse -> font_color -> white, bytes clamped
check("color hex parse", src:find("parse_hex_color(params.color)", 1, true) ~= nil)
check("color font fallback", src:find("parse_hex_color(state.font_color)", 1, true) ~= nil)
check("color white default", src:find("r = 255, g = 255, b = 255, a = 255", 1, true) ~= nil)
check("color byte clamps", src:find("clamp_byte(params.r or color.r)", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("RESOLVE TESTS DONE")
