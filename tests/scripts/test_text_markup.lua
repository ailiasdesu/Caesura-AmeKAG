-- test_text_markup.lua — Neo-Genesis inline text markup:
--   {color=#rrggbb}...{/color} per-span colors in [ch]/[text] messages.
--   {b} synthetic bold, {i} italic shear, {size} glyph scaling — rendered.
--   Unknown {tags} pass through literally.
local results = {}
local function check(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    results[#results + 1] = cond
end

-- Mock the engine-side APIs (no GPU in unit tests)
_G.backend = _G.backend or {}
_G.backend = {
    create_solid_texture = function() return { _mock = true } end,
    render_text = function() end,
    set_input_focus = function() end,
    audio_play = function() end,
    create_viewport = function() return { _mock = true } end,
    destroy_viewport = function() end,
    clear_text = function() end,
    load_texture = function() return { _mock = true } end,
    submit_batch = function() end,
    text_set_state = function() end,
}

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local TextLayout = require("kag.text_layout")
local TextScene = require("kag.text_scene")
local TextCommands = require("kag.commands.text")
local layers = require("layers")

-- ---------------------------------------------------------------------------
-- 1. parse_markup
-- ---------------------------------------------------------------------------
local m1 = TextLayout.parse_markup("A{color=#ff0000}B{/color}C")
check("parse: three spans", #m1.spans == 3
      and m1.spans[1].text == "A" and m1.spans[1].color == nil
      and m1.spans[2].text == "B" and m1.spans[2].color ~= nil
          and m1.spans[2].color.r == 255 and m1.spans[2].color.g == 0
          and m1.spans[2].color.b == 0
      and m1.spans[3].text == "C" and m1.spans[3].color == nil)
check("parse: plain strips markup", m1.plain == "ABC")

local m2 = TextLayout.parse_markup("plain text")
check("parse: no markup passthrough", #m2.spans == 1
      and m2.spans[1].text == "plain text" and m2.plain == "plain text")

local m3 = TextLayout.parse_markup("A{foo}B")
check("parse: unknown tag literal", m3.plain == "A{foo}B")

local m4 = TextLayout.parse_markup("A{color=#00ff00}B")
check("parse: unclosed color to end", m4.plain == "AB"
      and m4.spans[2].color ~= nil and m4.spans[2].color.g == 255)

local m5 = TextLayout.parse_markup(
    "{color=#ff0000}A{color=#0000ff}B{/color}C{/color}D")
check("parse: nested colors", m5.plain == "ABCD"
      and m5.spans[1].color.r == 255
      and m5.spans[2].color.b == 255
      and m5.spans[3].color.r == 255
      and m5.spans[4].color == nil)

local m6 = TextLayout.parse_markup("{b}bold{/b}{i}it{/i}{size=30}sz{/size}")
check("parse: b/i/size consumed", m6.plain == "bolditsz")
check("parse: bold span flagged", m6.spans[1].bold == true
      and m6.spans[2].bold == false)
check("parse: size span carries size", m6.spans[3].size == 30
      and m6.spans[2].size == nil)
check("parse: italic span flagged", m6.spans[2].italic == true
      and m6.spans[1].italic == false and m6.spans[3].italic == false)

local m10 = TextLayout.parse_markup("{b}{i}both{/i}{/b}x")
check("parse: bold+italic stackable", m10.spans[1].bold == true
      and m10.spans[1].italic == true and m10.spans[2].italic == false)

local m11 = TextLayout.parse_markup("a{i}b")
check("parse: unclosed italic to end", m11.plain == "ab"
      and m11.spans[2].italic == true)

local m12 = TextLayout.parse_markup("{s}struck{/s}x")
check("parse: strike span flagged", m12.plain == "struckx"
      and m12.spans[1].strike == true and m12.spans[2].strike == false)

local m13 = TextLayout.parse_markup("{b}{s}both{/s}{/b}y")
check("parse: bold+strike stackable", m13.spans[1].bold == true
      and m13.spans[1].strike == true and m13.spans[2].strike == false)

local m14 = TextLayout.parse_markup("z{s}w")
check("parse: unclosed strike to end", m14.plain == "zw"
      and m14.spans[2].strike == true)

local m7 = TextLayout.parse_markup("X{/color}Y")
check("parse: stray closer consumed", m7.plain == "XY")

local m8 = TextLayout.parse_markup("{color=ff0000}X")
check("parse: hex without #", m8.plain == "X"
      and m8.spans[1].color.r == 255)

local m9 = TextLayout.parse_markup("你好{color=#ff0000}世界{/color}")
check("parse: utf8 content", m9.plain == "你好世界"
      and m9.spans[2].text == "世界" and m9.spans[2].color ~= nil)

-- ---------------------------------------------------------------------------
-- 2. wrap_spans — segment grouping + color across line breaks
-- ---------------------------------------------------------------------------
local RED = { r = 255, g = 0, b = 0 }
local w1 = TextLayout.wrap_spans(
    { { text = "ab", color = nil }, { text = "cd", color = RED } },
    { max_width = 1000, font_size = 24 })
check("wrap: one line two segments", #w1 == 1 and #w1[1].segments == 2
      and w1[1].segments[1].text == "ab" and w1[1].segments[1].color == nil
      and w1[1].segments[2].text == "cd" and w1[1].segments[2].color == RED)

-- 20 half-width chars at font 24 -> 240px; max_width 120 -> 2 lines
local longText = string.rep("x", 20)
local w2 = TextLayout.wrap_spans(
    { { text = longText, color = RED } },
    { max_width = 120, font_size = 24 })
check("wrap: color split across lines", #w2 == 2
      and #w2[1].segments == 1 and w2[1].segments[1].color == RED
      and #w2[2].segments == 1 and w2[2].segments[1].color == RED
      and w2[1].segments[1].text .. w2[2].segments[1].text == longText)

-- {size=36} over font 24 -> scale 1.5; a 10-char run wraps earlier
local w3 = TextLayout.wrap_spans(
    { { text = "abcdefghij", size = 36, bold = true } },
    { max_width = 120, font_size = 24 })
check("wrap: scaled width wraps earlier", #w3 >= 2)
check("wrap: segments carry scale and bold",
      w3[1].segments[1].scale == 1.5 and w3[1].segments[1].bold == true)

-- style boundary splits segments (color vs bold vs size)
local w4 = TextLayout.wrap_spans(
    {
        { text = "ab", bold = true },
        { text = "cd", color = RED },
        { text = "ef", size = 36 },
    },
    { max_width = 1000, font_size = 24 })
check("wrap: style boundaries split segments", #w4 == 1
      and #w4[1].segments == 3
      and w4[1].segments[1].bold == true
      and w4[1].segments[2].color == RED
      and w4[1].segments[3].scale == 1.5)

-- instant spans (NVL speaker prefix) keep their own segment even when
-- colors are nil, so the message stays a separate typewriter draw
local w5 = TextLayout.wrap_spans(
    { { text = "「A」：", instant = true }, { text = "msg" } },
    { max_width = 1000, font_size = 24 })
check("wrap: instant prefix keeps its own segment", #w5 == 1
      and #w5[1].segments == 2
      and w5[1].segments[1].instant == true
      and w5[1].segments[1].text == "「A」："
      and w5[1].segments[2].instant == false)

-- ---------------------------------------------------------------------------
-- 3. add_wrapped_spans — per-segment draws with advancing x
-- ---------------------------------------------------------------------------
local ctxT = {
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
}
TextScene.add_wrapped_spans(ctxT,
    { { text = "aa", color = nil }, { text = "bb", color = RED } },
    { x = 32, y = 580, max_width = 1000, line_height = 24,
      font_size = 24, color = { r = 255, g = 255, b = 255 } })
local draws = ctxT.text_state.draws
check("spans: two draws", #draws == 2)
check("spans: default color on plain", draws[1].r == 255 and draws[1].g == 255
      and draws[1].b == 255)
check("spans: red span color", draws[2].r == 255 and draws[2].g == 0
      and draws[2].b == 0)
check("spans: x advances by width", draws[2].x == draws[1].x + 24)

-- {size}/{b} flow into draws
local ctxT2 = {
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
}
TextScene.add_wrapped_spans(ctxT2,
    { { text = "big", size = 36, bold = true }, { text = "xx", bold = true } },
    { x = 32, y = 580, max_width = 1000, line_height = 24,
      font_size = 24, color = { r = 255, g = 255, b = 255 } })
local draws2 = ctxT2.text_state.draws
check("spans: scale and bold on draws", #draws2 == 2
      and draws2[1].scale == 1.5 and draws2[1].bold == true
      and draws2[2].scale == 1 and draws2[2].bold == true)

-- instant draws skip the typewriter (NVL speaker prefix)
local ctxT3 = {
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
}
TextScene.add_wrapped_spans(ctxT3,
    { { text = "「A」：", instant = true }, { text = "x" } },
    { x = 32, y = 580, max_width = 1000, line_height = 24,
      font_size = 24, color = { r = 255, g = 255, b = 255 } })
local draws3 = ctxT3.text_state.draws
check("spans: instant draw not typewriter", #draws3 == 2
      and draws3[1].typewriter == false
      and draws3[2].typewriter == true)

-- ---------------------------------------------------------------------------
-- 4. [ch] integration — draws colored, backlog/reveal use plain text
-- ---------------------------------------------------------------------------
local ctxC = {
    backlog = {},
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
    current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
}
-- seed the message layer so [ch] does not need layer creation
layers.add_layer(nil, {
    name = "message", layer_type = 0,
    x = 0, y = 520, w = 1280, h = 200, visible = true,
})

TextCommands.ch(ctxC, { text = "Hi {color=#00ff00}green{/color}!" })
local drawsC = ctxC.text_state.draws
check("ch: colored draws", #drawsC >= 3)
local greenDraw
for _, d in ipairs(drawsC) do
    if d.text == "green" then greenDraw = d end
end
check("ch: green span drawn", greenDraw ~= nil and greenDraw.g == 255
      and greenDraw.r == 0 and greenDraw.b == 0)
check("ch: backlog plain", ctxC.backlog[1] ~= nil
      and ctxC.backlog[1].text == "Hi green!")
check("ch: reveal plain length", ctxC.reveal ~= nil and ctxC.reveal.total == 9)

local failed = 0
for _, ok in ipairs(results) do
    if not ok then failed = failed + 1 end
end
if failed > 0 then
    print(string.format("TEXT MARKUP TESTS: %d passed, %d FAILED",
        #results - failed, failed))
    os.exit(1)
end
print(string.format("TEXT MARKUP TESTS DONE (%d passed)", #results))
