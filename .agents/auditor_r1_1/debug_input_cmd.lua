local test_dir = "tests/scripts/"
package.path = test_dir .. "../../scripts/?.lua;" .. test_dir .. "?.lua;" .. package.path
local _real_dofile = dofile
pcall(require, "replay")
pcall(require, "palette")
pcall(require, "viewport")

_real_dofile(test_dir .. "test_color_filter.lua")

-- Now debug test_input_cmd step by step
local Schema = require("kag.schema")
local TextCommands = require("kag.commands.text")
local TextScene = require("kag.text_scene")
local backend = require("backend")

local ctx = {
    f = {}, sf = {}, tf = {}, mp = {},
    text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
    backlog = {}, layers = {},
}

local co = coroutine.create(function()
    TextCommands.input(ctx, { name = "f.player_name", prompt = "Name:", default = "Hero", maxlen = 10, y = 800 })
end)
local ok, err = coroutine.resume(co)
print("co resume ok:", ok, "err:", err)
print("status:", coroutine.status(co))
print("_KAG_onTextInput:", type(_G._KAG_onTextInput))
