-- ===========================================================================
--  Caesura (AmeKAG) -- kag.lua
--  KAG command dispatch library. One function per tag.
--  Spec: Appendix A — 50+ commands, krkrz KAG Lua port.
--  Call convention: kag.<cmd>(ctx, params) → nil | "jump" | "return" | "stop"
--  Blocking commands use coroutine.yield(); non-blocking commands return immediately.
-- ===========================================================================

local backend    = require("backend")
local audio      = require("audio")
local layers     = require("layers")
local system     = require("system")
local flow       = require("flow")
local rtt        = require("rtt")
local transition = require("transition")
local transform  = require("transform")
local vfx        = require("vfx")

local kag = {}
kag.scheduler_ref = nil  -- set by scheduler

-- ===========================================================================
--  Text / Message commands
-- ===========================================================================

--- [text] — display text on message layer
function kag.text(ctx, params)
    local content = params[1] or params.text or ""
    if #content == 0 then return end
    -- i18n expansion
    local ok, i18n = pcall(require, "i18n")
    if ok then content = i18n.expand(content) end
    ctx.backlog = ctx.backlog or {}
    ctx.backlog[#ctx.backlog + 1] = content
    -- Store for per-frame re-render
    ctx.currentText = content
    ctx.textDisplayX = ctx.textCursorX or 32
    ctx.textDisplayY = ctx.textCursorY or 600
    backend.show_text(content)
    backend.render_text(content, ctx.textDisplayX, ctx.textDisplayY)
    local gw = (backend.line_height() > 16) and 16 or 8
    ctx.textCursorX = ctx.textDisplayX + #content * gw
end

--- [ch] — character-by-character display with speed (ms/char)
function kag.ch(ctx, params)
    local content = params.text or params[1] or ""
    local ok, i18n = pcall(require, "i18n")
    if ok then content = i18n.expand(content) end
    local speed = (tonumber(params.speed) or 50) / 1000.0
    ctx.backlog = ctx.backlog or {}
    ctx.backlog[#ctx.backlog + 1] = content
    local cx = ctx.textCursorX or 32
    local cy = ctx.textCursorY or 600
    for i = 1, #content do
        ctx.currentText = content:sub(1, i)
        ctx.textDisplayX = cx
        ctx.textDisplayY = cy
        local elapsed = 0
        while elapsed < speed do
            elapsed = elapsed + (coroutine.yield() or 0)
        end
    end
    local gw2 = (backend.line_height() > 16) and 16 or 8
    ctx.textCursorX = cx + #content * gw2
end

--- [p] — wait for user click (blocking)
function kag.p(ctx, params)
    ctx.waiting_input = true
    while ctx.waiting_input do
        coroutine.yield()
    end
end

--- [l] — alias for [p] (krkrz: line break + click wait)
kag.l = kag.p

--- [r] — newline: reset cursor X, advance Y
function kag.r(ctx, params)
    ctx.textCursorX = 32
    local lineH = backend.line_height()
    ctx.textCursorY = (ctx.textCursorY or 600) + lineH
end

--- [er] — erase message layer
function kag.er(ctx, params)
    ctx.textCursorX = 32
    ctx.textCursorY = 600
    ctx.currentText = nil
    backend.clear_text()
end

--- [current] — set target text layer
function kag.current(ctx, params)
    ctx.textLayer = params.layer or params[1] or "message0"
end

--- [ruby] — text with ruby annotation above (krkrz TextRenderer::renderRuby)
function kag.ruby(ctx, params)
    local text = params.text or ""
    local ruby = params.ruby or ""
    if #text == 0 then return end
    ctx.backlog = ctx.backlog or {}
    ctx.backlog[#ctx.backlog + 1] = text
    local cx = ctx.textCursorX or 32
    local cy = ctx.textCursorY or 600
    backend.render_ruby(text, ruby, cx, cy)
    ctx.textCursorX = cx + #text * backend.line_height() * 0.5
end

--- [font] — switch embedded font (0=Small, 1=Large; krkrz TextRenderer::setFont)
function kag.font(ctx, params)
    local id = tonumber(params.size) or 0
    if params.size == "large" or params.size == "big" then id = 1 end
    backend.set_font(id)
end

-- ===========================================================================

﻿--- [locate] — set text cursor position (krkrz: message layer cursor)
--  params: x, y
function kag.locate(ctx, params)
    if params.x ~= nil then ctx.textCursorX = tonumber(params.x) end
    if params.y ~= nil then ctx.textCursorY = tonumber(params.y) end
end

--- [wt] — write-text with animation effect (krkrz: text animation)
--  params: text, speed, effect(fadein/slide/typewriter), time
function kag.wt(ctx, params)
    local content = params.text or params[1] or ""
    local ok, i18n = pcall(require, "i18n")
    if ok then content = i18n.expand(content) end
    local speed = (tonumber(params.speed) or 50) / 1000.0
    local effect = params.effect or "typewriter"
    ctx.backlog = ctx.backlog or {}
    ctx.backlog[#ctx.backlog + 1] = content
    local cx = ctx.textCursorX or 32
    local cy = ctx.textCursorY or 600
    if effect == "typewriter" then
        -- Character-by-character (like [ch])
        for i = 1, #content do
            ctx.currentText = content:sub(1, i)
            ctx.textDisplayX = cx
            ctx.textDisplayY = cy
            local elapsed = 0
            while elapsed < speed do elapsed = elapsed + (coroutine.yield() or 0) end
        end
    elseif effect == "fadein" then
        -- Fade in whole text at once
        local dur = (tonumber(params.time) or 500) / 1000.0
        local elapsed = 0
        while elapsed < dur do
            elapsed = elapsed + (coroutine.yield() or 0.016)
            local alpha = math.min(255, math.floor((elapsed / dur) * 255))
            ctx.currentText = content
            ctx.textAlpha = alpha
            backend.render_text(content, cx, cy, alpha)
        end
        ctx.textAlpha = nil
    else
        -- Instant display
        ctx.currentText = content
        backend.render_text(content, cx, cy)
    end
    ctx.textDisplayX = cx
    ctx.textDisplayY = cy
    local gw = (backend.line_height() > 16) and 16 or 8
    ctx.textCursorX = cx + #content * gw
end

--- [wa] — write-animation: text with per-character animation (krkrz: wave/fade chars)
--  params: text, speed, effect(wave/jump/fade), amplitude, time
function kag.wa(ctx, params)
    local content = params.text or params[1] or ""
    local ok, i18n = pcall(require, "i18n")
    if ok then content = i18n.expand(content) end
    local speed = (tonumber(params.speed) or 60) / 1000.0
    local effect = params.effect or "fade"
    local amplitude = tonumber(params.amplitude) or 4
    ctx.backlog = ctx.backlog or {}
    ctx.backlog[#ctx.backlog + 1] = content
    local cx = ctx.textCursorX or 32
    local cy = ctx.textCursorY or 600
    local gw = (backend.line_height() > 16) and 16 or 8
    -- Build all characters
    local chars = {}
    for i = 1, #content do
        chars[i] = { char = content:sub(i, i), x = cx + (i-1)*gw, y = cy }
    end
    -- Animate them entering
    for i = 1, #chars do
        local elapsed = 0
        while elapsed < speed do elapsed = elapsed + (coroutine.yield() or 0) end
        ctx.currentText = content:sub(1, i)
        -- Apply per-char animation
        for j = 1, i do
            local offset = 0
            if effect == "wave" then
                offset = math.sin((i - j) * 0.8 + elapsed * 8) * amplitude
            elseif effect == "jump" then
                offset = (j == i) and -amplitude * 3 or 0
            end
            if offset ~= 0 then
                backend.render_text(chars[j].char, chars[j].x, chars[j].y + offset)
            end
        end
    end
    ctx.textDisplayX = cx
    ctx.textDisplayY = cy
    ctx.textCursorX = cx + #content * gw
end

--  Image / Layer commands
-- ===========================================================================

--- [bg] — set background layer (z=0)
function kag.bg(ctx, params)
    local layer = layers.ensure(ctx, "bg", 0)
    local src = params.storage or params.file or params[1]
    if src then
        layers.load_image(layer, src)
        if not layer.texture then
            -- Queue deferred solid fill
            ctx._pending_solids = ctx._pending_solids or {}
            local h = 0
            for c in src:gmatch(".") do h = (h * 31 + c:byte()) % 16777216 end
            table.insert(ctx._pending_solids, { layer = layer, name = src, color = h })
        end
    end
    layer.blend = params.blend or "alpha"
    layer.visible = (params.visible ~= false)
    layer.dirty = true
end

--- [fg] — set foreground/sprite layer (z=1)
function kag.fg(ctx, params)
    local index = tonumber(params.index or params[1]) or 0
    local layer = layers.ensure(ctx, "fg", index + 1)
    local src = params.storage or params.file or params[2]
    if src then layers.load_image(layer, src) end
    layer.blend = params.blend or "alpha"
    layer.visible = (params.visible ~= false)
    layer.dirty = true
end

--- [image] / [img] — display image at specified layer/index
function kag.image(ctx, params)
    local layerName = params.layer or "fg"
    local index = tonumber(params.index or params[1]) or 0
    local layer = layers.ensure(ctx, layerName, index)
    local src = params.storage or params.file or params[2]
    if src then layers.load_image(layer, src) end
    if params.x then layer.x = tonumber(params.x) end
    if params.y then layer.y = tonumber(params.y) end
    if params.scale then layer.scale = tonumber(params.scale) end
    if params.opacity then layer.opacity = tonumber(params.opacity) end
    layer.blend = params.blend or "alpha"
    layer.visible = (params.visible ~= false)
    layer.dirty = true
end
kag.img = kag.image  -- alias

--- [cl] — clear specified layer
function kag.cl(ctx, params)
    local layerName = params.layer or params[1] or "fg"
    layers.clear(ctx, layerName)
end

--- [freeimage] — release image resource
function kag.freeimage(ctx, params)
    local src = params.storage or params.file or params[1]
    if src then
        backend.free_texture(src)
    end
end

-- ===========================================================================
﻿-- ===========================================================================
--  Layer position & animation commands
-- ===========================================================================

--- [position] — set layer position/dimensions/opacity (krkrz: layer positioning)
--  params: layer, left, top, width, height, opacity, visible, move, time
function kag.position(ctx, params)
    local layerName = params.layer or "fg"
    local layer = ctx.layers and ctx.layers[layerName]
    if not layer then
        layer = layers.ensure(ctx, layerName, 5)
    end
    if params.left  ~= nil then layer.x = tonumber(params.left)  end
    if params.top   ~= nil then layer.y = tonumber(params.top)   end
    if params.width ~= nil then layer.w = tonumber(params.width) end
    if params.height~= nil then layer.h = tonumber(params.height)end
    if params.opacity~= nil then
        layer.opacity = math.max(0, math.min(255, tonumber(params.opacity)))
        layer.alpha = layer.opacity / 255.0
    end
    if params.visible ~= nil then
        layer.visible = (params.visible ~= "false" and params.visible ~= false)
    end
    -- Animated move
    local dur = tonumber(params.time or params.move)
    if dur and dur > 0 then
        local tx = tonumber(params.left) or layer.x
        local ty = tonumber(params.top)  or layer.y
        local sx, sy = layer.x, layer.y
        local elapsed = 0
        dur = dur / 1000.0
        while elapsed < dur do
            elapsed = elapsed + (coroutine.yield() or 0.016)
            local t = elapsed / dur
            if t > 1 then t = 1 end
            layer.x = sx + (tx - sx) * t
            layer.y = sy + (ty - sy) * t
            layer.dirty = true
        end
    end
    layer.dirty = true
end

--- [anime] — sprite sequence animation on a layer (krkrz: AnimeLayer)
--  params: layer, name, interval, from, to, loop, movie (file list)
function kag.anime(ctx, params)
    local layerName = params.layer or "fg"
    local layer = ctx.layers and ctx.layers[layerName]
    if not layer then
        layer = layers.ensure(ctx, layerName, 5)
    end
    local base = params.name or params.file or params[1]
    local interval = tonumber(params.interval or params.time) or 100
    local from = tonumber(params.from) or 0
    local to   = tonumber(params.to)   or 0
    local loopCount = tonumber(params.loop) or -1
    -- Movie-style: comma-separated frame list
    local movieStr = params.movie or ""
    local frames = {}
    if #movieStr > 0 then
        for f in movieStr:gmatch("[^,]+") do
            table.insert(frames, f:match("^%s*(.-)%s*$"))
        end
    elseif base and to >= from then
        for i = from, to do
            table.insert(frames, base .. tostring(i))
        end
    end
    if #frames == 0 then return end
    local idx = 1
    local loops = 0
    local durSec = interval / 1000.0
    while loopCount < 0 or loops < loopCount do
        local frame = frames[idx]
        if frame then
            layers.load_image(layer, frame)
            layer.dirty = true
        end
        idx = idx + 1
        if idx > #frames then
            idx = 1
            loops = loops + 1
        end
        local elapsed = 0
        while elapsed < durSec do elapsed = elapsed + (coroutine.yield() or 0.016) end
    end
end

--  Audio commands (routed through audio.lua → backend.audio_*)
-- ===========================================================================

--- [playbgm] — play/switch BGM
function kag.playbgm(ctx, params)
    local src = params.storage or params.file or params[1]
    if not src then return end
    backend.audio_play("bgm", src, {
        loop = (params.loop ~= false),
        volume = tonumber(params.volume) or 100,
        fadein = tonumber(params.fadein) or 0,
    })
end

--- [stopbgm] — stop BGM
function kag.stopbgm(ctx, params)
    local fadeout = tonumber(params.fadeout or params.time) or 0
    if fadeout > 0 then
        backend.audio_fade_volume("bgm", 0, fadeout)
    else
        backend.audio_stop("bgm")
    end
end

--- [fadebgm] — fade BGM to silence
function kag.fadebgm(ctx, params)
    local time = tonumber(params.time) or 1000
    backend.audio_fade_volume("bgm", 0, time)
end

--- [xfadebgm] — crossfade BGM
function kag.xfadebgm(ctx, params)
    local src = params.storage or params.file or params[1]
    local time = tonumber(params.time) or 1000
    if src then
        audio.crossfade_bgm(src, time / 1000)
    end
end

--- [playse] — play sound effect
function kag.playse(ctx, params)
    local src = params.storage or params.file or params[1]
    if not src then return end
    backend.audio_play("se", src, {
        volume = tonumber(params.volume) or 100,
    })
end

--- [stopse] — stop all sound effects
function kag.stopse(ctx, params)
    backend.audio_stop("se")
end

--- [fadeinse] — fade-in sound effect
function kag.fadeinse(ctx, params)
    local src = params.storage or params.file or params[1]
    local time = tonumber(params.time) or 1000
    if src then
        backend.audio_play("se", src, { volume = 0 })
        backend.audio_fade_volume("se", 1.0, time)
    end
end

--- [playvoice] — play voice (blocking — waits until playback ends)
function kag.playvoice(ctx, params)
    local src = params.storage or params.file or params[1]
    if not src then return end
    backend.audio_play("voice", src)
    while backend.audio_is_playing("voice") do
        coroutine.yield()
    end
end

--- [stopvoice] — stop voice playback
function kag.stopvoice(ctx, params)
    backend.audio_stop("voice")
end

--- [waitsound] — wait until all audio finishes (blocking)
function kag.waitsound(ctx, params)
    while backend.audio_is_playing("bgm") or backend.audio_is_playing("se") or backend.audio_is_playing("voice") do
        coroutine.yield()
    end
end

--- [playse3d] — play 3D positional sound effect
function kag.playse3d(ctx, params)
    local src = params.storage or params.file or params[1]
    if not src then return end
    local x = tonumber(params.x) or 0
    local y = tonumber(params.y) or 0
    local z = tonumber(params.z) or 0
    backend.audio_play("se", src, { x = x, y = y, z = z, volume = tonumber(params.volume) or 100,
    })
end

--- [setlistener] — set 3D audio listener position/orientation
function kag.setlistener(ctx, params)
    local x  = tonumber(params.x or params.atx) or 0
    local y  = tonumber(params.y or params.aty) or 0
    local z  = tonumber(params.z or params.atz) or 0
    local ax = tonumber(params.ax) or 0
    local ay = tonumber(params.ay) or 0
    local az = tonumber(params.az) or 0
    backend.audio_set_listener(x, y, z, ax, ay, az)
end

-- ===========================================================================

﻿--- [freebgm] — release BGM resource from memory (krkrz: free bgm cache)
--  params: storage (bgm name to free)
function kag.freebgm(ctx, params)
    local src = params.storage or params.file or params[1]
    if src then
        backend.flush_wave_cache()
    end
end

--- [bgmvol] — set BGM volume (0-100) (krkrz: BGM bus volume)
--  params: volume, time (fade duration ms)
function kag.bgmvol(ctx, params)
    local vol = tonumber(params.volume) or 100
    local dur = tonumber(params.time)
    if dur and dur > 0 then
        audio.fade_bgm_volume(vol / 100.0, dur / 1000.0)
        local elapsed = 0
        dur = dur / 1000.0
        while elapsed < dur do elapsed = elapsed + (coroutine.yield() or 0) end
    else
        audio.set_bgm_volume(vol / 100.0)
    end
end

--- [sevol] — set SE volume (0-100) (krkrz: SE bus volume)
--  params: volume, time (fade duration ms)
function kag.sevol(ctx, params)
    local vol = tonumber(params.volume) or 100
    local dur = tonumber(params.time)
    if dur and dur > 0 then
        audio.fade_se_volume(vol / 100.0, dur / 1000.0)
        local elapsed = 0
        dur = dur / 1000.0
        while elapsed < dur do elapsed = elapsed + (coroutine.yield() or 0) end
    else
        audio.set_se_volume(vol / 100.0)
    end
end

--- [voicevol] — set VOICE volume (0-100) (krkrz: voice bus volume)
--  params: volume, time (fade duration ms)
function kag.voicevol(ctx, params)
    local vol = tonumber(params.volume) or 100
    audio.set_voice_volume(vol / 100.0)
end

--  Animation / Transition commands
-- ===========================================================================

--- [wait] — wait for specified milliseconds (blocking)
function kag.wait(ctx, params)
    local dur = tonumber(params.time or params[1]) or 1000
    local elapsed = 0
    while elapsed < dur do
        elapsed = elapsed + (coroutine.yield() or 0)
    end
end

--- [trans] / [waittrans] — execute transition effect (blocking)

﻿--- [delay] — simple delay without UI interaction (krkrz: non-interactive wait)
--  params: time (ms)
function kag.delay(ctx, params)
    local dur = (tonumber(params.time) or 1000) / 1000.0
    local elapsed = 0
    while elapsed < dur do elapsed = elapsed + (coroutine.yield() or 0) end
end

function kag.trans(ctx, params)
    local dur = tonumber(params.time) or 1000
    local elapsed = 0
    local method = params.method or "crossfade"
    local rule_img = nil
    if params.rule and params.rule ~= "" then
        rule_img = transition.load_rule(params.rule)
    end
    while elapsed < dur do
        elapsed = elapsed + (coroutine.yield() or 0)
        local t = math.min(elapsed / dur, 1.0)
        if method == "rule" and rule_img then
            transition.rule(ctx.from_rt, ctx.to_rt, rule_img, t)
        else
            transition.crossfade(ctx.from_rt, ctx.to_rt, t)
        end
    end
end
kag.waittrans = kag.trans  -- alias

--- [move] — move/scale/rotate layer (blocking)
function kag.move(ctx, params)
    local layerName = params.layer or "fg"
    local index = tonumber(params.index) or 0
    local layer = layers.ensure(ctx, layerName, index)
    local dur = tonumber(params.time) or 500
    local elapsed = 0
    local sx, sy = layer.x or 0, layer.y or 0
    local tx = tonumber(params.x) or sx
    local ty = tonumber(params.y) or sy
    local ss = layer.scale or 1.0
    local ts = tonumber(params.scale) or ss
    local sr = layer.rotate or 0
    local tr = tonumber(params.rotate) or sr
    local so = layer.opacity or 255
    local to = tonumber(params.opacity) or so
    while elapsed < dur do
        elapsed = elapsed + (coroutine.yield() or 0)
        local t = math.min(elapsed / dur, 1.0)
        layer.x = sx + (tx - sx) * t
        layer.y = sy + (ty - sy) * t
        layer.scale = ss + (ts - ss) * t
        layer.rotate = sr + (tr - sr) * t
        layer.opacity = so + (to - so) * t
        layer.dirty = true
    end
    layer.x, layer.y = tx, ty
    layer.scale, layer.rotate, layer.opacity = ts, tr, to
end

--- [quake] / [shake] — screen shake effect (blocking)
function kag.quake(ctx, params)
    local dur = tonumber(params.time) or 500
    local intensity = tonumber(params.intensity or params[1]) or 10
    local elapsed = 0
    vfx.start_quake(intensity)
    while elapsed < dur do
        elapsed = elapsed + (coroutine.yield() or 0)
    end
    vfx.stop_quake()
end
kag.shake = kag.quake  -- alias

--- [stopquake] — stop screen shake immediately
function kag.stopquake(ctx, params)
    vfx.stop_quake()
end

--- [fade] / [flash] — layer fade in/out (blocking)
function kag.fade(ctx, params)
    local layerName = params.layer or "fg"
    local index = tonumber(params.index) or 0
    local layer = layers.ensure(ctx, layerName, index)
    local fadeType = params.type or "in"
    local targetOpacity = (fadeType == "out") and 0 or 255
    local dur = tonumber(params.time) or 500
    local elapsed = 0
    local startO = layer.opacity or ((fadeType == "out") and 255 or 0)
    while elapsed < dur do
        elapsed = elapsed + (coroutine.yield() or 0)
        local t = math.min(elapsed / dur, 1.0)
        layer.opacity = startO + (targetOpacity - startO) * t
        layer.dirty = true
    end
    layer.opacity = targetOpacity
end
kag.flash = kag.fade  -- alias

--- [blur] — blur layer (blocking)
function kag.blur(ctx, params)
    local layerName = params.layer or "fg"
    local index = tonumber(params.index) or 0
    local layer = layers.ensure(ctx, layerName, index)
    local targetBlur = tonumber(params.amount) or 5
    local dur = tonumber(params.time) or 500
    local elapsed = 0
    local startBlur = layer.blur_amount or 0
    while elapsed < dur do
        elapsed = elapsed + (coroutine.yield() or 0)
        local t = math.min(elapsed / dur, 1.0)
        layer.blur_amount = startBlur + (targetBlur - startBlur) * t
        layer.dirty = true
    end
    layer.blur_amount = targetBlur
end

-- ===========================================================================

﻿-- ===========================================================================
--  Particle effects: [snow] [rain] [sakura] [stars] (krkrz: weather effects)
--  Pure Lua particle system using coroutine.yield for frame-stepped animation.
--  params: time(ms), count(particles), speed, size, loop
-- ===========================================================================

local function kag_weather_particles(ctx, params, ptype)
    local dur = tonumber(params.time or params.duration) or -1
    local count = tonumber(params.count or params.num) or 50
    local spd = tonumber(params.speed) or 1.0
    local size = tonumber(params.size) or 3
    local screenW = params.width or 1280
    local screenH = params.height or 720
    -- Pre-generate particles
    local particles = {}
    for i = 1, count do
        particles[i] = {
            x = math.random() * screenW,
            y = math.random() * -screenH,
            speed = (0.3 + math.random() * 0.7) * spd * 60,
            size = (0.5 + math.random()) * size,
            wobble = math.random() * math.pi * 2,
            wobbleSpeed = 0.5 + math.random() * 2.0,
        }
    end
    local elapsed = 0
    local durSec = (dur > 0) and (dur / 1000.0) or nil
    while (not durSec) or elapsed < durSec do
        local dt = coroutine.yield() or 0.016
        elapsed = elapsed + dt
        -- Update & render particles
        backend.begin_particles()
        for _, p in ipairs(particles) do
            -- Move downward
            p.y = p.y + p.speed * dt
            p.wobble = p.wobble + p.wobbleSpeed * dt
            -- Lateral wobble
            local wobbleX = math.sin(p.wobble) * 15
            local px = p.x + wobbleX
            local py = p.y
            -- Wrap around
            if py > screenH + 10 then
                py = -10
                p.x = math.random() * screenW
            end
            -- Different shapes per type
            backend.draw_particle(px, py, p.size, ptype)
        end
        backend.end_particles()
    end
    backend.clear_particles()
end

function kag.snow(ctx, params)
    return kag_weather_particles(ctx, params, "snow")
end

function kag.rain(ctx, params)
    return kag_weather_particles(ctx, params, "rain")
end

function kag.sakura(ctx, params)
    return kag_weather_particles(ctx, params, "sakura")
end

function kag.stars(ctx, params)
    return kag_weather_particles(ctx, params, "stars")
end

--  Flow Control commands (delegate to flow.lua)
-- ===========================================================================

--- [jump] — jump to label
function kag.jump(ctx, params)
    return flow.jump(ctx, params)
end

--- [call] — call subroutine
function kag.call(ctx, params)
    return flow.call(ctx, params)
end

--- [return] — return from subroutine
function kag.return_(ctx, params)
    return flow.return_(ctx, params)
end

--- [link] — jump to another .ks file
function kag.link(ctx, params)
    -- In option/selnum mode, collect choice instead of jumping
    if ctx.optionMode then
        local text = params.text or params[1] or ""
        if #text == 0 and ctx.currentText then text = ctx.currentText end
        ctx.optionChoices = ctx.optionChoices or {}
        table.insert(ctx.optionChoices, {
            text = text,
            target = params.target or params.storage or params[1],
            x = tonumber(params.x) or ctx.textCursorX or 32,
            y = tonumber(params.y) or ctx.textCursorY or 600,
        })
        ctx.currentText = nil
        return
    end
    if ctx.selnumMode then
        local text = params.text or params[1] or ""
        if #text == 0 and ctx.currentText then text = ctx.currentText end
        ctx.selnumChoices = ctx.selnumChoices or {}
        table.insert(ctx.selnumChoices, {
            text = text,
            target = params.target or params.storage or params[1],
        })
        ctx.currentText = nil
        return
    end
    return flow.link(ctx, params)
end

--- [goto] — alias for [jump]
kag["goto"] = function(ctx, params)
    return flow.goto_(ctx, params)
end

--- [end] — end current script
function kag.end_(ctx, params)
    return flow.end_(ctx, params)
end

--- [label] — no-op; labels pre-resolved by scheduler
function kag.label(ctx, params)
    -- scheduler.load_script builds labelMap
end

--- [if] / [elif] / [else] / [endif] — conditional branching
-- Note: actual token-skip logic is in scheduler._execute;
-- these are provided for direct programmatic use.
function kag.if_(ctx, params)
    flow.if_(ctx, params)
end
function kag.elif_(ctx, params)
    flow.elif_(ctx, params)
end
function kag.else_(ctx, params)
    flow.else_(ctx, params)
end
function kag.endif(ctx, params)
    flow.endif(ctx, params)
end

--- [switch] / [case] / [default] / [endswitch]
function kag.switch(ctx, params)
    flow.switch_(ctx, params)
end
function kag.case(ctx, params)
    flow.case_(ctx, params)
end
function kag.default(ctx, params)
    flow.default_(ctx, params)
end
function kag.endswitch(ctx, params)
    flow.endswitch_(ctx, params)
end

-- ===========================================================================

﻿-- ===========================================================================
--  Choice / Selection commands (UI)
-- ===========================================================================

--- [option] — begin a clickable choice menu (krkrz: option layer)
--  Each option block ends at [endoption]; clicking a choice jumps to its target.
--  params: (none from the option tag; choices are defined by subsequent text + link)
function kag.option(ctx, params)
    -- Enter option mode — the scheduler will show option overlay
    ctx.optionMode = true
    ctx.optionChoices = {}
    ctx.optionCursor = 0
    ctx._savedTextCursorX = ctx.textCursorX
    ctx._savedTextCursorY = ctx.textCursorY
end

--- [endoption] — end the option block and wait for player selection
function kag.endoption(ctx, params)
    ctx.optionMode = false
    if not ctx.optionChoices or #ctx.optionChoices == 0 then
        ctx.textCursorX = ctx._savedTextCursorX
        ctx.textCursorY = ctx._savedTextCursorY
        return
    end
    -- Draw options and wait for selection
    local selected = nil
    while not selected do
        -- Show choices on screen
        backend.clear_text()
        local optY = ctx._savedTextCursorY or 600
        for i, choice in ipairs(ctx.optionChoices) do
            local prefix = (i == ctx.optionCursor + 1) and "> " or "  "
            backend.render_text(prefix .. choice.text, choice.x or 50, optY)
            optY = optY + (backend.line_height() or 24) + 4
        end
        ctx.textCursorX = ctx._savedTextCursorX
        ctx.textCursorY = ctx._savedTextCursorY
        -- Wait for input
        coroutine.yield()
        -- Check input (up/down/click)
        if ctx.pendingInput then
            local inp = ctx.pendingInput
            ctx.pendingInput = nil
            if inp == "up" then
                ctx.optionCursor = math.max(0, ctx.optionCursor - 1)
            elseif inp == "down" then
                ctx.optionCursor = math.min(#ctx.optionChoices - 1, ctx.optionCursor + 1)
            elseif inp == "click" or inp == "confirm" then
                selected = ctx.optionChoices[ctx.optionCursor + 1]
            elseif tonumber(inp) then
                local n = tonumber(inp)
                if ctx.optionChoices[n] then
                    selected = ctx.optionChoices[n]
                end
            end
        end
    end
    -- Execute selected choice: jump to target
    ctx.optionChoices = nil
    ctx.optionCursor = nil
    ctx.waiting_input = false
    if selected and selected.target then
        return flow.jump(ctx, { target = selected.target, storage = selected.target })
    end
end

--- [selnum] — begin numbered selection block (krkrz: selnum/selend)
--  Each choice numbered 1..N; player presses number key
function kag.selnum(ctx, params)
    ctx.selnumMode = true
    ctx.selnumChoices = {}
    ctx.selnumIdx = 0
    ctx._selSavedX = ctx.textCursorX
    ctx._selSavedY = ctx.textCursorY
end

--- [endselnum] — end numbered selection and wait for numeric input
function kag.endselnum(ctx, params)
    ctx.selnumMode = false
    if not ctx.selnumChoices or #ctx.selnumChoices == 0 then
        ctx.textCursorX = ctx._selSavedX
        ctx.textCursorY = ctx._selSavedY
        return
    end
    -- Display numbered options
    backend.clear_text()
    local optY = ctx._selSavedY or 600
    for i, choice in ipairs(ctx.selnumChoices) do
        backend.render_text(tostring(i) .. ". " .. choice.text, 50, optY)
        optY = optY + (backend.line_height() or 24) + 4
    end
    -- Wait for numeric input
    local selected = nil
    while not selected do
        coroutine.yield()
        if ctx.pendingInput then
            local inp = ctx.pendingInput
            ctx.pendingInput = nil
            local n = tonumber(inp)
            if n and n >= 1 and n <= #ctx.selnumChoices then
                selected = ctx.selnumChoices[n]
            end
        end
    end
    ctx.selnumChoices = nil
    ctx.waiting_input = false
    if selected and selected.target then
        return flow.jump(ctx, { target = selected.target, storage = selected.target })
    end
end

--  System / Variable commands
-- ===========================================================================

--- [save] — save game to slot
function kag.save(ctx, params)
    local slot = tonumber(params.slot or params[1]) or 0
    system.save(slot, ctx)
end

--- [load] — load game from slot (returns "stop" to restart execution)
function kag.load(ctx, params)
    local slot = tonumber(params.slot or params[1]) or 0
    if system.load(slot, ctx) then
        return "stop"
    end
end

--- [saveplace] — quick save to slot 0

﻿--- [reset] — reset all engine state (clear layers, stop audio, clear text)
--  params: (none) — fully reset for new scene
function kag.reset(ctx, params)
    -- Stop all audio
    pcall(function() audio.stop_bgm(0) end)
    pcall(function() audio.stop_voice() end)
    pcall(function() audio.stop_se() end)
    -- Clear layers
    if ctx.layers then
        for name, layer in pairs(ctx.layers) do
            if type(layer) == "table" and name ~= "bg" then
                layer.visible = false
                layer.x = 0
                layer.y = 0
                layer.opacity = 255
                layer.alpha = 1.0
                layer.dirty = true
                if layer.rt and name ~= "bg" then
                    pcall(function() rtt.destroy(layer.rt) end)
                    layer.rt = nil
                end
            end
        end
    end
    -- Clear text state
    ctx.textCursorX = 32
    ctx.textCursorY = 600
    ctx.currentText = nil
    ctx.backlog = {}
    backend.clear_text()
    -- Clear VFX
    pcall(function() vfx.stop_all() end)
    -- Clear callbacks and option state
    ctx._callbacks = {}
    ctx.optionMode = false
    ctx.optionChoices = nil
    ctx.selnumMode = false
    ctx.selnumChoices = nil
    ctx.waiting_input = false
    ctx.pendingInput = nil
    -- Clear particles
    pcall(function() backend.clear_particles() end)
    -- Reset flags
    ctx.ifSkip = false
    ctx.switchActive = false
end

function kag.saveplace(ctx, params)
    system.quick_save(ctx)
end

--- [loadplace] — quick load from slot 0
function kag.loadplace(ctx, params)
    if system.quick_load(ctx) then
        return "stop"
    end
end

--- [savename] — set save slot title
function kag.savename(ctx, params)
    ctx.save_title = params.title or params.text or params[1] or "Untitled"
end

--- [history] / [backlog] — show backlog window (blocking)
function kag.history(ctx, params)
    ctx.waiting_input = true
    system.show_backlog(ctx.backlog or {})
    while ctx.waiting_input do
        coroutine.yield()
    end
end
kag.backlog = kag.history  -- alias

--- [eval] — execute Lua expression
function kag.eval(ctx, params)
    local expr = params.exp or params[1] or ""
    if #expr > 0 then
        local fn, err = load("return " .. expr, "=eval")
        if fn then
            local ok, result = pcall(fn)
            if not ok then
                backend.log("eval error: " .. tostring(result))
            end
        else
            backend.log("eval compile error: " .. tostring(err))
        end
    end
end

--- [emb] — execute embedded Lua code block (the content is collected by tokenizer as embed_lua)
function kag.emb(ctx, params)
    -- [emb]...[/emb] blocks: the embedded code is tokenized as embed_lua tokens
    -- and executed by scheduler. This function handles the inline form.
    local code = params.exp or params[1] or ""
    if #code > 0 then
        local fn, err = load(code, "=emb")
        if fn then
            local ok, runErr = pcall(fn)
            if not ok then
                backend.log("emb error: " .. tostring(runErr))
            end
        end
    end
end

--- [macro] ... [/endmacro] — define macro (handled by tokenizer preprocessor)
function kag.macro(ctx, params)
    -- Macro definitions are collected during tokenizer.parse and stored in ctx.macros
    -- The body between [macro] and [/endmacro] is tokenized as macro_def tokens
    -- This function is a no-op at the kag command level
end
kag.endmacro = function(ctx, params) end  -- close tag for [macro]

--- [erasemacro] — delete a macro definition
function kag.erasemacro(ctx, params)
    local name = params.name or params[1]
    if name and ctx.macros then
        ctx.macros[name] = nil
    end
end

--- [mode] — set engine mode (krkrz: switches between normal/gallery/music modes)
function kag.mode(ctx, params)
    local modeName = params.name or params[1] or "normal"
    ctx.mode = modeName
    backend.log("mode set to: " .. modeName)
end

--- [config] — set engine configuration (Spec [4.3]: auto-notify subsystems)
function kag.config(ctx, params)
    if params.text_speed then
        system.set_config({ text_speed = tonumber(params.text_speed) })
    end
    if params.bgm_volume then
        audio.set_bgm_volume(tonumber(params.bgm_volume) / 100)
    end
    if params.se_volume then
        audio.set_se_volume(tonumber(params.se_volume) / 100)
    end
    if params.voice_volume then
        audio.set_voice_volume(tonumber(params.voice_volume) / 100)
    end
    if params.window_width and params.window_height then
        backend.set_resolution(tonumber(params.window_width), tonumber(params.window_height))
    end
    backend.log("config applied")
end

-- ===========================================================================

﻿-- ===========================================================================
--  Callback system: [callback] [cancel] (krkrz: onKeyDown / onTimer events)
-- ===========================================================================

--- [callback] — register a named callback to be invoked on event
--  params: type(keydown|timer|click), name(label), time(ms for timer), key(keycode)
function kag.callback(ctx, params)
    local cbType = params.type or params[1] or "keydown"
    local target = params.name or params.target or params.label
    ctx._callbacks = ctx._callbacks or {}
    local cb = {
        type = cbType,
        target = target,
        key = params.key,
        time = tonumber(params.time),
        interval = tonumber(params.interval),
        active = true,
        elapsed = 0,
    }
    table.insert(ctx._callbacks, cb)
    -- If timer type, start a timer coroutine
    if cbType == "timer" and cb.time then
        cb._timerCoroutine = coroutine.create(function()
            while cb.active do
                local elapsed = 0
                while elapsed < (cb.time / 1000.0) do
                    elapsed = elapsed + (coroutine.yield() or 0.016)
                end
                if cb.active and target then
                    -- Execute callback target
                    ctx._pendingCallbacks = ctx._pendingCallbacks or {}
                    table.insert(ctx._pendingCallbacks, target)
                end
                if not cb.interval then break end
            end
        end)
         -- Kick the coroutine once to start
        coroutine.resume(cb._timerCoroutine)
    end
end

--- [cancel] — cancel a registered callback by type or name
--  params: type, name, all
function kag.cancel(ctx, params)
    local cancelType = params.type
    local cancelName = params.name
    local cancelAll = params.all
    ctx._callbacks = ctx._callbacks or {}
    for i = #ctx._callbacks, 1, -1 do
        local cb = ctx._callbacks[i]
        local match = true
        if cancelType and cb.type ~= cancelType then match = false end
        if cancelName and cb.target ~= cancelName then match = false end
        if match or cancelAll then
            cb.active = false
            table.remove(ctx._callbacks, i)
        end
    end
end

--  Viewport extension: vp (create / draw / destroy)
-- ===========================================================================

function kag.vp(ctx, params)
    local action = params.action or params[1] or "create"
    if action == "create" then
        local w = tonumber(params.width) or 1280
        local h = tonumber(params.height) or 720
        ctx.viewport = rtt.create(w, h)
    elseif action == "destroy" then
        if ctx.viewport then
            rtt.destroy(ctx.viewport)
            ctx.viewport = nil
        end
    elseif action == "draw" then
        if ctx.viewport then
            local x = tonumber(params.x) or 0
            local y = tonumber(params.y) or 0
            local w = tonumber(params.width) or 1280
            local h = tonumber(params.height) or 720
            rtt.blit(ctx.viewport, x, y, w, h)
        end
    end
end

-- ===========================================================================
--  P1 Extended commands
-- ===========================================================================

--- [skip] — toggle skip mode [P0.1]
function kag.skip(ctx, params)
    local scheduler = require("scheduler")
    scheduler.toggleSkip(ctx)
end

--- [autoread] — toggle auto-read mode [P0.2]
function kag.autoread(ctx, params)
    local scheduler = require("scheduler")
    local speed = params.speed or params.frames
    scheduler.toggleAuto(ctx, speed)
end

--- [saveskip] — save skip state
function kag.saveskip(ctx, params)
    ctx._savedSkip = ctx.skipMode
end

--- [gallery] — open CG gallery [P1.1]
function kag.gallery(ctx, params)
    local Gallery = require("gallery")
    Gallery.show(ctx)
end

--- [musicroom] — open music room [P1.2]
function kag.musicroom(ctx, params)
    local MusicRoom = require("music_room")
    MusicRoom.show(ctx)
end

--- [settings] — open settings menu [P1.4]
function kag.settings(ctx, params)
    local Settings = require("settings")
    Settings.show(ctx)
end

--- [lang] — switch language at runtime [P1.3]
function kag.lang(ctx, params)
    local lang = params.lang or params[1] or "zh"
    local ok, i18n = pcall(require, "i18n")
    if ok then i18n.load(lang) end
end

-- ===========================================================================
--  Export
-- ===========================================================================


-- =========================================================================
-- KAG fallback helpers for backend.lua fallback path (pure Lua mode)
-- These are called by backend.lua when no C++ backend is available.
-- =========================================================================

--- Audio fallback: always false in pure Lua mode
function kag.is_bgm_playing()   return false end
function kag.is_voice_playing() return false end
function kag.is_se_playing()    return false end

--- Audio control fallbacks (delegate to audio.lua)
function kag.set_bus_volume(bus, vol)
    local a = require("audio")
    if bus == "bgm" then a.set_bgm_volume(vol)
    elseif bus == "voice" then a.set_voice_volume(vol)
    elseif bus == "se" then a.set_se_volume(vol) end
end
function kag.get_bus_volume(bus)
    local a = require("audio")
    if bus == "bgm" then return a.get_bgm_volume()
    elseif bus == "voice" then return a.get_voice_volume()
    elseif bus == "se" then return a.get_se_volume() end
    return 0
end
function kag.set_listener(px, py, pz, ax, ay, az)
    local a = require("audio")
    return a.set_listener(px, py, pz, ax, ay, az)
end
function kag.flush_wave_cache() end  -- no-op in pure Lua mode

--- Audio play/stop fallbacks (backend.lua fallback path)
function kag.play_bgm(file, fadein)
    local a = require("audio")
    return a.play_bgm(file, { fadein = fadein })
end
function kag.stop_bgm(fadeout)
    local a = require("audio")
    return a.stop_bgm(fadeout)
end
function kag.play_voice(file)
    local a = require("audio")
    return a.play_voice(file)
end
function kag.stop_voice()
    local a = require("audio")
    return a.stop_voice()
end
function kag.play_se(file)
    local a = require("audio")
    return a.play_se(file)
end
function kag.stop_se()
    local a = require("audio")
    return a.stop_se()
end
function kag.play_se_3d(file, x, y, z)
    local a = require("audio")
    return a.play_se_3d(file, x, y, z)
end

--- Text/UI fallbacks (backend.lua fallback path)
function kag.show_text(text)
    print("[KAG] " .. (text or ""))
end
function kag.clear_text()
    -- no-op in pure Lua mode
end
function kag.render_ruby(text, ruby, x, y)
    print("[KAG Ruby] " .. (text or "") .. " (" .. (ruby or "") .. ")")
end
function kag.set_font(id)
    -- no-op in pure Lua mode; font handled by platform
end
function kag.show_image(file, x, y)
    print("[KAG Image] " .. (file or "") .. " at (" .. tostring(x) .. "," .. tostring(y) .. ")")
end
function kag.clear_screen()
    -- no-op in pure Lua mode
end
function kag.wait_click()
    print("[KAG] wait_click — waiting for input...")
end

return kag
