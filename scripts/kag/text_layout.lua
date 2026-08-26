-- Pure UTF-8 text layout helpers used by KAG text commands.

local TextLayout = {}

local OPENING_PUNCTUATION = {
    -- ASCII brackets
    [0x0028] = true, -- (
    [0x005B] = true, -- [
    [0x007B] = true, -- {
    -- Quotes
    [0x00AB] = true, -- «
    [0x2018] = true, -- ‘
    [0x201C] = true, -- “
    [0x2039] = true, -- ‹
    -- CJK brackets
    [0x3008] = true, -- 〈
    [0x300A] = true, -- 《
    [0x300C] = true, -- 「
    [0x300E] = true, -- 『
    [0x3010] = true, -- 【
    [0x3014] = true, -- 〔
    [0x3016] = true, -- 〖
    [0x3018] = true, -- 〘
    [0x301A] = true, -- 〚
    [0xFE59] = true, -- ﹙
    [0xFE5B] = true, -- ﹛
    [0xFE5D] = true, -- ﹝
    [0xFF08] = true, -- （
    [0xFF3B] = true, -- ［
    [0xFF5B] = true, -- ｛
    [0xFF5F] = true, -- ｟
    [0xFF62] = true, -- ｢
    -- Currency and prefix symbols (cannot end a line)
    [0x0023] = true, -- #
    [0x0024] = true, -- $
    [0x00A3] = true, -- £
    [0x00A5] = true, -- ¥
    [0x00A7] = true, -- §
    [0x20AC] = true, -- €
    [0x20A9] = true, -- ₩
    [0x2116] = true, -- №
    [0xFF03] = true, -- ＃
    [0xFF04] = true, -- ＄
    [0xFFE1] = true, -- ￡
    [0xFFE5] = true, -- ￥
    [0xFFE6] = true, -- ￦
}

local CLOSING_PUNCTUATION = {
    -- ASCII closing punctuation and quotes
    [0x0021] = true, -- !
    [0x0022] = true, -- "
    [0x0027] = true, -- '
    [0x0029] = true, -- )
    [0x002C] = true, -- ,
    [0x002E] = true, -- .
    [0x003A] = true, -- :
    [0x003B] = true, -- ;
    [0x003F] = true, -- ?
    [0x005D] = true, -- ]
    [0x007D] = true, -- }
    -- Quotes
    [0x00BB] = true, -- »
    [0x2019] = true, -- ’
    [0x201D] = true, -- ”
    [0x203A] = true, -- ›
    -- CJK closing brackets
    [0x3009] = true, -- 〉
    [0x300B] = true, -- 》
    [0x300D] = true, -- 」
    [0x300F] = true, -- 』
    [0x3011] = true, -- 】
    [0x3015] = true, -- 〕
    [0x3017] = true, -- 〗
    [0x3019] = true, -- 㙹
    [0x301B] = true, -- 㛼
    [0xFE5A] = true, -- ﹚
    [0xFE5C] = true, -- ﹜
    [0xFE5E] = true, -- ﹞
    [0xFF09] = true, -- ）
    [0xFF3D] = true, -- ］
    [0xFF5D] = true, -- ｝
    [0xFF60] = true, -- ｠
    [0xFF63] = true, -- ｣
    -- CJK commas, periods, marks
    [0x3001] = true, -- 、
    [0x3002] = true, -- 。
    [0xFE50] = true, -- ﹐
    [0xFE51] = true, -- ﹑
    [0xFE52] = true, -- ﹒
    [0xFE54] = true, -- ﹔
    [0xFE55] = true, -- ﹕
    [0xFE56] = true, -- ﹖
    [0xFE57] = true, -- ﹗
    [0xFF01] = true, -- ！
    [0xFF0C] = true, -- ，
    [0xFF0E] = true, -- ．
    [0xFF1A] = true, -- ：
    [0xFF1B] = true, -- ；
    [0xFF1F] = true, -- ？
    [0xFF61] = true, -- ｡
    [0xFF64] = true, -- ､
    -- Connecting / middle dots / ellipsis / dashes / prolonging / iteration marks
    [0x00B7] = true, -- ·
    [0x2014] = true, -- —
    [0x2015] = true, -- ―
    [0x2025] = true, -- ‥
    [0x2026] = true, -- …
    [0x3005] = true, -- 々
    [0x301C] = true, -- 〜
    [0x303B] = true, -- 〻
    [0x303C] = true, -- 〼
    [0x309D] = true, -- ゝ
    [0x309E] = true, -- ゞ
    [0x30FB] = true, -- ・
    [0x30FC] = true, -- ー
    [0x30FD] = true, -- ヽ
    [0x30FE] = true, -- ヾ
    [0xFF5E] = true, -- ～
    [0xFF65] = true, -- ･
    [0xFF70] = true, -- ｰ
    -- Japanese Small Hiragana
    [0x3041] = true, -- ぁ
    [0x3043] = true, -- ぃ
    [0x3045] = true, -- ぅ
    [0x3047] = true, -- ぇ
    [0x3049] = true, -- ぉ
    [0x3063] = true, -- っ
    [0x3083] = true, -- ゃ
    [0x3085] = true, -- ゅ
    [0x3087] = true, -- ょ
    [0x308E] = true, -- ゎ
    [0x3095] = true, -- ゕ
    [0x3096] = true, -- ゖ
    -- Japanese Small Katakana
    [0x30A1] = true, -- ァ
    [0x30A3] = true, -- ィ
    [0x30A5] = true, -- ゥ
    [0x30A7] = true, -- ェ
    [0x30A9] = true, -- ォ
    [0x30C3] = true, -- ッ
    [0x30E3] = true, -- ャ
    [0x30E5] = true, -- ュ
    [0x30E7] = true, -- ョ
    [0x30EE] = true, -- ヮ
    [0x30F5] = true, -- ヵ
    [0x30F6] = true, -- ヶ
    -- Units / Symbols
    [0x0025] = true, -- %
    [0x00B0] = true, -- °
    [0x2032] = true, -- ′
    [0x2033] = true, -- ″
    [0x2103] = true, -- ℃
    [0xFF05] = true, -- ％
}
for cp = 0xFF67, 0xFF6F do CLOSING_PUNCTUATION[cp] = true end
for cp = 0x31F0, 0x31FF do CLOSING_PUNCTUATION[cp] = true end

local function is_combining(codepoint)
    return (codepoint >= 0x0300 and codepoint <= 0x036F)
        or (codepoint >= 0x1AB0 and codepoint <= 0x1AFF)
        or (codepoint >= 0x1DC0 and codepoint <= 0x1DFF)
        or (codepoint >= 0x20D0 and codepoint <= 0x20FF)
        or (codepoint >= 0xFE20 and codepoint <= 0xFE2F)
end

local function is_full_width(codepoint)
    return (codepoint >= 0x1100 and codepoint <= 0x115F)
        or (codepoint >= 0x2E80 and codepoint <= 0xA4CF)
        or (codepoint >= 0xAC00 and codepoint <= 0xD7A3)
        or (codepoint >= 0xF900 and codepoint <= 0xFAFF)
        or (codepoint >= 0xFE10 and codepoint <= 0xFE6F)
        or (codepoint >= 0xFF01 and codepoint <= 0xFF60)
        or (codepoint >= 0xFFE0 and codepoint <= 0xFFE6)
        or (codepoint >= 0x1F300 and codepoint <= 0x1FAFF)
end

local function default_measure(_, codepoint, options)
    local font_size = tonumber(options.font_size) or 24
    if is_combining(codepoint) then return 0 end
    if codepoint == 0x09 then return font_size * 2 end
    if codepoint == 0x20 then return font_size * 0.5 end
    if is_full_width(codepoint) then return font_size end
    return font_size * 0.5
end

local function to_characters(text)
    local characters = {}
    for _, codepoint in utf8.codes(tostring(text or "")) do
        characters[#characters + 1] = {
            text = utf8.char(codepoint),
            codepoint = codepoint,
        }
    end
    return characters
end

local function is_ascii_word(codepoint)
    return (codepoint >= 0x30 and codepoint <= 0x39)
        or (codepoint >= 0x41 and codepoint <= 0x5A)
        or (codepoint >= 0x61 and codepoint <= 0x7A)
        or codepoint == 0x27
        or codepoint == 0x5F
end

local function is_space(character)
    return character.codepoint == 0x20 or character.codepoint == 0x09
end

local function can_break(left, right, preserve_words)
    if not left or not right then return true end
    if OPENING_PUNCTUATION[left.codepoint] then return false end
    if CLOSING_PUNCTUATION[right.codepoint] then return false end
    if is_combining(right.codepoint) then return false end
    -- Inseparable consecutive punctuation pairs
    if (left.codepoint == 0x2026 and right.codepoint == 0x2026)
        or (left.codepoint == 0x2025 and right.codepoint == 0x2025)
        or (left.codepoint == 0x2014 and right.codepoint == 0x2014)
        or (left.codepoint == 0x2015 and right.codepoint == 0x2015) then
        return false
    end
    if preserve_words
        and is_ascii_word(left.codepoint)
        and is_ascii_word(right.codepoint) then
        return false
    end
    return true
end

local function measure_character(character, options)
    local measure = options.measure_char or default_measure
    local width = tonumber(measure(
        character.text, character.codepoint, options)) or 0
    width = math.max(width, 0)
    -- {size=N} markup: scale the advance by the span's relative size
    local scale = character.scale or 1
    -- {letter_spacing=N} markup: this is THE wiring point for that tag. Because
    -- every width decision (measure_range, wrap_paragraph's fit test, segment
    -- widths that position the next draw) funnels through here, adding the extra
    -- advance makes the tag genuinely take effect on both glyph advance and line
    -- breaking. Negative values are allowed (tightening) but the per-character
    -- advance is clamped at 0 below so a large negative spacing can never make a
    -- line measure backwards, which would let wrap_paragraph loop forever.
    local extra_spacing = tonumber(character.letter_spacing) or 0
    return math.max(0, (width * scale) + extra_spacing)
end

local function measure_range(characters, first, last, options)
    local width = 0
    for i = first, last do
        width = width + measure_character(characters[i], options)
    end
    return width
end

local function join_range(characters, first, last)
    local parts = {}
    for i = first, last do
        parts[#parts + 1] = characters[i].text
    end
    return table.concat(parts)
end

local function append_line(lines, characters, first, last, options)
    while last >= first and is_space(characters[last]) do
        last = last - 1
    end
    if last < first then return end

    lines[#lines + 1] = {
        text = join_range(characters, first, last),
        width = measure_range(characters, first, last, options),
    }
end

-- Span-aware append: groups consecutive characters that share the same
-- inline style (color / scale / bold / font / spacing) into segments
-- so the caller can emit one draw per segment.
--
-- NOTE on segment.font and segment.line_height: both are emitted here and both
-- participate in same_style() (so changing either really does start a new
-- segment), but NO downstream consumer reads them -- see the "WIRING STATUS"
-- block above parse_open_tag(). They are carried, not honored. Do not read this
-- as "the feature works"; the segment split is the only observable effect today.
local function append_line_segments(lines, characters, first, last, options)
    while last >= first and is_space(characters[last]) do
        last = last - 1
    end
    if last < first then return end

    local segments = {}
    local seg_first = first
    local color = characters[first].color
    local scale = characters[first].scale
    local bold = characters[first].bold
    local italic = characters[first].italic
    local strike = characters[first].strike
    local instant = characters[first].instant
    local font = characters[first].font
    local letter_spacing = characters[first].letter_spacing
    local line_height = characters[first].line_height
    local function same_style(a, b)
        return a.color == b.color and a.scale == b.scale
            and a.bold == b.bold and a.italic == b.italic
            and a.strike == b.strike and a.instant == b.instant
            and a.font == b.font and a.letter_spacing == b.letter_spacing
            and a.line_height == b.line_height
    end
    for i = first + 1, last + 1 do
        local c = characters[i]
        if i > last or not same_style(c, characters[i - 1]) then
            segments[#segments + 1] = {
                text = join_range(characters, seg_first, i - 1),
                color = color,
                size = characters[seg_first].size,
                scale = scale,
                bold = bold,
                italic = italic,
                strike = strike,
                instant = instant,
                font = font,                    -- carried, NOT consumed (see above)
                letter_spacing = letter_spacing,-- already applied inside width
                line_height = line_height,      -- carried, NOT consumed (see above)
                width = measure_range(characters, seg_first, i - 1, options),
            }
            if i > last then break end
            seg_first = i
            color = characters[i].color
            scale = characters[i].scale
            bold = characters[i].bold
            italic = characters[i].italic
            strike = characters[i].strike
            instant = characters[i].instant
            font = characters[i].font
            letter_spacing = characters[i].letter_spacing
            line_height = characters[i].line_height
        end
    end
    lines[#lines + 1] = {
        segments = segments,
        width = measure_range(characters, first, last, options),
        text = join_range(characters, first, last),
    }
end

local function wrap_paragraph(characters, options, lines, append)
    append = append or append_line
    if #characters == 0 then
        lines[#lines + 1] = { text = "", width = 0 }
        return
    end

    local max_width = options.max_width
    local first = 1
    while first <= #characters do
        while first <= #characters and is_space(characters[first]) do
            first = first + 1
        end
        if first > #characters then break end

        local width = 0
        local last_fit = first - 1
        for i = first, #characters do
            local next_width = width + measure_character(characters[i], options)
            if last_fit >= first and next_width > max_width then break end
            width = next_width
            last_fit = i
            if width > max_width then break end
        end

        if last_fit >= #characters then
            append(lines, characters, first, #characters, options)
            break
        end

        local break_at
        for i = last_fit, first, -1 do
            if can_break(characters[i], characters[i + 1], true) then
                break_at = i
                break
            end
        end
        if not break_at then
            for i = last_fit, first, -1 do
                if can_break(characters[i], characters[i + 1], false) then
                    break_at = i
                    break
                end
            end
        end
        break_at = break_at or math.max(first, last_fit)

        append(lines, characters, first, break_at, options)
        first = break_at + 1
    end
end

function TextLayout.measure(text, options)
    options = options or {}
    local characters = to_characters(text)
    return measure_range(characters, 1, #characters, options)
end

function TextLayout.wrap(text, options)
    options = options or {}
    local max_width = tonumber(options.max_width)
    assert(max_width and max_width > 0,
        "text layout max_width must be a positive number")

    local lines = {}
    local paragraph = {}
    local characters = to_characters(text)
    for _, character in ipairs(characters) do
        if character.codepoint == 0x0A then
            wrap_paragraph(paragraph, options, lines)
            paragraph = {}
        elseif character.codepoint ~= 0x0D then
            paragraph[#paragraph + 1] = character
        end
    end

    if #paragraph > 0 or #characters == 0
        or characters[#characters].codepoint ~= 0x0A then
        wrap_paragraph(paragraph, options, lines)
    elseif characters[#characters].codepoint == 0x0A then
        lines[#lines + 1] = { text = "", width = 0 }
    end

    return lines
end

function TextLayout.measure_ruby(base_text, ruby_text, options)
    options = options or {}
    local ruby_scale = tonumber(options.ruby_scale) or 0.5
    local base_width = TextLayout.measure(base_text, options)
    local ruby_width = TextLayout.measure(ruby_text, options) * ruby_scale
    local width = math.max(base_width, ruby_width)
    return {
        width = width,
        base_width = base_width,
        ruby_width = ruby_width,
        base_offset = (width - base_width) * 0.5,
        ruby_offset = (width - ruby_width) * 0.5,
    }
end

function TextLayout.is_opening_punctuation(text)
    local characters = to_characters(text)
    return #characters == 1
        and OPENING_PUNCTUATION[characters[1].codepoint] == true
end

function TextLayout.is_closing_punctuation(text)
    local characters = to_characters(text)
    return #characters == 1
        and CLOSING_PUNCTUATION[characters[1].codepoint] == true
end

-- ---------------------------------------------------------------------------
-- Inline text markup (Neo-Genesis; Ren'Py `{...}` parity):
--   {color=#RRGGBB} ... {/color}  — per-span text color (rendered)
--   {b}/{/b}                       — synthetic bold (rendered, double-pass)
--   {i}/{/i}                       — italic shear (rendered, top-edge offset)
--   {s}/{/s}                       — strikethrough (rendered, middle bar)
--   {size=N}/{/size}               — absolute font size (rendered, scaling)
--   {font=...}/{/font}             — per-span font family (PARSED, NOT WIRED)
--   {letter_spacing=N} / {spacing=N} / {/letter_spacing} — extra char spacing
--                                     (WIRED: changes advance and wrapping)
--   {line_height=N} / {/line_height} — span line height (PARSED, NOT WIRED)
--   Unknown {tags} pass through as literal text.
--
-- ---------------------------------------------------------------------------
-- WIRING STATUS of the three markup tags added in this batch. Established by
-- grepping the whole repo for downstream readers PER FEATURE, not as a blanket
-- claim: "the markup parses" is not the same as "the engine honors it".
--
--   {letter_spacing=N} / {spacing=N}  ==>  WIRED, actually takes effect.
--     measure_character() adds it to the per-character advance
--     ((width * scale) + extra_spacing), so it changes glyph advance AND the
--     greedy wrap positions. Locked by behavior tests in
--     tests/scripts/test_text_markup.lua (the "letter_spacing:" cases), which
--     drive the real path: markup string -> parse_markup -> span_characters ->
--     measure/wrap, never a hand-built character table.
--
--   {line_height=N}  ==>  PARSED and carried into the span/segment, NOT WIRED.
--     append_line_segments() folds it into every segment and same_style()
--     compares it (so changing it does split segments), but NOTHING downstream
--     reads segment.line_height. What actually drives line advance is the
--     BLOCK-level options.line_height read in kag/text_scene.lua
--     (add_wrapped :132, add_wrapped_spans :156, add_ruby :186), so a span-level
--     line height is computed and then dropped.
--     Who should wire it, and where: kag/text_scene.lua add_wrapped_spans()
--     would have to advance y by max(segment.line_height) across each line's
--     segments instead of the single options.line_height. That changes layout
--     semantics (mixed line heights within one wrapped line, plus how it
--     interacts with the typewriter reveal's per-draw y), i.e. NEW behavior
--     rather than closing out this batch -- deliberately not done here.
--
--   {font=...}  ==>  PARSED and carried into the span/segment, NOT WIRED, and
--     not wirable from Lua alone. text_scene.lua reads no .font, and the render
--     chain carries no font argument at all:
--       text_scene.lua:267  render_backend.render_text(text, x, y, r,g,b,a,
--                                                      scale, bold, italic, strike)
--       -> backend.lua:280  Backend.render_text(the same 11 args)
--       -> KAGBinding.cpp   lua_KAG_render_text (reads exactly those 11)
--       -> IRenderDevice::renderText(viewId, text, x, y, r,g,b,a, scale,
--                                    bold, italic, strike)
--     The only font switch is the GLOBAL, stateful backend.text_set_font(face,
--     size) used by the block-level [font] command; it reloads the entire TTF
--     atlas and invalidates the text cache, so calling it per span would thrash
--     the atlas on every draw.
--     Who should wire it, and where: whoever owns the render interface. It needs
--     (1) a font parameter threaded through backend.render_text -> KAGBinding ->
--     IRenderDevice/TextRenderer, and (2) TextRenderer keeping more than one
--     resident TTF atlas so a per-span family does not force a reload. Both are
--     outside this batch's file set and are new capability, not WIP closure.
-- ---------------------------------------------------------------------------

local function parse_open_tag(tag)
    local name = tag:match("^([%a_]+)")
    if not name then return nil end
    if name == "i" then return { kind = "italic" } end
    if name == "b" then return { kind = "bold" } end
    if name == "s" then return { kind = "strike" } end
    if name == "size" then
        local size = tonumber(tag:match("^size%s*=%s*(%d+)%s*$"))
        if size then return { kind = "size", size = size } end
        return nil
    end
    if name == "font" then
        local font = tag:match("^font%s*=%s*[\"']?([^\"'}]+)[\"']?%s*$")
        if font then return { kind = "font", font = font } end
        return nil
    end
    if name == "letter_spacing" or name == "spacing" then
        local ls = tonumber(tag:match("^[%a_]+%s*=%s*([%-%d%.]+)%s*$"))
        if ls then return { kind = "letter_spacing", letter_spacing = ls } end
        return nil
    end
    if name == "line_height" then
        local lh = tonumber(tag:match("^line_height%s*=%s*([%-%d%.]+)%s*$"))
        if lh then return { kind = "line_height", line_height = lh } end
        return nil
    end
    if name == "color" then
        local hex = tag:match("^color%s*=%s*#?(%x%x%x%x%x%x)%s*$")
        if hex then
            return {
                kind = "color",
                r = tonumber(hex:sub(1, 2), 16),
                g = tonumber(hex:sub(3, 4), 16),
                b = tonumber(hex:sub(5, 6), 16),
            }
        end
        return nil
    end
    return nil
end

local MARKUP_CLOSE_NAMES = {
    color = true, b = true, i = true, size = true, s = true,
    font = true, letter_spacing = true, spacing = true, line_height = true,
}

--- TextLayout.parse_markup(text) → { spans = {{text, color, size, bold, italic, font, letter_spacing, line_height}}, plain }
--  Splits a message into styled spans and returns the markup-stripped
--  plain text (backlog / reveal counters use the visible characters only).
--  size is an absolute font size (px); bold/italic are booleans.
function TextLayout.parse_markup(text)
    text = tostring(text or "")
    local spans = {}
    local plain_parts = {}
    local color_stack = {}
    local size_stack = {}
    local bold_stack = {}
    local italic_stack = {}
    local strike_stack = {}
    local font_stack = {}
    local letter_spacing_stack = {}
    local line_height_stack = {}
    local current_color = nil
    local current_size = nil
    local current_bold = false
    local current_italic = false
    local current_strike = false
    local current_font = nil
    local current_letter_spacing = nil
    local current_line_height = nil
    local buf = {}

    local function flush()
        if #buf == 0 then return end
        local s = table.concat(buf)
        buf = {}
        spans[#spans + 1] = {
            text = s,
            color = current_color,
            size = current_size,
            bold = current_bold,
            italic = current_italic,
            strike = current_strike,
            font = current_font,
            letter_spacing = current_letter_spacing,
            line_height = current_line_height,
        }
        plain_parts[#plain_parts + 1] = s
    end

    local i, n = 1, #text
    while i <= n do
        if text:sub(i, i) ~= "{" then
            buf[#buf + 1] = text:sub(i, i)
            i = i + 1
        else
            local close_at = text:find("}", i, true)
            if close_at then
                local tag = text:sub(i + 1, close_at - 1)
                local parsed = parse_open_tag(tag)
                if parsed then
                    flush()
                    if parsed.kind == "color" then
                        color_stack[#color_stack + 1] = parsed
                        current_color = parsed
                    elseif parsed.kind == "size" then
                        size_stack[#size_stack + 1] = parsed.size
                        current_size = parsed.size
                    elseif parsed.kind == "bold" then
                        bold_stack[#bold_stack + 1] = true
                        current_bold = true
                    elseif parsed.kind == "italic" then
                        italic_stack[#italic_stack + 1] = true
                        current_italic = true
                    elseif parsed.kind == "strike" then
                        strike_stack[#strike_stack + 1] = true
                        current_strike = true
                    elseif parsed.kind == "font" then
                        font_stack[#font_stack + 1] = parsed.font
                        current_font = parsed.font
                    elseif parsed.kind == "letter_spacing" then
                        letter_spacing_stack[#letter_spacing_stack + 1] = parsed.letter_spacing
                        current_letter_spacing = parsed.letter_spacing
                    elseif parsed.kind == "line_height" then
                        line_height_stack[#line_height_stack + 1] = parsed.line_height
                        current_line_height = parsed.line_height
                    end
                    i = close_at + 1
                else
                    local close_name = tag:match("^%s*/([%a_]+)%s*$")
                    if close_name and MARKUP_CLOSE_NAMES[close_name] then
                        flush()
                        if close_name == "color" and #color_stack > 0 then
                            color_stack[#color_stack] = nil
                            current_color = color_stack[#color_stack]
                        elseif close_name == "size" and #size_stack > 0 then
                            size_stack[#size_stack] = nil
                            current_size = size_stack[#size_stack]
                        elseif close_name == "b" and #bold_stack > 0 then
                            bold_stack[#bold_stack] = nil
                            current_bold = #bold_stack > 0
                        elseif close_name == "i" and #italic_stack > 0 then
                            italic_stack[#italic_stack] = nil
                            current_italic = #italic_stack > 0
                        elseif close_name == "s" and #strike_stack > 0 then
                            strike_stack[#strike_stack] = nil
                            current_strike = #strike_stack > 0
                        elseif close_name == "font" and #font_stack > 0 then
                            font_stack[#font_stack] = nil
                            current_font = font_stack[#font_stack]
                        elseif (close_name == "letter_spacing" or close_name == "spacing") and #letter_spacing_stack > 0 then
                            letter_spacing_stack[#letter_spacing_stack] = nil
                            current_letter_spacing = letter_spacing_stack[#letter_spacing_stack]
                        elseif close_name == "line_height" and #line_height_stack > 0 then
                            line_height_stack[#line_height_stack] = nil
                            current_line_height = line_height_stack[#line_height_stack]
                        end
                        i = close_at + 1
                    else
                        buf[#buf + 1] = "{"
                        i = i + 1
                    end
                end
            else
                buf[#buf + 1] = "{"
                i = i + 1
            end
        end
    end
    flush()
    if #spans == 0 then
        spans = { { text = text, color = nil, size = nil, bold = false,
                    italic = false, strike = false, font = nil,
                    letter_spacing = nil, line_height = nil } }
    end
    return { spans = spans, plain = table.concat(plain_parts) }
end

local function span_characters(spans, options)
    local baseSize = tonumber(options and options.font_size) or 24
    local characters = {}
    for _, span in ipairs(spans) do
        local scale = 1.0
        if span.size then
            scale = baseSize > 0 and (span.size / baseSize) or 1.0
        end
        for _, codepoint in utf8.codes(span.text) do
            characters[#characters + 1] = {
                text = utf8.char(codepoint),
                codepoint = codepoint,
                color = span.color,
                size = span.size,
                scale = scale,
                bold = span.bold == true,
                italic = span.italic == true,
                strike = span.strike == true,
                instant = span.instant == true,
                font = span.font,
                letter_spacing = span.letter_spacing or 0,
                line_height = span.line_height,
            }
        end
    end
    return characters
end

--- TextLayout.wrap_spans(spans, options) → lines of color-aware segments
--  Same wrapping algorithm as wrap() but lines carry `segments`
--  ({text, color, width}) so callers can emit per-span colored draws.
function TextLayout.wrap_spans(spans, options)
    options = options or {}
    local max_width = tonumber(options.max_width)
    assert(max_width and max_width > 0,
        "text layout max_width must be a positive number")

    local lines = {}
    local paragraph = {}
    local characters = span_characters(spans, options)
    for _, character in ipairs(characters) do
        if character.codepoint == 0x0A then
            wrap_paragraph(paragraph, options, lines, append_line_segments)
            paragraph = {}
        elseif character.codepoint ~= 0x0D then
            paragraph[#paragraph + 1] = character
        end
    end

    if #paragraph > 0 or #characters == 0
        or characters[#characters].codepoint ~= 0x0A then
        wrap_paragraph(paragraph, options, lines, append_line_segments)
    elseif characters[#characters].codepoint == 0x0A then
        lines[#lines + 1] = { text = "", width = 0, segments = {} }
    end

    return lines
end

return TextLayout
