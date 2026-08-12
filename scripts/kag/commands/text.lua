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

-- Floor then clamp: float components (e.g. "128.9") must yield bytes
-- (review nit: max/min alone drops the floor for in-range floats).
local function clamp_byte(value)
    value = math.floor(tonumber(value) or 0)
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

-- Neo-Genesis contracts: typed + clamped via kag/schema.
local schema = require("kag.schema")
schema.define("ch", {
    _meta = { category = "text", blocking = true, desc = "KAG3-compatible ch command" },
    name   = { type = "string", default = "" },
    text   = { type = "string", default = "", interpolate = true },
    voice  = { type = "string", default = "" },
    sprite = { type = "string" },  -- no default: "" is truthy and would shadow storage/file
    max_width = { type = "number", default = 0, min = 0, max = 4096 },
    chars_per_line = { type = "number", default = 0, min = 0, max = 512 },
})
schema.define("text", {
    _meta = { category = "text", blocking = false, desc = "KAG3-compatible text command" },
    text = { type = "string", default = "", interpolate = true },
    fade_time = { type = "number", default = 0, min = 0, max = 30000 },
    fade = { type = "number", default = 0, min = 0, max = 30000 },
})
schema.define("ruby", {
    _meta = { category = "text", blocking = false, desc = "KAG3-compatible ruby command" },
    text = { type = "string", default = "" },
    ruby = { type = "string", default = "" },
    x = { type = "number", default = 0 },
    y = { type = "number", default = 0 },
    ruby_scale = { type = "number", default = 0.5, min = 0.1, max = 2.0 },
})
schema.define("font", {
    _meta = { category = "text", blocking = false, desc = "KAG3-compatible font command" },
    face = { type = "string", default = "default" },
    size = { type = "number", default = 22, min = 4, max = 256 },
    color = { type = "string", default = "white" },  -- KAG3 color param
})
-- Text-flow family (Neo-Genesis: typed + validated like every command).
schema.define("l", {
    _meta = { category = "text", blocking = false, desc = "line break" },
})          -- line break (no params)
schema.define("r", {
    _meta = { category = "text", blocking = false, desc = "carriage return" },
})          -- carriage return (no params)
schema.define("er", {
    _meta = { category = "text", blocking = false, desc = "erase line" },
})         -- erase line (no params)
schema.define("br", {
    _meta = { category = "text", blocking = false, desc = "KAG3 line-break alias" },
})         -- KAG3 line-break alias (no params)
schema.define("hr", {
    _meta = { category = "text", blocking = false, desc = "horizontal rule" },
})         -- horizontal rule (decorative)
schema.define("p", {
    _meta = { category = "text", blocking = true, desc = "click-to-advance" },
})          -- click-to-advance (no params)
schema.define("reset", {
    _meta = { category = "text", blocking = false, desc = "reset text state" },
})      -- reset text state (no params)
schema.define("s", {
    _meta = { category = "text", blocking = true, desc = "KAG3 short-wait" },            -- KAG3 short-wait
    ms = { type = "number", default = 250, min = 0, max = 60000 },
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

-- [textbox] -- Neo-Genesis message-window styling (KAG3 needed TJS ext).
-- Configures the message layer: position, size, background color and
-- opacity. State persists in ctx.textbox_style and is re-applied on
-- [cl] (clearscreen rebuilds the window).
schema.define("textbox", {
    _meta = { category = "text", blocking = false, desc = "KAG3-compatible textbox command" },
    x       = { type = "number", default = 0 },
    y       = { type = "number", default = 520 },
    w       = { type = "number", default = 1280, min = 64, max = 4096 },
    h       = { type = "number", default = 200, min = 32, max = 1024 },
    color   = { type = "string", default = "0,0,0" },
    opacity = { type = "number", default = 200, min = 0, max = 255 },
    visible = { type = "boolean", default = true },
})

function TextCommands.textbox(ctx, params)
    ctx.textbox_style = {
        x = params.x, y = params.y, w = params.w, h = params.h,
        color = params.color, opacity = params.opacity, visible = params.visible,
    }
    local layers = require("layers")
    local bg = layers.ensure(ctx, "_textbox", 2)  -- below message text
    bg.visible = params.visible
    bg.x, bg.y = params.x, params.y
    bg.w, bg.h = params.w, params.h
    -- color "r,g,b" -> solid texture (backend.create_solid_texture)
    local r, g, b = params.color:match("(%d+),%s*(%d+),%s*(%d+)")
    if r then
        bg.texture = backend.create_solid_texture(
            clamp_byte(r), clamp_byte(g), clamp_byte(b),
            math.floor(params.opacity))
    else
        bg.texture = nil  -- unparseable color: clear stale texture
    end
    layers.mark_dirty(bg)
end

-- [nameplate] -- Neo-Genesis character-name plate (KAG3 needed TJS ext).
-- Styles the speaker name display above the message window. When
-- configured, [ch name=X] shows the plate with the character's name;
-- the style persists in ctx.nameplate_style.
schema.define("nameplate", {
    _meta = { category = "text", blocking = false, desc = "KAG3-compatible nameplate command" },
    x       = { type = "number", default = 32 },
    y       = { type = "number", default = 480 },
    w       = { type = "number", default = 220, min = 32, max = 1024 },
    h       = { type = "number", default = 36, min = 16, max = 256 },
    color   = { type = "string", default = "0,0,0" },
    opacity = { type = "number", default = 220, min = 0, max = 255 },
    text_color = { type = "string", default = "255,255,255" },
})

function TextCommands.nameplate(ctx, params)
    ctx.nameplate_style = {
        x = params.x, y = params.y, w = params.w, h = params.h,
        color = params.color, opacity = params.opacity,
        text_color = params.text_color,
    }
    -- Re-render the current speaker's plate immediately.
    if ctx and ctx.current_speaker and #ctx.current_speaker > 0 then
        TextCommands._renderNameplate(ctx, ctx.current_speaker)
    end
end

-- Render the nameplate layer for a speaker (called by [ch] and [nameplate]).
function TextCommands._renderNameplate(ctx, speaker)
    local layers = require("layers")
    local bg = layers.ensure(ctx, "_nameplate", 3)  -- above textbox, below text
    local st = ctx.nameplate_style or {
        x = 32, y = 480, w = 220, h = 36,
        color = "0,0,0", opacity = 220,
        text_color = "255,255,255",
    }
    bg.visible = true
    bg.x, bg.y, bg.w, bg.h = st.x, st.y, st.w, st.h
    local r, g, b = st.color:match("(%d+),%s*(%d+),%s*(%d+)")
    if r then
        bg.texture = backend.create_solid_texture(
            clamp_byte(r), clamp_byte(g), clamp_byte(b),
            math.floor(st.opacity))
    else
        bg.texture = nil  -- unparseable color: clear stale plate
    end
    layers.mark_dirty(bg)
    -- Speaker name text: backend.render_text(text, x, y, r, g, b, a).
    local tr, tg, tb = st.text_color:match("(%d+),%s*(%d+),%s*(%d+)")
    backend.render_text(speaker, st.x + 8, st.y + 6,
        clamp_byte(tr or 255), clamp_byte(tg or 255), clamp_byte(tb or 255), 255)
end

-- [sprite_fade] -- character-sprite fade in/out (performance idiom:
-- KAG3 needed layeredit + tween glue for this). Animates the
-- _char_<speaker> layer opacity 0..255 via an operation yield loop.
schema.define("sprite_fade", {
    _meta = { category = "text", blocking = true, desc = "KAG3-compatible sprite_fade command" },
    speaker = { type = "string", required = true },
    to = { type = "number", default = 255, min = 0, max = 255 },
    time = { type = "number", default = 300, min = 0, max = 30000 },
})

function TextCommands.sprite_fade(ctx, params)
    local layers = require("layers")
    local name = "_char_" .. (params.speaker or "")
    local node = layers.get(name) or layers.find(name)
    if not node then
        print("[sprite_fade] no sprite layer: " .. name)
        return
    end
    local from = node.opacity or 255
    local to = params.to
    local dur = params.time
    if dur <= 0 then
        layers.set_layer_opacity(node, to)
        return
    end
    local operation <close> = require("kag.operation").start(ctx)
    local ct = operation.token
    local elapsed = 0
    while elapsed < dur and not ct.cancelled do
        elapsed = elapsed + (coroutine.yield() or 16)
        local t = math.min(1, elapsed / dur)
        local o = math.floor(from + (to - from) * t)
        layers.set_layer_opacity(node, o)
    end
    if not ct.cancelled then
        layers.set_layer_opacity(node, to)
        operation:complete()
    end
end

-- [sprite_move] -- character-sprite slide (entrance/exit performance).
-- Animates the _char_<speaker> layer x/y toward a target position.
schema.define("sprite_move", {
    _meta = { category = "text", blocking = true, desc = "KAG3-compatible sprite_move command" },
    speaker = { type = "string", required = true },
    x = { type = "number", default = 440 },
    y = { type = "number", default = 200 },
    time = { type = "number", default = 400, min = 0, max = 30000 },
})

function TextCommands.sprite_move(ctx, params)
    local layers = require("layers")
    local name = "_char_" .. (params.speaker or "")
    local node = layers.get(name) or layers.find(name)
    if not node then
        print("[sprite_move] no sprite layer: " .. name)
        return
    end
    local fromX, fromY = node.x or 0, node.y or 0
    local toX, toY = params.x, params.y
    local dur = params.time
    if dur <= 0 or (fromX == toX and fromY == toY) then
        layers.move_layer(node, toX, toY)
        return
    end
    local operation <close> = require("kag.operation").start(ctx)
    local ct = operation.token
    local elapsed = 0
    while elapsed < dur and not ct.cancelled do
        elapsed = elapsed + (coroutine.yield() or 16)
        local t = math.min(1, elapsed / dur)
        layers.move_layer(node, fromX + (toX - fromX) * t,
                               fromY + (toY - fromY) * t)
    end
    if not ct.cancelled then
        layers.move_layer(node, toX, toY)
        operation:complete()
    end
end

-- [sprite_scale] -- character-sprite zoom (performance emphasis).
-- Animates the _char_<speaker> layer scaleX/scaleY toward a target.
schema.define("sprite_scale", {
    _meta = { category = "text", blocking = true, desc = "KAG3-compatible sprite_scale command" },
    speaker = { type = "string", required = true },
    scale = { type = "number", default = 1.0, min = 0.1, max = 4.0 },
    time = { type = "number", default = 300, min = 0, max = 30000 },
})

function TextCommands.sprite_scale(ctx, params)
    local layers = require("layers")
    local name = "_char_" .. (params.speaker or "")
    local node = layers.get(name) or layers.find(name)
    if not node then
        print("[sprite_scale] no sprite layer: " .. name)
        return
    end
    local from = node.scaleX or node.scale or 1.0
    local to = params.scale
    local dur = params.time
    if dur <= 0 or math.abs(from - to) < 0.001 then
        node.scaleX, node.scaleY = to, to
        layers.mark_dirty(node)
        return
    end
    local operation <close> = require("kag.operation").start(ctx)
    local ct = operation.token
    local elapsed = 0
    while elapsed < dur and not ct.cancelled do
        elapsed = elapsed + (coroutine.yield() or 16)
        local t = math.min(1, elapsed / dur)
        local sc = from + (to - from) * t
        node.scaleX, node.scaleY = sc, sc
        layers.mark_dirty(node)
    end
    if not ct.cancelled then
        node.scaleX, node.scaleY = to, to
        layers.mark_dirty(node)
        operation:complete()
    end
end

-- [sprite_swap] -- character re-dress / expression swap (performance
-- idiom: KAG3 needed layeredit + reload glue). Swaps the standing
-- portrait's texture and re-registers the sprite for future [ch].
schema.define("sprite_swap", {
    _meta = { category = "text", blocking = true, desc = "KAG3-compatible sprite_swap command" },
    speaker = { type = "string", required = true },
    sprite = { type = "string", required = true },
})

function TextCommands.sprite_swap(ctx, params)
    local layers = require("layers")
    local name = "_char_" .. (params.speaker or "")
    local node = layers.get(name) or layers.find(name)
    if not node then
        print("[sprite_swap] no sprite layer: " .. name)
        return
    end
    local tex = backend.load_texture(params.sprite)
    if not tex or tex == 0 then
        print("[sprite_swap] failed to load: " .. params.sprite)
        return  -- keep the current outfit visible
    end
    node.texture = tex
    layers.mark_dirty(node)
    -- Re-register so later [ch name=<speaker>] keeps the new outfit.
    ctx.characters = ctx.characters or {}
    if ctx.characters[params.speaker] then
        ctx.characters[params.speaker].sprite = params.sprite
    end
end

function TextCommands.ch(ctx, params)
    local speaker = params.name or params.character or ""
    local message = params.text or params.message or ""

    -- U1.3: pos parameter (left/center/right) for text alignment
    local pos = params.pos or "center"
    if pos ~= "left" and pos ~= "center" and pos ~= "right" then
        pos = "center"
    end

    -- Neo-Genesis: show the nameplate when a speaker is present.
    ctx.current_speaker = speaker
    if ctx.nameplate_style and #speaker > 0 then
        TextCommands._renderNameplate(ctx, speaker)
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
        -- Guard must admit sprite-only lines (storage/file are nil then)
        -- and reject the contract's "" sprite default (truthy -> would
        -- shadow storage/file and create a bogus empty layer).
        if (params.sprite and params.sprite ~= "") or params.storage or params.file then
            -- sprite= is the contract-advertised param; storage/file are the
            -- KAG3 aliases. All three register the standing portrait.
            ctx.characters[speaker].sprite =
                (params.sprite ~= "" and params.sprite) or params.storage or params.file
        end
    end

    -- Voice: [ch voice="assets/voice/x.wav"] plays the line and stores the
    -- file in the backlog entry so the history overlay's V key can replay it.
    -- Accessibility: with cc_mode enabled the CURRENT line becomes the
    -- closed-caption text (drawn at a fixed bottom position by
    -- kag_runner.render) while a voice line is on screen.
    local voiceFile = params.voice or params.voicefile or ""
    if #voiceFile > 0 then
        -- Closed captions: with cc_mode enabled the voiced line becomes
        -- the standing caption until the next voiced (or voiceless) line.
        -- cc_mode lives on ctx (set by Settings._applyAll, game scripts,
        -- or the runner's startup sync from config) -- ch NEVER loads
        -- config itself (its require chain is unavailable in degraded
        -- contexts and would disturb suite ordering).
        if ctx.cc_mode == true then
            ctx.cc_text = { speaker = speaker, text = message }
        end
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
    else
        -- No voice on this line: clear any standing caption (CC shows only
        -- lines that are actually voiced; the textbox itself stays).
        ctx.cc_text = nil
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
        -- Skip only already-seen text ([skip mode=seen]); a second
        -- [skip mode=seen] turns it OFF. (Audit fix: the old
        -- `cond and false or "seen"` is a value-selector, NOT a branch --
        -- it ALWAYS yielded "seen", so seen-skip could never be turned
        -- off.)
        if ctx.skip_mode == "seen" then
            ctx.skip_mode = false
        else
            ctx.skip_mode = "seen"
        end
    else
        ctx.skip_mode = not ctx.skip_mode
    end
end

-- =============================================================================
--  [auto] ?? toggle auto-advance mode
--  In auto mode the runner advances past click-waits ([p]) after ~1.5s,
--  like a visual-novel auto-play button. State is persisted by [save].
-- =============================================================================

-- Neo-Genesis: explicit mode param (on/off/toggle) beyond KAG3's bare toggle.
schema.define("auto", {
    _meta = { category = "text", blocking = false, desc = "KAG3-compatible auto command" },
    mode = { type = "string", choices = { ["on"] = true, ["off"] = true, ["toggle"] = true } },
})

function TextCommands.auto(ctx, params)
    local m = params.mode or "toggle"
    if m == "on" then ctx.auto_mode = true
    elseif m == "off" then ctx.auto_mode = false
    else ctx.auto_mode = not ctx.auto_mode end
end

-- [voice_off] -- mute/unmute voice without stopping the engine bus
-- (Neo-Genesis convenience: KAG3 needed stopvoice + a saved setting to mute).
schema.define("voice_off", {
    _meta = { category = "text", blocking = false, desc = "KAG3-compatible voice_off command" },
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

-- Neo-Genesis contract: typed + clamped (replaces the inline clamps).
require("kag.schema").define("pt", {
    _meta = { category = "text", blocking = false, desc = "point text at position" },
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
    -- bare [button *route_a text="..."] -> params[1] as target
    -- (consistency with jump/call/link -- audit)
    local target = params.target or params.storage
    if target == nil and type(params[1]) == "string" then
        target = params[1]
    end
    -- Neo-Genesis: [button cond="f.x > 1"] — conditional choice (Ren'Py
    -- menu `if` parity). Evaluated when [endbutton] renders the block;
    -- false choices are hidden. TJS syntax, runtime-translated.
    table.insert(ctx._choiceButtons, {
        text = text, target = target or "", cond = params.cond,
    })
end

function TextCommands.endbutton(ctx, params)
    if not ctx._choiceButtons or #ctx._choiceButtons == 0 then
        ctx._choiceButtons = nil
        return
    end

    -- Neo-Genesis: [button cond=...] — drop choices whose condition is
    -- false (Ren'Py menu `if` parity). All-hidden blocks just dissolve.
    local exprLang = require("kag.expr")
    local filtered = {}
    for _, choice in ipairs(ctx._choiceButtons) do
        local cond = choice.cond
        if type(cond) == "string" and cond ~= "" then
            local ok, v = exprLang.evaluate(ctx, cond)
            if ok and v then filtered[#filtered + 1] = choice end
        else
            filtered[#filtered + 1] = choice
        end
    end
    ctx._choiceButtons = filtered
    if #filtered == 0 then
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

-- KAG3 select syntax: [select] opens the block (no-op), [sel] registers
-- an option (same fields as [button]), [endselect] renders + blocks
-- (same as [endbutton]). The choice engine is shared; no schema is
-- needed (button itself is unmigrated -- raw params pass through).
function TextCommands.select(ctx, params)
    ctx._choiceButtons = ctx._choiceButtons or {}
end

TextCommands.sel = TextCommands.button

function TextCommands.endselect(ctx, params)
    return TextCommands.endbutton(ctx, params)
end

return TextCommands
