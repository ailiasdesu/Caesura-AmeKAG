-- Persistent, serializable text draw state for the KAG render loop.

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

function TextScene.clear(ctx)
    local state = ensure_state(ctx)
    state.draws = {}
    state.cursor_x = 32
    state.cursor_y = 580
    ctx.textCursorX = state.cursor_x
    ctx.textCursorY = state.cursor_y
end

function TextScene.reset(ctx)
    ctx.text_state = {
        line = 1,
        char_offset = 0,
        opacity = 255,
        cursor_x = 32,
        cursor_y = 580,
        draws = {},
    }
    ctx.textCursorX = 32
    ctx.textCursorY = 580
end

function TextScene.set_opacity(ctx, opacity)
    local state = ensure_state(ctx)
    state.opacity = clamp_byte(opacity)
    return state.opacity
end

function TextScene.add_text(ctx, text, x, y, color, group)
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
        typewriter = true,  -- may be truncated by the reveal animation
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
                    shown = utf8.sub(draw.text, 1, reveal)
                end
                render_backend.render_text(
                    shown, draw.x, draw.y,
                    draw.r, draw.g, draw.b, alpha)
            end
            submitted = submitted + 1
        end
    end
    return submitted
end

return TextScene
