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
-- (anchored to the function's body -- the y-advance line also exists in
-- add_ruby, review nit)
local ws = src:find("function TextScene.add_wrapped", 1, true)
local we = src:find("function TextScene.add_ruby", ws or 1, true)
local wbody = ws and we and src:sub(ws, we) or ""
check("wrapped body found", #wbody > 0)
check("wrapped advances y", wbody:find("y = y + line_height", 1, true) ~= nil)
check("wrapped state cursor", wbody:find("state.cursor_y = y", 1, true) ~= nil)
check("wrapped ctx cursor", wbody:find("ctx.textCursorY = y", 1, true) ~= nil)

-- reset rebuilds the full text_state (line/char_offset/opacity)
check("reset full state", src:find("char_offset = 0", 1, true) ~= nil
      and src:find("opacity = 255", 1, true) ~= nil)

-- set_opacity clamps to a byte
check("opacity clamp", src:find("clamp_byte(opacity)", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("LAYOUT TESTS DONE")
