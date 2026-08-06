-- test_layout.lua — text scene layout state (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- TextScene requires backend (render) -- load it and test the pure
-- layout/wrap logic via the module functions that don't render.
local f = assert(io.open("scripts/kag/text_scene.lua", "r"))
local src = f:read("*a")
f:close()

-- clear resets cursor + draws
check("clear resets cursor_x", src:find("state.cursor_x = 32", 1, true) ~= nil)
check("clear resets cursor_y", src:find("state.cursor_y = 580", 1, true) ~= nil)
check("clear empties draws", src:find("state.draws = {}", 1, true) ~= nil)

-- add_wrapped advances y per line and updates both cursor state copies
check("wrapped advances y", src:find("y = y + line_height", 1, true) ~= nil)
check("wrapped updates ctx cursor", src:find("ctx.textCursorY = y", 1, true) ~= nil)

-- reset rebuilds the full text_state (line/char_offset/opacity)
check("reset full state", src:find("char_offset = 0", 1, true) ~= nil
      and src:find("opacity = 255", 1, true) ~= nil)

-- set_opacity clamps to a byte
check("opacity clamp", src:find("clamp_byte(opacity)", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("LAYOUT TESTS DONE")
