-- =============================================================================
--  Caesura (AmeKAG) ?? kag/commands/text.lua
--  Phase 4: KAG text tag handlers ?? [ch], [text], [l], [r], [er], [p]
--  Manages character dialog display, backlog, and text cursor state.
--  All rendering delegates to backend.font_render_text / backend.font_clear.
-- =============================================================================

local backend = require("backend")
local layers  = require("layers")
local Operation = require("kag.operation")
local TextScene = require("kag.text_scene")

-- =============================================================================
--  Internal: update text_state for save/load position tracking
-- =============================================================================

local function update_text_state(ctx, action, char_count)
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
        ctx.text_state.char_offset =
            ctx.text_state.char_offset + (tonumber(char_count) or 1)
    end
end

local function clamp_byte(value)
    value = tonumber(value) or 0
    return math.max(0, math.min(255, value))
end

local function parse_hex_color(value)
    if type(value) ~= "string" then return nil end
    local hex = value:match("^#?(%x%x%x%x%x%x)$")
    if not hex then return nil end
    return {
        r = tonumber(hex:sub(1, 2), 16),
        g = tonumber(hex:sub(3, 4), 16),
        b = tonumber(hex:sub(5, 6), 16),
        a = 255,
    }
end

local function resolve_color(ctx, params)
    local state = TextScene.get_state(ctx)
    local color = parse_hex_color(params.color)
        or parse_hex_color(state.font_color)
        or { r = 255, g = 255, b = 255, a = 255 }
    color.r = clamp_byte(params.r or color.r)
    color.g = clamp_byte(params.g or color.g)
    color.b = clamp_byte(params.b or color.b)
    color.a = clamp_byte(params.a or color.a)
    return color
end

local function resolve_line_height(ctx)
    local state = TextScene.get_state(ctx)
    -- line_height() may return extra values; wrap in parens to keep only
    -- the first, otherwise tonumber gets >1 args and errors.
    local line_height = tonumber((backend.line_height()))
        or tonumber(state.font_size)
        or 24
    if line_height <= 0 then return 24 end
    return line_height
end

local function resolve_max_width(ctx, params, x)
    local explicit = params.max_width
    if explicit and explicit > 0 then return explicit end

    local chars_per_line = params.chars_per_line
    if chars_per_line and chars_per_line > 0 then
        local state = TextScene.get_state(ctx)
        return chars_per_line * (tonumber(state.font_size) or 24)
    end
    return math.max(1, 1280 - x - 48)
end

local function animate_text_opacity(ctx, params)
    local duration = params.fade_time or params.fade or 0
    local target = clamp_byte(params.opacity or params.fade_to or 255)
    if duration <= 0 then
        TextScene.set_opacity(ctx, target)
        return
    end

    local from = clamp_byte(params.fade_from or 0)
    TextScene.set_opacity(ctx, from)

    local operation <close> = Operation.start(ctx)
    local elapsed = 0
    while elapsed < duration and not operation.token.cancelled do
        local delta_ms = tonumber(coroutine.yield()) or 16
        if operation.token.cancelled then break end
        elapsed = math.min(duration, elapsed + math.max(delta_ms, 0))
        local progress = elapsed / duration
        TextScene.set_opacity(ctx, from + (target - from) * progress)
    end

    if not operation.token.cancelled then
        TextScene.set_opacity(ctx, target)
        operation:complete()
    end
end

local TextCommands = {}

-- =============================================================================
--  Internal: push a message entry to the ctx.backlog (spec [4.1])
--  [R5-FIX] Exported for system.lua delegation
-- =============================================================================

-- Next-gen contracts: typed + clamped via kag/schema.
local schema = require("kag.schema")
schema.define("ch", {
    name   = { type = "string", default = "" },
    text   = { type = "string", default = "", interpolate = true },
    voice  = { type = "string", default = "" },
    sprite = { type = "string", default = "" },
    max_width = { type = "number", default = 0, min = 0, max = 4096 },
    chars_per_line = { type = "number", default = 0, min = 0, max = 512 },
})
schema.define("text", {
    text = { type = "string", default = "", interpolate = true },
    fade_time = { type = "number", default = 0, min = 0, max = 30000 },
    fade = { type = "number", default = 0, min = 0, max = 30000 },
})
schema.define("ruby", {
    text = { type = "string", default = "" },
    ruby = { type = "string", default = "" },
    x = { type = "number", default = 0 },
    y = { type = "number", default = 0 },
    ruby_scale = { type = "number", default = 0.5, min = 0.1, max = 2.0 },
})
schema.define("font", {
    face = { type = "string", default = "default" },
    size = { type = "number", default = 22, min = 4, max = 256 },
    color = { type = "string", default = "white" },  -- KAG3 color param
})

function TextCommands.push_backlog(ctx, speaker, text, voiceFile)
    ctx.backlog = ctx.backlog or {}
    local entry = {
        name        = speaker or "",
        text        = text or "",
        voice       = voiceFile or "",
        time        = os.date("%H:%M:%S"),
        timestamp   = os.time(),
        scene       = ctx.current_scene or ctx.currentScene or "",
        token_index = ctx.token_index or 1,
    }
    table.insert(ctx.backlog, entry)

    -- Trim if over max
    local maxEntries = ctx.backlog_max or 500
    while #ctx.backlog > maxEntries do
        table.remove(ctx.backlog, 1)
    end

    -- [R7-FIX] Seen-marking moved to the click handler (kag_runner.on_click):
    -- marking here made every line "seen" the moment it was displayed, so
    -- read-skip could never distinguish unread text.
end

-- =============================================================================
--  [ch name="Hero" text="Hello, world!"]
--  Display character dialog: renders name + text on message layer,
--  appends to backlog, and blocks until click (via [p] semantics).
-- =============================================================================

function TextCommands.ch(ctx, params)
    local speaker = params.name or params.character or ""
    local message = params.text or params.message or ""

    -- U1.3: pos parameter (left/center/right) for text alignment
    local pos = params.pos or "center"
    if pos ~= "left" and pos ~= "center" and pos ~= "right" then
        pos = "center"
    end

    -- U1.3: ctx.characters registry -- track active on-screen characters
    ctx.characters = ctx.characters or {}
    if #speaker > 0 then
        if not ctx.characters[speaker] then
            ctx.characters[speaker] = { pos = pos }
        else
            -- Inherit stored position unless explicitly overridden
            if params.pos then
                ctx.characters[speaker].pos = pos
            else
                pos = ctx.characters[speaker].pos or "center"
            end
        end
        if params.layer then
            ctx.characters[speaker].layer = params.layer
        end
        if params.storage or params.file then
            ctx.characters[speaker].sprite = params.storage or params.file
        end
    end

    -- Voice: [ch voice="assets/voice/x.wav"] plays the line and stores the
    -- file in the backlog entry so the history overlay's V key can replay it.
    local voiceFile = params.voice or params.voicefile or ""
    if #voiceFile > 0 then
        -- Non-blocking voice: playvoice() yields until the clip ends, which
        -- would stall the script (and swallow clicks) for the whole line.
        -- Fire-and-forget matches VN behavior: the line's voice plays while
        -- the text is on screen; clicking continues immediately.
        pcall(function()
            local audio = require("kag.commands.audio")
            local file = voiceFile
            -- resolve_file lives on the audio module; reuse its resolution
            -- via a direct play with the raw path when valid.
            backend.audio_play("voice", file, {})
        end)
    end

    -- Character sprite (KAG-style standing portrait): if the speaker has a
    -- registered sprite (via [ch sprite=] or a previous [ch storage=]), show
    -- it on a dedicated layer, positioned by the speaker's registered pos.
    if #speaker > 0 and ctx.characters and ctx.characters[speaker]
       and ctx.characters[speaker].sprite then
        local sprite = ctx.characters[speaker].sprite
        local spritePos = ctx.characters[speaker].pos or "center"
        local charLayerName = "_char_" .. speaker
        local node = layers.get(charLayerName)
        if not node then
            node = layers.add_layer(nil, {
                name = charLayerName, layer_type = 0,
                x = 0, y = 200, w = 400, h = 520, visible = true,
            })
            layers.set_z(node, 1)
        end
        if spritePos == "left" then
            node.x, node.y = 40, 200
        elseif spritePos == "right" then
            node.x, node.y = 840, 200
        else
            node.x, node.y = 440, 200
        end
        node.texture = backend.load_texture(sprite)
        node.visible = true
    end

    -- Store in backlog
    TextCommands.push_backlog(ctx, speaker, message, voiceFile)

    -- Set up message layer if needed
    local msgNode = layers.get("message")
    if not msgNode then
        msgNode = layers.add_layer(nil, {
            name = "message",
            layer_type = layers.Type.LAYER_MESSAGE,
            x = 0, y = 520, w = 1280, h = 200, visible = true,
        })
        layers.set_z(msgNode, 2)
    end

    -- Replace the persistent message draw list. The render loop submits it
    -- every frame, so text remains visible after the command returns.
    backend.clear_text()
    TextScene.clear(ctx)

    -- Calculate X positions based on "pos"
    local nameX, msgX
    if pos == "left" then
        nameX = 48
        msgX  = 48
    elseif pos == "right" then
        nameX = 1032
        msgX  = 784    -- right-side text starts earlier for readability
    else  -- center (default)
        nameX = 540   -- speaker name centered
        msgX  = 48     -- dialogue text left-aligned (standard galgame convention)
    end

    local color = resolve_color(ctx, params)
    local lineHeight = resolve_line_height(ctx)

    -- Render speaker name if present
    if #speaker > 0 then
        TextScene.add_text(
            ctx, "[" .. speaker .. "]", nameX, 540, color)
    end

    -- Calculate y-position for message (below speaker if present)
    local msgY = 580

    if #message > 0 then
        msgY = TextScene.add_wrapped(ctx, message, {
            x = msgX,
            y = msgY,
            max_width = resolve_max_width(ctx, params, msgX),
            line_height = lineHeight,
            font_size = tonumber(TextScene.get_state(ctx).font_size)
                or lineHeight,
            color = color,
        })
    end

    ctx.textCursorX = msgX
    ctx.textCursorY = msgY
    animate_text_opacity(ctx, params)
    ctx.waiting_input = true
    update_text_state(ctx, "ch", utf8.len(message) or #message)
    -- Typewriter reveal: animate chars in over text_speed ms/char.
    ctx.reveal = { total = utf8.len(message) or #message, elapsed = 0 }
    TextScene.get_state(ctx).reveal_chars = 0
end

-- =============================================================================
--  [text text="Plain narration text."]
--  Display narration (no speaker name). Appends to backlog.
-- =============================================================================

function TextCommands.text(ctx, params)
    local message = params.text or params.message or params.content or ""
    if #message == 0 then return end

    TextCommands.push_backlog(ctx, "", message)

    backend.clear_text()
    TextScene.clear(ctx)

    local lineHeight = resolve_line_height(ctx)
    local y = TextScene.add_wrapped(ctx, message, {
        x = 32,
        y = 580,
        max_width = resolve_max_width(ctx, params, 32),
        line_height = lineHeight,
        font_size = tonumber(TextScene.get_state(ctx).font_size)
            or lineHeight,
        color = resolve_color(ctx, params),
    })

    ctx.textCursorX = 32
    ctx.textCursorY = y
    animate_text_opacity(ctx, params)
    ctx.waiting_input = true
    update_text_state(ctx, "text", utf8.len(message) or #message)
    ctx.reveal = { total = utf8.len(message) or #message, elapsed = 0 }
    TextScene.get_state(ctx).reveal_chars = 0
end

-- =============================================================================
--  [l] ?? line break: advance text cursor to next line
-- =============================================================================

function TextCommands.l(ctx, params)
    local lineHeight = resolve_line_height(ctx)
    ctx.textCursorY = (ctx.textCursorY or 600) + lineHeight
    ctx.textCursorX = 32
    local state = TextScene.get_state(ctx)
    state.cursor_x = ctx.textCursorX
    state.cursor_y = ctx.textCursorY
    update_text_state(ctx, "l")
end

-- =============================================================================
--  [r] ?? carriage return: reset cursor to start of current line
-- =============================================================================

function TextCommands.r(ctx, params)
    ctx.textCursorX = 32
    TextScene.get_state(ctx).cursor_x = ctx.textCursorX
    update_text_state(ctx, "r")
end

-- =============================================================================
--  [er] ?? erase: clear all text from message layer (backlog preserved)
-- =============================================================================

function TextCommands.er(ctx, params)
    backend.clear_text()
    TextScene.clear(ctx)
    ctx.waiting_input = false
    update_text_state(ctx, "er")
end

-- =============================================================================
--  [p] ?? page break / click-to-advance
--  Blocks the coroutine until user clicks or presses Enter/Space.
--  The scheduler detects ctx.waiting_input and handles resume on input.
-- =============================================================================

function TextCommands.p(ctx, params)
    ctx.waiting_input = true

    -- Keep the current page visible while waiting, then clear it only after
    -- the scheduler resumes this coroutine for the accepted click.
    coroutine.yield()
    backend.clear_text()
    TextScene.clear(ctx)
    update_text_state(ctx, "p")
end


-- =============================================================================
--  [ruby text="?h??" ruby="????"]
--  Render base text with ruby (furigana) annotation above it.
--  Delegates to backend.text_render_ruby for glyph layout.
-- =============================================================================

function TextCommands.ruby(ctx, params)
    local text = params.text or ""
    local ruby_text = params.ruby or ""
    if text == "" then return end

    local lineHeight = resolve_line_height(ctx)
    local startX = params.start_x or 32
    TextScene.add_ruby(ctx, text, ruby_text, {
        x = params.x,
        y = params.y,
        start_x = startX,
        max_width = resolve_max_width(ctx, params, startX),
        line_height = lineHeight,
        font_size = tonumber(TextScene.get_state(ctx).font_size)
            or lineHeight,
        ruby_scale = params.ruby_scale or 0.5,
        color = resolve_color(ctx, params),
    })
end

-- =============================================================================
--  [font face="Noto Serif" size=28 color="#333333"]
--  Set font face, size, and/or color for subsequent text rendering.
--  Only specified params are updated; existing values are preserved.
-- =============================================================================

function TextCommands.font(ctx, params)
    ctx.text_state = ctx.text_state or {}
    if params.face then ctx.text_state.font_face = params.face end
    if params.size then ctx.text_state.font_size = params.size end
    if params.color then ctx.text_state.font_color = params.color end
    local backend = require("backend")
    backend.text_set_font(ctx.text_state.font_face, ctx.text_state.font_size, ctx.text_state.font_color)
end

-- =============================================================================
--  [skip] ?? toggle skip mode
--  When active, scheduler auto-advances without waiting for user click.
-- =============================================================================

function TextCommands.skip(ctx, params)
    local mode = params.mode
    if mode == "seen" then
        -- Skip only already-seen text ([skip mode=seen]); plain [skip] toggles
        -- the all-mode on/off.
        ctx.skip_mode = (ctx.skip_mode == "seen") and false or "seen"
    else
        ctx.skip_mode = not ctx.skip_mode
    end
end

-- =============================================================================
--  [auto] ?? toggle auto-advance mode
--  In auto mode the runner advances past click-waits ([p]) after ~1.5s,
--  like a visual-novel auto-play button. State is persisted by [save].
-- =============================================================================

-- Next-gen: explicit mode param (on/off/toggle) beyond KAG3's bare toggle.
schema.define("auto", {
    mode = { type = "string", choices = { ["on"] = true, ["off"] = true, ["toggle"] = true } },
})

function TextCommands.auto(ctx, params)
    local m = params.mode or "toggle"
    if m == "on" then ctx.auto_mode = true
    elseif m == "off" then ctx.auto_mode = false
    else ctx.auto_mode = not ctx.auto_mode end
end

-- [voice_off] -- mute/unmute voice without stopping the engine bus
-- (next-gen convenience: KAG3 needed stopvoice + a saved setting to mute).
schema.define("voice_off", {
    on = { type = "boolean", default = true },
})

function TextCommands.voice_off(ctx, params)
    ctx.voice_muted = params.on ~= false
end

-- =============================================================================
--  [reset] ?? reset text state
--  Clears line/char_offset tracking and resets backend text renderer.
--  Registered as KAG.reset via auto-iteration in kag.lua.
-- =============================================================================

function TextCommands.reset(ctx, params)
    TextScene.reset(ctx)
    backend.text_reset_state()
    ctx.reveal = nil
end

-- =============================================================================
--  [pt speed=50] ?? typewriter speed (ms per character)
--  Controls the delay between each character appearing in [ch] / [text].
-- =============================================================================

-- Next-gen contract: typed + clamped (replaces the inline clamps).
require("kag.schema").define("pt", {
    speed = { type = "number", default = 50, min = 8, max = 5000 },
})

function TextCommands.pt(ctx, params)
    ctx.text_speed = params.speed
end



-- =============================================================================
--  [button text="Choice 1" target="*label_a"]
--  [button text="Choice 2" target="*label_b"]
--  [endbutton]
--  Interactive choice buttons. Blocks coroutine until user selects.
--  Each [button] renders a clickable choice. [endbutton] executes the block.
--  On selection, jumps to the target label within current scene.
--
--[[
[R4-FIX] Choice/Branch System Design Note:
The choice system is implemented purely in Lua (no C++ ChoiceController class).
This is intentional - the visual novel choice system is a UI concern that benefits
from Lua's flexibility for layout, styling, and animation.
Architecture:
  1. [button text="..." target="*label"] registers choices into ctx._choiceButtons[]
  2. [endbutton] renders all buttons, blocks the coroutine via coroutine.yield(),
     and jumps to ctx._selectedChoice.target on resume
  3. Intermediate state fields: ctx._choiceButtons (staging), ctx._choiceButtonsActive (active),
     ctx._choiceMode (bool), ctx._selectedChoice (result)
  4. _KAG_onClick is temporarily overridden for hit-testing during choice mode
Future enhancement: extract to a standalone ChoiceController Lua class if complexity grows.
--]]
-- =============================================================================

function TextCommands.button(ctx, params)
    ctx._choiceButtons = ctx._choiceButtons or {}
    local text = params.text or params.caption or ""
    local target = params.target or params.storage or ""
    table.insert(ctx._choiceButtons, { text = text, target = target })
end

function TextCommands.endbutton(ctx, params)
    if not ctx._choiceButtons or #ctx._choiceButtons == 0 then
        ctx._choiceButtons = nil
        return
    end

    TextScene.remove_group(ctx, "choices")

    -- Store choice draws in the persistent text scene.
    local startY = 450
    local lineHeight = resolve_line_height(ctx)
    for idx, choice in ipairs(ctx._choiceButtons) do
        local y = startY + (idx - 1) * (lineHeight + 8)
        TextScene.add_text(
            ctx, idx .. ". " .. choice.text, 32, y,
            { r = 255, g = 255, b = 0, a = 255 }, "choices")
        -- Store button region for hit testing
        choice.y = y
        choice.h = lineHeight
        choice.index = idx
    end
    
    -- Install click handler for choice mode
    ctx._choiceButtonsActive = ctx._choiceButtons
    ctx._choiceButtons = nil
    ctx._choiceMode = true
    ctx.waiting_input = true

    -- Override the global click callback for choice detection. The engine
    -- dispatches clicks with no arguments (coalesced); read the mouse
    -- position from the per-frame globals instead.
    local oldClick = _G._KAG_onClick
    _G._KAG_onClick = function()
        if not ctx._choiceMode then
            if oldClick then oldClick() end
            return
        end
        local x, y = _G._GAME_MOUSE_X, _G._GAME_MOUSE_Y
        -- Hit-test against button regions
        local buttons = ctx._choiceButtonsActive
        if not buttons then return end
        for _, choice in ipairs(buttons) do
            if y >= choice.y and y <= choice.y + choice.h
               and x >= 32 and x <= 600 then
                ctx._selectedChoice = choice
                ctx._choiceMode = false
                ctx._choiceButtonsActive = nil
                ctx.waiting_input = false
                _G._KAG_onClick = oldClick  -- restore
                return
            end
        end
    end

    -- Block until user selects
    coroutine.yield()
    
    -- After selection, jump to target label
    local selected = ctx._selectedChoice
    ctx._selectedChoice = nil
    
    -- Ensure click handler is restored
    _G._KAG_onClick = oldClick
    TextScene.remove_group(ctx, "choices")
    
    if selected and selected.target then
        ctx._pendingJump = selected.target
    end
end

return TextCommands
