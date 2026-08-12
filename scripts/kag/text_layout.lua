-- Pure UTF-8 text layout helpers used by KAG text commands.

local TextLayout = {}

local OPENING_PUNCTUATION = {
    [0x0028] = true, [0x005B] = true, [0x007B] = true,
    [0x2018] = true, [0x201C] = true,
    [0x3008] = true, [0x300A] = true, [0x300C] = true,
    [0x300E] = true, [0x3010] = true, [0x3014] = true,
    [0x3016] = true, [0x3018] = true, [0x301A] = true,
    [0xFF08] = true, [0xFF3B] = true, [0xFF5B] = true,
}

local CLOSING_PUNCTUATION = {
    [0x0021] = true, [0x0029] = true, [0x002C] = true,
    [0x002E] = true, [0x003A] = true, [0x003B] = true,
    [0x003F] = true, [0x005D] = true, [0x007D] = true,
    [0x2019] = true, [0x201D] = true, [0x2026] = true,
    [0x3001] = true, [0x3002] = true, [0x3009] = true,
    [0x300B] = true, [0x300D] = true, [0x300F] = true,
    [0x3011] = true, [0x3015] = true, [0x3017] = true,
    [0x3019] = true, [0x301B] = true,
    [0xFF01] = true, [0xFF09] = true, [0xFF0C] = true,
    [0xFF0E] = true, [0xFF1A] = true, [0xFF1B] = true,
    [0xFF1F] = true, [0xFF3D] = true, [0xFF5D] = true,
}

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
    return width * scale
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
-- inline style (color / scale / bold) into segments so the caller can
-- emit one draw per segment (inline text markup: {color}/{size}/{b}).
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
    local function same_style(a, b)
        return a.color == b.color and a.scale == b.scale and a.bold == b.bold
    end
    for i = first + 1, last + 1 do
        local c = characters[i]
        if i > last or not same_style(c, characters[i - 1]) then
            segments[#segments + 1] = {
                text = join_range(characters, seg_first, i - 1),
                color = color,
                scale = scale,
                bold = bold,
                width = measure_range(characters, seg_first, i - 1, options),
            }
            if i > last then break end
            seg_first = i
            color = characters[i].color
            scale = characters[i].scale
            bold = characters[i].bold
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
        break_at = break_at or last_fit

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
--   {b}/{/b}, {i}/{/i}, {size=N}/{/size} — parsed and consumed for source
--     compatibility; the renderer has no bold/italic/size variants yet, so
--     they are a visual no-op. Unknown {tags} pass through as literal text.
-- ---------------------------------------------------------------------------

local function parse_open_tag(tag)
    local name = tag:match("^(%a+)")
    if not name then return nil end
    if name == "i" then return { kind = "noop" } end -- italic: no shear path yet
    if name == "b" then return { kind = "bold" } end
    if name == "size" then
        local size = tonumber(tag:match("^size%s*=%s*(%d+)%s*$"))
        if size then return { kind = "size", size = size } end
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

local MARKUP_CLOSE_NAMES = { color = true, b = true, i = true, size = true }

--- TextLayout.parse_markup(text) → { spans = {{text, color, size, bold}}, plain }
--  Splits a message into styled spans and returns the markup-stripped
--  plain text (backlog / reveal counters use the visible characters only).
--  size is an absolute font size (px); bold is a boolean. {i} is parsed
--  and consumed (renderer has no shear path yet — documented limitation).
function TextLayout.parse_markup(text)
    text = tostring(text or "")
    local spans = {}
    local plain_parts = {}
    local color_stack = {}
    local size_stack = {}
    local bold_stack = {}
    local current_color = nil
    local current_size = nil
    local current_bold = false
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
                    end
                    i = close_at + 1
                else
                    local close_name = tag:match("^%s*/(%a+)%s*$")
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
                        elseif close_name == "i" then
                            -- consumed; no visual effect (documented)
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
        spans = { { text = text, color = nil, size = nil, bold = false } }
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
                scale = scale,
                bold = span.bold == true,
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
