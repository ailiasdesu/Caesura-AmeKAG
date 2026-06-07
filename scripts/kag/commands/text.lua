-- =============================================================================
--  Caesura (AmeKAG) ¡ª kag/commands/text.lua
--  Phase 4: KAG text tag handlers ¡ª [ch], [text], [l], [r], [er], [p]
--  Manages character dialog display, backlog, and text cursor state.
--  All rendering delegates to backend.font_render_text / backend.font_clear.
-- =============================================================================

local backend = require("backend")
local layers  = require("layers")

-- =============================================================================
--  Internal: update text_state for save/load position tracking
-- =============================================================================

local function update_text_state(ctx, action)
    ctx.text_state = ctx.text_state or {}
    ctx.text_state.line = ctx.text_state.line or 1
    ctx.text_state.char_offset = ctx.text_state.char_offset or 0
    ctx.text_state.last_action = action

    if action == "l" or action == "r" then
        ctx.text_state.line = (ctx.text_state.line or 1) + 1
        ctx.text_state.char_offset = 0
    elseif action == "p" or action == "er" then
        ctx.text_state.line = 1
        ctx.text_state.char_offset = 0
    elseif action == "ch" or action == "text" then
        ctx.text_state.char_offset = ctx.text_state.char_offset + 1
    end
end

local TextCommands = {}

-- =============================================================================
--  Internal: push a message entry to the ctx.backlog (spec [4.1])
-- =============================================================================

local function push_backlog(ctx, speaker, text, voiceFile)
    ctx.backlog = ctx.backlog or {}
    local entry = {
        name      = speaker or "",
        text      = text or "",
        voice     = voiceFile or "",
        time      = os.date("%H:%M:%S"),
        timestamp = os.time(),
    }
    table.insert(ctx.backlog, entry)

    -- Trim if over max
    local maxEntries = ctx.backlog_max or 500
    while #ctx.backlog > maxEntries do
        table.remove(ctx.backlog, 1)
    end
end

-- =============================================================================
--  [ch name="Hero" text="Hello, world!"]
--  Display character dialog: renders name + text on message layer,
--  appends to backlog, and blocks until click (via [p] semantics).
-- =============================================================================

function TextCommands.ch(ctx, params)
    local speaker = params.name or params.character or ""
    local message = params.text or params.message or ""

    -- Store in backlog
    push_backlog(ctx, speaker, message)

    -- Set up message layer if needed
    local msgNode = layers.find(ctx, "message")
    if not msgNode then
        msgNode = layers.add_layer(ctx, "message", layers.Type.LAYER_MESSAGE, {
            x = 0, y = 520, w = 1280, h = 200, visible = true,
        })
        layers.set_z(ctx, msgNode, 2)
    end

    -- Clear previous text, render speaker + message
    backend.clear_text()

    -- Render speaker name if present
    if #speaker > 0 then
        backend.render_text("¡¾" .. speaker .. "¡¿", 32, 48, 540, 1, 1, 1, 1)
    end

    -- Calculate y-position for message (below speaker if present)
    local msgY = 580

    -- Auto-wrap long text: split into lines of ~40 chars per line for 1280 screen
    if #message > 0 then
        local lineHeight = backend.line_height() or 24
        local charsPerLine = params.chars_per_line or 48
        local pos = 1
        while pos <= #message do
            local lineEnd = math.min(pos + charsPerLine - 1, #message)
            -- Don't break mid-character for CJK; find a natural break point
            local line = message:sub(pos, lineEnd)
            backend.render_text(line, 32, 48, msgY, 1, 1, 1, 1)
            msgY = msgY + lineHeight
            pos = lineEnd + 1
        end
    end

    -- Mark text cursor for [p] blocking
    ctx.textCursorX = 32
    ctx.textCursorY = msgY
    ctx.waiting_input = true
    update_text_state(ctx, "ch")
end

-- =============================================================================
--  [text text="Plain narration text."]
--  Display narration (no speaker name). Appends to backlog.
-- =============================================================================

function TextCommands.text(ctx, params)
    local message = params.text or params.message or params.content or ""
    if #message == 0 then return end

    push_backlog(ctx, "", message)

    backend.clear_text()

    local lineHeight = backend.line_height() or 24
    local charsPerLine = params.chars_per_line or 48
    local y = 580
    local pos = 1
    while pos <= #message do
        local lineEnd = math.min(pos + charsPerLine - 1, #message)
        local line = message:sub(pos, lineEnd)
        backend.render_text(line, 32, 48, y, 1, 1, 1, 1)
        y = y + lineHeight
        pos = lineEnd + 1
    end

    ctx.textCursorX = 32
    ctx.textCursorY = y
    ctx.waiting_input = true
    update_text_state(ctx, "text")
end

-- =============================================================================
--  [l] ¡ª line break: advance text cursor to next line
-- =============================================================================

function TextCommands.l(ctx, params)
    local lineHeight = backend.line_height() or 24
    ctx.textCursorY = (ctx.textCursorY or 600) + lineHeight
    ctx.textCursorX = 32
    update_text_state(ctx, "l")
end

-- =============================================================================
--  [r] ¡ª carriage return: reset cursor to start of current line
-- =============================================================================

function TextCommands.r(ctx, params)
    ctx.textCursorX = 32
    update_text_state(ctx, "r")
end

-- =============================================================================
--  [er] ¡ª erase: clear all text from message layer (backlog preserved)
-- =============================================================================

function TextCommands.er(ctx, params)
    backend.clear_text()
    ctx.textCursorX = 32
    ctx.textCursorY = 580
    ctx.waiting_input = false
    update_text_state(ctx, "er")
end

-- =============================================================================
--  [p] ¡ª page break / click-to-advance
--  Blocks the coroutine until user clicks or presses Enter/Space.
--  The scheduler detects ctx.waiting_input and handles resume on input.
-- =============================================================================

function TextCommands.p(ctx, params)
    -- Clear any previous text rendering state
    backend.clear_text()

    -- Set state for scheduler to detect
    ctx.waiting_input = true

    -- Yield the coroutine ¡ª scheduler resumes on next click
    coroutine.yield()
    update_text_state(ctx, "p")
end


-- =============================================================================
--  [ruby text="h×Ö" ruby="¤«¤ó¤¸"]
--  Render base text with ruby (furigana) annotation above it.
--  Delegates to backend.text_render_ruby for glyph layout.
-- =============================================================================

function TextCommands.ruby(ctx, params)
    local text = params.text or ""
    local ruby_text = params.ruby or ""
    local backend = require("backend")
    backend.render_ruby(text, ruby_text, 0, 0)
end

-- =============================================================================
--  [font face="Noto Serif" size=28 color="#333333"]
--  Set font face, size, and/or color for subsequent text rendering.
--  Only specified params are updated; existing values are preserved.
-- =============================================================================

function TextCommands.font(ctx, params)
    ctx.text_state = ctx.text_state or {}
    if params.face then ctx.text_state.font_face = params.face end
    if params.size then ctx.text_state.font_size = tonumber(params.size) end
    if params.color then ctx.text_state.font_color = params.color end
    local backend = require("backend")
    -- text_set_font stub (face/size/color not yet implemented in C++)
end

-- =============================================================================
--  [skip] ¡ª toggle skip mode
--  When active, scheduler auto-advances without waiting for user click.
-- =============================================================================

function TextCommands.skip(ctx, params)
    ctx.skip_mode = not ctx.skip_mode
end

-- =============================================================================
--  [reset] ¡ª reset text state
--  Clears line/char_offset tracking and resets backend text renderer.
--  Registered as KAG.reset via auto-iteration in kag.lua.
-- =============================================================================

function TextCommands.reset(ctx, params)
    ctx.text_state = { line = 1, char_offset = 0 }
    local backend = require("backend")
    -- text_reset_state stub
end

-- =============================================================================
--  [pt speed=50] ¡ª typewriter speed (ms per character)
--  Controls the delay between each character appearing in [ch] / [text].
-- =============================================================================

function TextCommands.pt(ctx, params)
    ctx.text_speed = tonumber(params.speed) or 50
end


return TextCommands