-- Persistent, serializable text draw state for the KAG render loop.

-- The standalone test Lua (external/lua) ships a REDUCED utf8 lib
-- (len/codes/char, no sub). The engine's embedded Lua has the full
-- 5.4 lib. This fallback keeps text_scene testable in both.
local utf8_sub
if type(utf8) == "table" and utf8.sub then
    utf8_sub = utf8.sub
else
    utf8_sub = function(s, i, j)
        local out, n = {}, 0
        for _, cp in utf8.codes(s) do
            n = n + 1
            if n >= i and (j == nil or n <= j) then
                out[#out + 1] = utf8.char(cp)
            end
            if j and n > j then break end
        end
        return table.concat(out)
    end
end

local TextLayout = require("kag.text_layout")

local TextScene = {}

local function clamp_byte(value)
    value = tonumber(value) or 0
    return math.max(0, math.min(255, value))
end

local function ensure_state(ctx)
    assert(type(ctx) == "table", "text scene context must be a table")
    ctx.text_state = ctx.text_state or {}
    local state = ctx.text_state
    state.draws = type(state.draws) == "table" and state.draws or {}
    -- Per-line pre-localize sources for language hot-switch redraw
    -- (see kag/commands/text.lua relocalize_page). Parallel to draws,
    -- cleared wherever draws are cleared.
    state.page_src = type(state.page_src) == "table" and state.page_src or {}
    state.opacity = clamp_byte(state.opacity == nil and 255 or state.opacity)
    state.cursor_x = tonumber(state.cursor_x) or tonumber(ctx.textCursorX) or 32
    state.cursor_y = tonumber(state.cursor_y) or tonumber(ctx.textCursorY) or 580
    return state
end

local function color_value(color, name, index, fallback)
    if type(color) ~= "table" then return fallback end
    local value = color[name]
    if value == nil then value = color[index] end
    if value == nil then value = fallback end
    return clamp_byte(value)
end

function TextScene.get_state(ctx)
    return ensure_state(ctx)
end

function TextScene.reveal_length(ctx)
    local total=0
    for _,draw in ipairs(ensure_state(ctx).draws) do
        if draw.typewriter then total=total+(utf8.len(draw.text) or #draw.text) end
    end
    return total
end

function TextScene.clear(ctx)
    local state = ensure_state(ctx)
    state.draws = {}
    state.page_src = {}
    state.cursor_x = 32
    state.cursor_y = 580
    ctx.textCursorX = state.cursor_x
    ctx.textCursorY = state.cursor_y
end

--- TextScene.commit(ctx) — seal the current page for NVL accumulation.
--  Marks every typewriter draw as already-revealed so a subsequent
--  [ch]/[text] typewriter reveal animates ONLY the newly appended line,
--  never re-truncating earlier lines (render() slices all typewriter draws
--  against the single global reveal_chars counter).
function TextScene.commit(ctx)
    local state = ensure_state(ctx)
    for _, draw in ipairs(state.draws) do
        if draw.typewriter then
            draw.typewriter = false
            draw._shown = nil      -- drop the reveal slice cache
            draw._shown_len = nil
        end
    end
    return state
end

function TextScene.reset(ctx)
    ctx.text_state = {
        line = 1,
        char_offset = 0,
        opacity = 255,
        cursor_x = 32,
        cursor_y = 580,
        draws = {},
        page_src = {},
    }
    ctx.textCursorX = 32
    ctx.textCursorY = 580
end

function TextScene.set_opacity(ctx, opacity)
    local state = ensure_state(ctx)
    state.opacity = clamp_byte(opacity)
    return state.opacity
end

function TextScene.add_text(ctx, text, x, y, color, group, scale, bold, italic, instant, strike)
    local state = ensure_state(ctx)
    local draw = {
        kind = "text",
        text = tostring(text or ""),
        x = tonumber(x) or 0,
        y = tonumber(y) or 0,
        r = color_value(color, "r", 1, 255),
        g = color_value(color, "g", 2, 255),
        b = color_value(color, "b", 3, 255),
        a = color_value(color, "a", 4, 255),
        group = group,
        scale = scale or 1,    -- {size=N} markup (glyph scale factor)
        bold = bold == true,   -- {b} markup (synthetic bold)
        italic = italic == true, -- {i} markup (top-edge shear)
        strike = strike == true, -- {s} markup (middle bar)
        typewriter = not (instant == true),  -- instant: shown immediately
    }
    state.draws[#state.draws + 1] = draw
    return draw
end

function TextScene.add_wrapped(ctx, text, options)
    options = options or {}
    local x = tonumber(options.x) or 32
    local y = tonumber(options.y) or 580
    local line_height = tonumber(options.line_height) or 24
    local lines = TextLayout.wrap(text, options)
    for _, line in ipairs(lines) do
        TextScene.add_text(
            ctx, line.text, x, y, options.color, options.group)
        y = y + line_height
    end

    local state = ensure_state(ctx)
    state.cursor_x = x
    state.cursor_y = y
    ctx.textCursorX = x
    ctx.textCursorY = y
    return y, lines
end

--- TextScene.add_wrapped_spans(ctx, spans, options) — like add_wrapped but
--  for parsed inline-markup spans: each line is emitted as per-segment draws
--  ({text, color, width}) so {color=#rrggbb} spans render with their own
--  color. Draw order (line → segment) keeps the typewriter reveal correct.
function TextScene.add_wrapped_spans(ctx, spans, options)
    options = options or {}
    local x = tonumber(options.x) or 32
    local y = tonumber(options.y) or 580
    local line_height = tonumber(options.line_height) or 24
    local lines = TextLayout.wrap_spans(spans, options)
    for _, line in ipairs(lines) do
        local seg_x = x
        for _, seg in ipairs(line.segments) do
            local spacing = tonumber(seg.letter_spacing)
            if spacing and spacing ~= 0 then
                -- t105 visual letter-spacing: the engine's render_text has no
                -- per-glyph spacing parameter, so a spaced segment must be
                -- emitted one character per draw. x advances by the SAME
                -- per-character advance the layout already used
                -- (measure_character: rawWidth * scale + spacing, clamped >= 0;
                -- scale/spacing are uniform per segment because same_style()
                -- groups by both). UTF-8 code-point iteration keeps multibyte
                -- text (CJK) on character boundaries -- never byte slices.
                local scale = seg.scale or 1
                local char_x = seg_x
                for _, cp in utf8.codes(seg.text) do
                    local ch = utf8.char(cp)
                    TextScene.add_text(
                        ctx, ch, char_x, y,
                        seg.color or options.color, options.group,
                        scale, seg.bold == true, seg.italic == true,
                        seg.instant == true, seg.strike == true)
                    local raw = tonumber(TextLayout.measure(ch, options)) or 0
                    char_x = char_x + math.max(0, (raw * scale) + spacing)
                end
            else
                TextScene.add_text(
                    ctx, seg.text, seg_x, y,
                    seg.color or options.color, options.group,
                    seg.scale or 1, seg.bold == true, seg.italic == true,
                    seg.instant == true, seg.strike == true)
            end
            seg_x = seg_x + seg.width
        end
        y = y + line_height
    end

    local state = ensure_state(ctx)
    state.cursor_x = x
    state.cursor_y = y
    ctx.textCursorX = x
    ctx.textCursorY = y
    return y, lines
end

function TextScene.add_ruby(ctx, base_text, ruby_text, options)
    options = options or {}
    local state = ensure_state(ctx)
    local start_x = tonumber(options.start_x) or 32
    local x = tonumber(options.x) or state.cursor_x
    local y = tonumber(options.y) or state.cursor_y
    local max_width = tonumber(options.max_width) or 1216
    local line_height = tonumber(options.line_height) or 24
    local metrics = TextLayout.measure_ruby(base_text, ruby_text, options)

    if x > start_x and x + metrics.width > start_x + max_width then
        x = start_x
        y = y + line_height
    end

    local color = options.color
    local draw = {
        kind = "ruby",
        text = tostring(base_text or ""),
        ruby = tostring(ruby_text or ""),
        x = x + metrics.base_offset,
        y = y,
        r = color_value(color, "r", 1, 255),
        g = color_value(color, "g", 2, 255),
        b = color_value(color, "b", 3, 255),
        a = color_value(color, "a", 4, 255),
        group = options.group,
        layout_width = metrics.width,
    }
    state.draws[#state.draws + 1] = draw
    state.cursor_x = x + metrics.width
    state.cursor_y = y
    ctx.textCursorX = state.cursor_x
    ctx.textCursorY = state.cursor_y
    return draw, metrics
end

function TextScene.remove_group(ctx, group)
    local state = ensure_state(ctx)
    local kept = {}
    for _, draw in ipairs(state.draws) do
        if draw.group ~= group then
            kept[#kept + 1] = draw
        end
    end
    state.draws = kept
end

function TextScene.render(ctx, render_backend)
    local state = ensure_state(ctx)
    render_backend = render_backend or require("backend")
    local submitted = 0

    -- Typewriter reveal: if the current message is animating, truncate each
    -- text draw to the visible character count (state.reveal_chars > 0).
    -- reveal_chars == 0 must also truncate (animation not started); using
    -- "~= nil" avoids flashing the full line for one frame at reveal start.
    local reveal = state.reveal_chars

    -- Multi-line typewriter: reveal is a GLOBAL char count across the
    -- wrapped lines -- each typewriter draw consumes its own length, so
    -- line N truncates at reveal - (sum of previous line lengths). The
    -- old code sliced every line with the global reveal, over-showing
    -- later lines by the earlier lines' length (audit: two 5-char lines
    -- at reveal=7 showed 7 chars on line 2 instead of 2).
    local consumed = 0

    for _, draw in ipairs(state.draws) do
        local alpha = math.floor(
            clamp_byte(draw.a) * state.opacity / 255 + 0.5)
        if alpha > 0 then
            if draw.kind == "ruby" then
                render_backend.render_ruby(
                    draw.text, draw.ruby, draw.x, draw.y,
                    draw.r, draw.g, draw.b, alpha)
            else
                local shown = draw.text
                if reveal ~= nil and draw.typewriter then
                    local line_len = utf8.len(draw.text) or #draw.text
                    local visible = math.max(0,
                        math.min(line_len, reveal - consumed))
                    -- Cache per-draw: re-slice only when the position moved
                    if draw._shown_len ~= visible then
                        draw._shown = utf8_sub(draw.text, 1, visible)
                        draw._shown_len = visible
                    end
                    shown = draw._shown
                end
                render_backend.render_text(
                    shown, draw.x, draw.y,
                    draw.r, draw.g, draw.b, alpha,
                    draw.scale or 1, draw.bold == true,
                    draw.italic == true, draw.strike == true)
            end
            submitted = submitted + 1
        end
        -- consumed advances OUTSIDE the alpha guard (review blocking:
        -- inside the guard, alpha=0 frames skipped the whole body and
        -- the next line over-showed during fades)
        if draw.typewriter then
            consumed = consumed + (utf8.len(draw.text) or #draw.text)
        end
    end
    return submitted
end

local function saved_number(value, name, minimum, maximum)
    if type(value) ~= "number" or value ~= value or math.abs(value) == math.huge
        or (minimum and value < minimum) or (maximum and value > maximum) then
        error("Invalid saved text " .. name, 0)
    end
    return value
end

local function saved_array(value, name)
    if value == nil then return {} end
    if type(value) ~= "table" then error("Invalid saved text " .. name, 0) end
    local count = 0
    for key in pairs(value) do
        if type(key) ~= "number" or key < 1 or key ~= math.floor(key) then
            error("Invalid saved text array index", 0)
        end
        count = count + 1
    end
    if count ~= #value or count > 4096 then error("Invalid saved text array size", 0) end
    return value
end

local function saved_count(value, name)
    local result=saved_number(value,name,0)
    if result~=math.floor(result) then error("Invalid saved text "..name,0) end
    return result
end

local function saved_page_options(value)
    if value==nil then return nil end
    if type(value)~="table" then error("Invalid saved page options",0) end
    local opts=require("kag.save_state").copy(value)
    for _,key in ipairs({"msgX","msgY","nameX","lineHeight","maxWidth","font_size"}) do
        if opts[key]~=nil then
            saved_number(opts[key],key)
            if (key=="lineHeight" or key=="maxWidth" or key=="font_size") and opts[key]<=0 then
                error("Invalid saved page "..key,0)
            end
        end
    end
    if opts.nvl~=nil and type(opts.nvl)~="boolean" then error("Invalid saved page NVL flag",0) end
    if opts.pos~=nil and type(opts.pos)~="string" then error("Invalid saved page position",0) end
    if opts.color~=nil then
        if type(opts.color)~="table" then error("Invalid saved page color",0) end
        for _,key in ipairs({1,2,3,4,"r","g","b","a"}) do
            if opts.color[key]~=nil then saved_number(opts.color[key],"page color",0,255) end
        end
    end
    return opts
end

local function saved_draw(source)
    if type(source) ~= "table" or (source.kind ~= "text" and source.kind ~= "ruby")
        or type(source.text) ~= "string" or not utf8.len(source.text) then
        error("Invalid saved text draw", 0)
    end
    local draw = {kind=source.kind,text=source.text}
    for _, key in ipairs({"x","y"}) do draw[key]=saved_number(source[key],key) end
    for _, key in ipairs({"r","g","b","a"}) do draw[key]=saved_number(source[key],key,0,255) end
    for _, key in ipairs({"scale","layout_width"}) do
        if source[key] ~= nil then draw[key]=saved_number(source[key],key,0) end
    end
    for _, key in ipairs({"bold","italic","strike","typewriter","_page_src"}) do
        if source[key] ~= nil then
            if type(source[key]) ~= "boolean" then error("Invalid saved text flag",0) end
            draw[key]=source[key]
        end
    end
    if source.kind == "ruby" then
        if type(source.ruby) ~= "string" or not utf8.len(source.ruby) then
            error("Invalid saved ruby text",0)
        end
        draw.ruby=source.ruby
    end
    if source.group ~= nil then
        if type(source.group) ~= "string" and type(source.group) ~= "number" then
            error("Unrepresentable saved text group",0)
        end
        draw.group=source.group
        if type(draw.group)=="number" then saved_number(draw.group,"group") end
    end
    return draw
end

-- Normalize a plain snapshot before changing either the live context or font.
function TextScene.prepare_restore(snapshot)
    local copy = require("kag.save_state").copy
    if type(snapshot) ~= "table" or type(snapshot.state) ~= "table" then
        error("Invalid text snapshot",0)
    end
    local source, state = snapshot.state, {draws={},page_src={}}
    for _, key in ipairs({"cursor_x","cursor_y"}) do
        if source[key] ~= nil then state[key]=saved_number(source[key],key) end
    end
    for _, key in ipairs({"line","char_offset","reveal_chars"}) do
        if source[key] ~= nil then state[key]=saved_count(source[key],key) end
    end
    if source.font_size~=nil then state.font_size=saved_number(source.font_size,"font_size",0) end
    if source.opacity ~= nil then state.opacity=saved_number(source.opacity,"opacity",0,255) end
    for _, key in ipairs({"font_face","font_color","last_action"}) do
        if source[key] ~= nil then
            if type(source[key]) ~= "string" then error("Invalid saved text "..key,0) end
            state[key]=source[key]
        end
    end
    for i, draw in ipairs(saved_array(source.draws,"draws")) do state.draws[i]=saved_draw(draw) end
    for i, entry in ipairs(saved_array(source.page_src,"page sources")) do
        if type(entry) ~= "table" or type(entry.src) ~= "string"
            or not utf8.len(entry.src) or (entry.kind ~= "ch" and entry.kind ~= "text") then
            error("Invalid saved page source",0)
        end
        for _,key in ipairs({"scene","speaker"}) do
            if entry[key]~=nil and (type(entry[key])~="string" or not utf8.len(entry[key])) then
                error("Invalid saved page "..key,0)
            end
        end
        state.page_src[i]=copy(entry)
        state.page_src[i].opts=saved_page_options(entry.opts)
    end
    if snapshot.waiting_input~=nil and type(snapshot.waiting_input)~="boolean" then
        error("Invalid saved input wait",0)
    end
    local result={state=state,waiting_input=snapshot.waiting_input==true}
    for _, key in ipairs({"textCursorX","textCursorY"}) do
        if snapshot[key] ~= nil then result[key]=saved_number(snapshot[key],key) end
    end
    result.textCursorX = result.textCursorX or state.cursor_x or 32
    result.textCursorY = result.textCursorY or state.cursor_y or 580
    if snapshot.text_speed ~= nil then result.text_speed=saved_number(snapshot.text_speed,"speed",0) end
    if snapshot.reveal ~= nil then
        if type(snapshot.reveal) ~= "table" then error("Invalid saved text reveal",0) end
        local reveal = snapshot.reveal
        result.reveal = {
            total=saved_count(reveal.total,"reveal total"),
            elapsed=saved_number(reveal.elapsed,"reveal elapsed",0),
            last_shown=saved_count(reveal.last_shown==nil and 0 or reveal.last_shown,"last shown"),
        }
        if result.reveal.last_shown > result.reveal.total then error("Invalid saved reveal boundary",0) end
    end
    return result
end

function TextScene.capture(ctx)
    return TextScene.prepare_restore({state=ctx.text_state or {},reveal=ctx.reveal,
        textCursorX=ctx.textCursorX,textCursorY=ctx.textCursorY,
        text_speed=ctx.text_speed,waiting_input=ctx.waiting_input})
end

function TextScene.apply_restore(ctx, snapshot)
    ctx.text_state, ctx.reveal = snapshot.state, snapshot.reveal
    ctx.textCursorX = snapshot.textCursorX or snapshot.state.cursor_x or 32
    ctx.textCursorY = snapshot.textCursorY or snapshot.state.cursor_y or 580
    ctx.text_speed, ctx.waiting_input = snapshot.text_speed, snapshot.waiting_input
end

return TextScene
