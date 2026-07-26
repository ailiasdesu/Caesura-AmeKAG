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
    return math.max(width, 0)
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

local function wrap_paragraph(characters, options, lines)
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
            append_line(lines, characters, first, #characters, options)
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

        append_line(lines, characters, first, break_at, options)
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

return TextLayout
