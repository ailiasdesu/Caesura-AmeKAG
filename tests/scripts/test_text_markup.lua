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
local Schema = require("kag.schema")
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

-- ---------------------------------------------------------------------------
-- 5. [ruby] command — furigana parsing + positioning (phase D)
-- ---------------------------------------------------------------------------
-- measure_ruby: width is max(base, ruby*scale); offsets center each run
-- within that bounding box.
local mr = TextLayout.measure_ruby("漢字", "かんじ", { ruby_scale = 0.5, font_size = 24 })
-- base 2 full-width @24 = 48; ruby 3 full-width @24*0.5 = 36 -> width = 48
check("ruby: measure width = max(base, ruby)", mr.width == 48
      and mr.base_width == 48 and mr.ruby_width == 36)
check("ruby: base centered when ruby wider", (function()
    -- ruby wider: 4 chars (base 1 char => base=24, ruby=4*12=48)
    local m = TextLayout.measure_ruby("字", "かんじみ", { ruby_scale = 0.5, font_size = 24 })
    return m.width == 48 and m.base_offset == 12 and m.ruby_offset == 0
end)())

-- [ruby] cursor-follow (REVIEW-FIX regression): schema.coerce fills x=0/y=0,
-- but a bare [ruby ...] must follow the current text cursor, not pin to (0,0).
local ctxR = {
    backlog = {},
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 200, cursor_y = 300, draws = {},
                   font_size = 24 },
    textCursorX = 200, textCursorY = 300,
    current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
}
local rubyParams = Schema.coerce("ruby", { text = "漢字", ruby = "かんじ" }, ctxR)
TextCommands.ruby(ctxR, rubyParams)
local rd = ctxR.text_state.draws[#ctxR.text_state.draws]
check("ruby: bare command follows cursor", rd ~= nil and rd.x == 200
      and rd.y == 300)
check("ruby: draw carries ruby text + kind",
      rd ~= nil and rd.kind == "ruby" and rd.ruby == "かんじ"
      and rd.layout_width ~= nil)

-- explicit x/y override the cursor (positive only)
local ctxRX = {
    backlog = {},
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 200, cursor_y = 300, draws = {}, font_size = 24 },
    textCursorX = 200, textCursorY = 300,
    current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
}
TextCommands.ruby(ctxRX, Schema.coerce("ruby",
    { text = "字", ruby = "よみ", x = 500, y = 120 }, ctxRX))
local rdX = ctxRX.text_state.draws[#ctxRX.text_state.draws]
check("ruby: explicit position overrides cursor", rdX ~= nil
      and rdX.x == 500.0 and rdX.y == 120)

-- empty text is a no-op (no draw appended)
local ctxRE = {
    backlog = {},
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 200, cursor_y = 300, draws = {}, font_size = 24 },
    textCursorX = 200, textCursorY = 300,
    current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
}
local n0 = #ctxRE.text_state.draws
TextCommands.ruby(ctxRE, Schema.coerce("ruby", { text = "", ruby = "x" }, ctxRE))
check("ruby: empty text no-op", #ctxRE.text_state.draws == n0)

-- ruby wraps to the next line when it would overflow the width
local ctxRW = {
    backlog = {},
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 1000, cursor_y = 300, draws = {}, font_size = 24 },
    textCursorX = 1000, textCursorY = 300,
    current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
}
-- start_x=32, max_width=1216 -> x(1000)+width(48) > 32+32? no. Force overflow:
-- cursor pushed near the right edge so x+width exceeds start_x+max_width.
TextCommands.ruby(ctxRW, Schema.coerce("ruby",
    { text = "漢字", ruby = "かんじ", start_x = 1000, x = 1200, y = 300 }, ctxRW))
local rdW = ctxRW.text_state.draws[#ctxRW.text_state.draws]
check("ruby: overflow wraps to next line", rdW ~= nil and rdW.y == 324)

-- ---------------------------------------------------------------------------
-- 6. nameplate + [ch] name: overlay / replacement (phase D)
-- ---------------------------------------------------------------------------
-- Without a nameplate, [ch name=X] shows a "[X]" label draw.
local ctxN0 = {
    backlog = {},
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 32, cursor_y = 580, draws = {}, font_size = 24 },
    textCursorX = 32, textCursorY = 580,
    current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
}
-- reset current_speaker so the label path is taken
TextCommands.ch(ctxN0, { name = "Ama", text = "hi" })
local nd0
for _, d in ipairs(ctxN0.text_state.draws) do
    if d.text == "[Ama]" then nd0 = d end
end
check("nameplate: absent -> [Name] label draw", nd0 ~= nil)

-- [nameplate] configures ctx.nameplate_style (coerced defaults); the
-- current speaker's plate layer is (re)built with the style dimensions.
local ctxNP = {
    backlog = {},
    f = {}, sf = {}, tf = {}, mp = {}, variables = {}, characters = {},
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 32, cursor_y = 580, draws = {}, font_size = 24 },
    textCursorX = 32, textCursorY = 580,
    current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
}
TextCommands.ch(ctxNP, { name = "Rin", text = "yo" })
ctxNP.current_speaker = "Rin"
TextCommands.nameplate(ctxNP, Schema.coerce("nameplate",
    { x = 300, y = 470, w = 260, h = 40 }, ctxNP))
check("nameplate: style persisted in ctx", ctxNP.nameplate_style ~= nil
      and ctxNP.nameplate_style.x == 300 and ctxNP.nameplate_style.y == 470
      and ctxNP.nameplate_style.w == 260 and ctxNP.nameplate_style.h == 40)
local plate = layers.get("_nameplate")
check("nameplate: plate layer sized", plate ~= nil and plate.x == 300
      and plate.y == 470 and plate.w == 260 and plate.h == 40)

-- [ch name=Y] after [ch name=X] with a nameplate replaces the speaker
-- (overlay/replacement), updating ctx.current_speaker.
local ctxNR = {
    backlog = {},
    f = {}, sf = {}, tf = {}, mp = {}, variables = {}, characters = {},
    nameplate_style = Schema.coerce("nameplate", {}, {}),
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 32, cursor_y = 580, draws = {}, font_size = 24 },
    textCursorX = 32, textCursorY = 580,
    current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
}
TextCommands.ch(ctxNR, { name = "Kai", text = "z" })
check("nameplate: ch sets current speaker", ctxNR.current_speaker == "Kai")
check("nameplate: plate re-rendered for new speaker",
      layers.get("_nameplate") ~= nil and layers.get("_nameplate").visible)

-- ---------------------------------------------------------------------------
-- 7. [font] mid-stream base size — "多段文字不同字体" (phase D): [font size=N]
--    sets the base font size that subsequent [ch]/[text] measurement uses,
--    so a fixed-width long line wraps into MORE lines at a larger size.
-- ---------------------------------------------------------------------------
TextCommands.font(ctxC, Schema.coerce("font", { size = 48 }, ctxC))
check("font: size persisted in ctx.text_state", ctxC.text_state.font_size == 48)
local longS = string.rep("a", 100)
local ctxFD = {
    backlog = {},
    f = {}, sf = {}, tf = {}, mp = {}, variables = {}, characters = {},
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 32, cursor_y = 580, draws = {}, font_size = nil },
    textCursorX = 32, textCursorY = 580,
    current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
}
TextCommands.ch(ctxFD, { text = longS })
local defaultLines = #ctxFD.text_state.draws
local ctxFF = {
    backlog = {},
    f = {}, sf = {}, tf = {}, mp = {}, variables = {}, characters = {},
    text_state = { line = 1, char_offset = 0, opacity = 255,
                   cursor_x = 32, cursor_y = 580, draws = {}, font_size = nil },
    textCursorX = 32, textCursorY = 580,
    current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
}
TextCommands.font(ctxFF, Schema.coerce("font", { size = 48 }, ctxFF))
TextCommands.ch(ctxFF, { text = longS })
check("font: larger size wraps a long line into more lines",
      #ctxFF.text_state.draws > defaultLines)
check("font: per-line typewriter continues (draws still typewriter)",
      ctxFF.text_state.draws[1].typewriter == true)

-- ---------------------------------------------------------------------------
-- 8. long-text wrapping width — line width fits max_width, segments tile x
--    (a wrapped line never exceeds the box; consecutive draws advance x)
-- ---------------------------------------------------------------------------
for _, pref in ipairs({ 32 }) do
    local lines = TextLayout.wrap(string.rep("ab", 40), { max_width = 120, font_size = 24 })
    check("wrap: long text never exceeds max_width", (function()
        for _, ln in ipairs(lines) do
            if ln.width > 120 then return false end
        end
        return #lines > 1
    end)())
end


-- ---------------------------------------------------------------------------
-- 9. {letter_spacing=N} / {spacing=N} — the ONE new typography tag that is
--    actually wired: it feeds measure_character()'s advance, so it changes both
--    glyph advance and the greedy wrap positions.
--
--    These drive the REAL path end to end (markup string -> parse_markup ->
--    span_characters -> measure/wrap_spans, plus the full [ch] command for the
--    integration case). Deliberately NO hand-built character table: a synthetic
--    table would bypass the parse and span layers, which is exactly where a
--    regression would live.
-- ---------------------------------------------------------------------------

-- 9a. parse: recognized under both spellings, scoped by its close tag
local ls1 = TextLayout.parse_markup("ab{letter_spacing=4}cd{/letter_spacing}ef")
check("letter_spacing: three spans, middle one carries the spacing",
      #ls1.spans == 3
      and ls1.spans[1].letter_spacing == nil
      and ls1.spans[2].letter_spacing == 4
      and ls1.spans[3].letter_spacing == nil)
check("letter_spacing: markup stripped from plain text", ls1.plain == "abcdef")

local ls2 = TextLayout.parse_markup("x{spacing=2}y{/spacing}z")
check("letter_spacing: {spacing=N} alias parses", #ls2.spans == 3
      and ls2.spans[2].letter_spacing == 2 and ls2.plain == "xyz")

local ls3 = TextLayout.parse_markup(
    "a{letter_spacing=3}b{letter_spacing=9}c{/letter_spacing}d{/letter_spacing}e")
check("letter_spacing: nested tags restore the outer value on close",
      #ls3.spans == 5
      and ls3.spans[1].letter_spacing == nil
      and ls3.spans[2].letter_spacing == 3
      and ls3.spans[3].letter_spacing == 9
      and ls3.spans[4].letter_spacing == 3
      and ls3.spans[5].letter_spacing == nil)

-- 9b. measurement: exactly N px of extra advance per character
do
    local optsWide = { max_width = 100000, font_size = 24 }
    local plainLine = TextLayout.wrap_spans(
        TextLayout.parse_markup("aaaa").spans, optsWide)[1]
    local spacedLine = TextLayout.wrap_spans(
        TextLayout.parse_markup("{letter_spacing=5}aaaa{/letter_spacing}").spans,
        optsWide)[1]
    -- 4 characters, +5 px each => exactly +20 px of measured width
    check("letter_spacing: width grows by N per character (4 chars, +5 => +20)",
          math.abs((spacedLine.width - plainLine.width) - 20) < 0.001)
    check("letter_spacing: zero spacing measures like no markup",
          math.abs(TextLayout.wrap_spans(
              TextLayout.parse_markup("{letter_spacing=0}aaaa{/letter_spacing}").spans,
              optsWide)[1].width - plainLine.width) < 0.001)
end

-- 9c. THE behavior that matters: the wrap position actually moves.
--     10 ASCII chars at 12 px each fit a 120 px box exactly; +4 px of spacing
--     makes each 16 px, so the line has to break earlier.
do
    local opts = { max_width = 120, font_size = 24 }   -- ASCII advance = 12 px
    local plainLines = TextLayout.wrap_spans(
        TextLayout.parse_markup(string.rep("a", 10)).spans, opts)
    local spacedLines = TextLayout.wrap_spans(
        TextLayout.parse_markup(
            "{letter_spacing=4}" .. string.rep("a", 10) .. "{/letter_spacing}").spans,
        opts)
    check("letter_spacing: unspaced 10 chars fit one line", #plainLines == 1)
    check("letter_spacing: spacing forces an extra line", #spacedLines > #plainLines)
    check("letter_spacing: first spaced line holds fewer chars",
          utf8.len(spacedLines[1].text) < utf8.len(plainLines[1].text))
    check("letter_spacing: no spaced line exceeds max_width", (function()
        for _, ln in ipairs(spacedLines) do
            if ln.width > opts.max_width + 0.001 then return false end
        end
        return true
    end)())
    check("letter_spacing: all characters preserved across the re-wrap", (function()
        local total = 0
        for _, ln in ipairs(spacedLines) do total = total + (utf8.len(ln.text) or 0) end
        return total == 10
    end)())
end

-- 9d. a mid-line spacing change splits the segment AND widens only that part
do
    local opts = { max_width = 100000, font_size = 24 }
    local line = TextLayout.wrap_spans(
        TextLayout.parse_markup("aa{letter_spacing=6}bb{/letter_spacing}cc").spans,
        opts)[1]
    check("letter_spacing: mid-line change splits into 3 segments",
          #line.segments == 3)
    -- 12 px/char: "aa" and "cc" measure 24, "bb" measures 24 + 2*6 = 36
    check("letter_spacing: only the spaced segment is wider",
          math.abs(line.segments[1].width - 24) < 0.001
          and math.abs(line.segments[2].width - 36) < 0.001
          and math.abs(line.segments[3].width - 24) < 0.001)
end

-- 9e. negative spacing tightens but can never measure backwards
--     (measure_character clamps the per-character advance at 0, so an absurd
--     negative value cannot produce a negative width and hang the greedy wrap).
do
    local opts = { max_width = 120, font_size = 24 }
    local tight = TextLayout.wrap_spans(
        TextLayout.parse_markup(
            "{letter_spacing=-4}" .. string.rep("a", 10) .. "{/letter_spacing}").spans,
        opts)
    check("letter_spacing: negative spacing still fits one line", #tight == 1)
    check("letter_spacing: negative spacing narrows the line",
          tight[1].width < 120 and tight[1].width > 0)
    local absurd = TextLayout.wrap_spans(
        TextLayout.parse_markup(
            "{letter_spacing=-9999}" .. string.rep("a", 10) .. "{/letter_spacing}").spans,
        opts)
    check("letter_spacing: absurd negative spacing terminates with width 0",
          #absurd == 1 and absurd[1].width == 0)
end

-- 9f. integration through the real [ch] command: a spaced long line produces
--     MORE typewriter draws (one per wrapped line) than the unspaced one.
do
    -- 300 chars, not 100: with 100 the plain text already wraps to 2 lines and
    -- the spaced one also lands on 2 (the last line is short either way), so a
    -- draw-count assertion at 100 would pass or fail on the ceil boundary rather
    -- than on the behavior. The per-line length assertion below is the robust
    -- signal at any length; the count assertion needs enough text for the
    -- narrower lines to accumulate into an extra one.
    local longA = string.rep("a", 300)
    local mk = function()
        return {
            backlog = {},
            f = {}, sf = {}, tf = {}, mp = {}, variables = {}, characters = {},
            text_state = { line = 1, char_offset = 0, opacity = 255,
                           cursor_x = 32, cursor_y = 580, draws = {}, font_size = nil },
            textCursorX = 32, textCursorY = 580,
            current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
        }
    end
    local ctxLS0 = mk()
    TextCommands.ch(ctxLS0, { text = longA })
    local ctxLS1 = mk()
    TextCommands.ch(ctxLS1, { text = "{letter_spacing=6}" .. longA .. "{/letter_spacing}" })
    check("letter_spacing: [ch] spacing increases the wrapped draw count",
          #ctxLS1.text_state.draws > #ctxLS0.text_state.draws)
    check("letter_spacing: [ch] first rendered line is shorter when spaced",
          utf8.len(ctxLS1.text_state.draws[1].text)
              < utf8.len(ctxLS0.text_state.draws[1].text))
    check("letter_spacing: [ch] draws stay typewriter-revealed",
          ctxLS1.text_state.draws[1].typewriter == true)
    -- The rendered draw text is the markup-stripped plain text: the tag must
    -- never leak into what the player sees.
    check("letter_spacing: markup absent from the rendered draw text", (function()
        for _, d in ipairs(ctxLS1.text_state.draws) do
            if type(d.text) == "string" and d.text:find("letter_spacing", 1, true) then
                return false
            end
        end
        return true
    end)())
end

-- 9g. {font=} and {line_height=} parse and reach the segment, but nothing
--     downstream consumes them (see the WIRING STATUS block in
--     kag/text_layout.lua). Pinned so the not-wired state is explicit and a
--     future wiring change has to update this test deliberately.
do
    local m = TextLayout.parse_markup("a{font=Serif}b{/font}c")
    check("font: parses into the span (still unconsumed downstream)",
          #m.spans == 3 and m.spans[2].font == "Serif" and m.plain == "abc")
    local lh = TextLayout.parse_markup("a{line_height=40}b{/line_height}c")
    check("line_height: parses into the span (still unconsumed downstream)",
          #lh.spans == 3 and lh.spans[2].line_height == 40 and lh.plain == "abc")
    -- Both DO split segments (same_style compares them) even though no consumer
    -- reads the values -- that split is their only observable effect today.
    local opts = { max_width = 100000, font_size = 24 }
    local fontLine = TextLayout.wrap_spans(m.spans, opts)[1]
    check("font: a font change splits segments",
          #fontLine.segments == 3 and fontLine.segments[2].font == "Serif")
    local lhLine = TextLayout.wrap_spans(lh.spans, opts)[1]
    check("line_height: a line_height change splits segments",
          #lhLine.segments == 3 and lhLine.segments[2].line_height == 40)
    -- ...and neither affects measured width (they are not advance-affecting)
    local plainLine = TextLayout.wrap_spans(
        TextLayout.parse_markup("abc").spans, opts)[1]
    check("font/line_height do not affect measured width",
          math.abs(fontLine.width - plainLine.width) < 0.001
          and math.abs(lhLine.width - plainLine.width) < 0.001)
end

-- ---------------------------------------------------------------------------
-- 9h. VISUAL (t105): a letter_spaced segment renders ONE DRAW PER CHARACTER
--     with x advancing by glyph advance + spacing. t101 verified the engine's
--     render_text has no per-glyph spacing parameter (11-arg KAG binding), so
--     the visual gap must be produced by per-character draws; the x sequence
--     must replicate the layout's per-character advance exactly
--     (rawWidth * scale + spacing, clamped >= 0) so glyph positions stay
--     aligned with measure/wrap.
-- ---------------------------------------------------------------------------
do
    local function mkctx()
        return {
            text_state = { line = 1, char_offset = 0, opacity = 255,
                           cursor_x = 32, cursor_y = 580, draws = {} },
            textCursorX = 32, textCursorY = 580,
        }
    end
    local opts = { x = 100, y = 580, max_width = 100000, line_height = 24,
                   font_size = 24, color = { r = 255, g = 255, b = 255 } }

    -- spaced span: two chars -> two draws, x = 100 then 100 + 12 + 4
    local ctxV = mkctx()
    TextScene.add_wrapped_spans(ctxV,
        TextLayout.parse_markup("{letter_spacing=4}ab{/letter_spacing}").spans,
        opts)
    local d = ctxV.text_state.draws
    check("letter_spacing visual: one draw per character",
          #d == 2 and d[1].text == "a" and d[2].text == "b")
    check("letter_spacing visual: x advances by glyph + spacing",
          d[1].x == 100 and math.abs(d[2].x - 116) < 0.001)

    -- segment boundary: the next segment starts at seg.width, which counts
    -- the LAST char's spacing too -- so the per-char loop must also advance
    -- past the final spacing (x_cc = 158 = 100 + 24 + 34).
    local ctxV2 = mkctx()
    TextScene.add_wrapped_spans(ctxV2,
        TextLayout.parse_markup(
            "aa{letter_spacing=5}bb{/letter_spacing}cc").spans, opts)
    local d2 = ctxV2.text_state.draws
    check("letter_spacing visual: mixed line = single + per-char + single",
          #d2 == 4 and d2[1].text == "aa" and d2[2].text == "b"
          and d2[3].text == "b" and d2[4].text == "cc")
    check("letter_spacing visual: next segment aligns with seg.width",
          d2[1].x == 100 and math.abs(d2[2].x - 124) < 0.001
          and math.abs(d2[3].x - 141) < 0.001
          and math.abs(d2[4].x - 158) < 0.001)

    -- UTF-8 multibyte: CJK is full-width (24 px at font 24); the second
    -- character x = 100 + 24 + 3 = 127 -- character boundary, not bytes.
    local ctxV3 = mkctx()
    TextScene.add_wrapped_spans(ctxV3,
        TextLayout.parse_markup("{letter_spacing=3}中b{/letter_spacing}").spans,
        opts)
    local d3 = ctxV3.text_state.draws
    check("letter_spacing visual: multibyte split on char boundary",
          #d3 == 2 and d3[1].text == "中" and d3[2].text == "b"
          and d3[1].x == 100 and math.abs(d3[2].x - 127) < 0.001)

    -- regression: spacing=0 and no spacing keep the single-draw path
    local ctxZ = mkctx()
    TextScene.add_wrapped_spans(ctxZ,
        TextLayout.parse_markup("{letter_spacing=0}ab{/letter_spacing}").spans,
        opts)
    check("letter_spacing visual: spacing=0 single draw",
          #ctxZ.text_state.draws == 1 and ctxZ.text_state.draws[1].text == "ab")
    local ctxP = mkctx()
    TextScene.add_wrapped_spans(ctxP,
        TextLayout.parse_markup("ab").spans, opts)
    check("letter_spacing visual: unspaced span single draw (zero regression)",
          #ctxP.text_state.draws == 1 and ctxP.text_state.draws[1].text == "ab"
          and ctxP.text_state.draws[1].x == 100)
end

-- 9i. typewriter x per-character spacing: partial reveal shows exactly the
--     first k characters as full glyphs at their laid-out x positions (each
--     per-char draw is its own typewriter unit, so the truncation math is
--     exact and the visible prefix stays aligned).
do
    local ctxT = {
        text_state = { line = 1, char_offset = 0, opacity = 255,
                       cursor_x = 32, cursor_y = 580, draws = {} },
        textCursorX = 32, textCursorY = 580,
    }
    TextScene.add_wrapped_spans(ctxT,
        TextLayout.parse_markup("{letter_spacing=4}abc{/letter_spacing}").spans,
        { x = 100, y = 580, max_width = 100000, line_height = 24,
          font_size = 24, color = { r = 255, g = 255, b = 255 } })
    local calls = {}
    local function mock(text, x)
        calls[#calls + 1] = { text = text, x = x }
    end
    local mock_backend = {
        render_text = mock,
        render_ruby = function() end,
    }
    ctxT.text_state.reveal_chars = 2
    TextScene.render(ctxT, mock_backend)
    check("typewriter x spacing: reveal=2 shows exactly the first two glyphs",
          #calls == 3 and calls[1].text == "a" and calls[2].text == "b"
          and calls[3].text == "")
    check("typewriter x spacing: revealed glyphs at laid-out x positions",
          calls[1].x == 100 and math.abs(calls[2].x - 116) < 0.001)
end

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
