-- =============================================================================
--  Caesura (AmeKAG) â€?kag/commands/audio.lua
--  KAG audio tag handlers: [playbgm], [stopbgm], [playse], [playvoice],
--  [fadebgm], [xfadebgm]
--  All audio calls route through backend.lua (unified C++ proxy).
--  Voice playback uses coroutine.yield (cooperative multitasking, no polling).
-- =============================================================================

local backend = require("backend")

local AudioCommands = {}

-- Internal: resolve file path (storage > path > file > positional)
local function resolve_file(params)
    return params.storage or params.path or params.file or params[1]
end

-- =============================================================================
--  [playbgm storage="file.ogg" volume=0.8 fadein=2000 loop=true]
--  Load + play on BGM bus with optional fade-in and loop.
-- =============================================================================

-- Next-gen contracts: typed + clamped via kag/schema.
local schema = require("kag.schema")
-- Volume setter family: clamped 0..1.5 like every other volume param
-- (security: no amplification through the set*volume entry points).
schema.define("setbgmvolume", {
    volume = { type = "number", default = 1.0, min = 0, max = 1.5 },
})
schema.define("setsevolume", {
    volume = { type = "number", default = 1.0, min = 0, max = 1.5 },
})
schema.define("setvoicevolume", {
    volume = { type = "number", default = 1.0, min = 0, max = 1.5 },
})
schema.define("playbgmstop", {
    file = { type = "string" },
    volume = { type = "number", default = 1.0, min = 0, max = 1.5 },
    fadeout = { type = "number", default = 0, min = 0, max = 30000 },
    fadein = { type = "number", default = 0, min = 0, max = 30000 },
})
schema.define("playbgm", {
    _require_any = { "file", "storage" },
    file    = { type = "string" },
    storage = { type = "string" },  -- KAG3 alias for file
    volume = { type = "number", default = 1.0, min = 0, max = 1.5 },
    fadein = { type = "number", default = 0, min = 0, max = 30000 },
    loop   = { type = "boolean", default = true },
})
schema.define("playse", {
    _require_any = { "file", "storage" },
    file    = { type = "string" },
    storage = { type = "string" },
    volume = { type = "number", default = 1.0, min = 0, max = 1.5 },
    fadein = { type = "number", default = 0, min = 0, max = 30000 },
})
schema.define("stopbgm", {
    fadeout = { type = "number", default = 0, min = 0, max = 30000 },
})
schema.define("stopse", {
    fadeout = { type = "number", default = 0, min = 0, max = 30000 },
})
schema.define("fadebgm", {
    volume = { type = "number", default = 0, min = 0, max = 1.5 },
    time   = { type = "number", default = 1000, min = 0, max = 30000 },
    fadein = { type = "number", default = 0, min = 0, max = 30000 },
})
schema.define("fadevol", {
    volume = { type = "number", default = 1.0, min = 0, max = 1.5 },
    time   = { type = "number", default = 1000, min = 0, max = 30000 },
})

function AudioCommands.playbgm(ctx, params)
    local file = resolve_file(params)
    if not file then
        print("[AudioCmd] playbgm: no file specified")
        return
    end

    local volume = params.volume  -- schema-typed
    local fadein = params.fadein

    backend.audio_play("bgm", file, {
        fadein = fadein / 1000.0,   -- KAG uses ms, backend uses seconds
        volume = volume,
        loop   = (params.loop ~= false),
    })
end

-- =============================================================================
--  [stopbgm fadeout=2000]
--  Stop BGM with optional fade-out.
-- =============================================================================

function AudioCommands.stopbgm(ctx, params)
    local fadeout = params.fadeout  -- schema-typed

    if fadeout > 0 then
        backend.audio_fade_volume("bgm", 0, fadeout / 1000.0)
        backend.audio_stop("bgm", { fadeout = fadeout / 1000.0 + 0.1 })
    else
        backend.audio_stop("bgm")
    end
end

-- =============================================================================
--  [playbgmstop storage="file.ogg" fadeout=2000 fadein=2000]
--  krkrz KAG: stop current BGM with fadeout, then play new BGM with fadein.
--  This is an alias for xfadebgm.
-- =============================================================================

function AudioCommands.playbgmstop(ctx, params)
    local file = resolve_file(params)
    local fadeout = params.fadeout
    local fadein  = params.fadein

    if fadeout > 0 then
        backend.audio_fade_volume("bgm", 0, fadeout / 1000.0)
        backend.audio_stop("bgm", { fadeout = fadeout / 1000.0 + 0.1 })
    else
        backend.audio_stop("bgm")
    end

    if file then
        backend.audio_play("bgm", file, {
            fadein = fadein / 1000.0,
            volume = params.volume,
        })
    end
end

-- =============================================================================
--  [fadebgm volume=0 time=2000]
--  Fade BGM bus volume to target without stopping playback.
--  KAG time is in milliseconds; backend uses seconds.
-- =============================================================================

function AudioCommands.fadebgm(ctx, params)
    local target = params.volume
    local time   = params.time

    backend.audio_fade_volume("bgm", target, time / 1000.0)
end

-- =============================================================================
--  [xfadebgm storage="file.ogg" time=2000]
--  Cross-fade: fade out current BGM, then start new BGM with fade-in.
-- =============================================================================

-- Next-gen contract: typed crossfade (time clamped).
schema.define("xfadebgm", {
    file  = { type = "string" },
    storage = { type = "string" },
    time  = { type = "number", default = 2000, min = 0, max = 30000 },
})

function AudioCommands.xfadebgm(ctx, params)
    local file  = resolve_file(params)
    local time  = params.time  -- schema-typed

    backend.audio_xfade("bgm", file, time / 1000.0)
end

-- =============================================================================
--  [playse storage="click.wav" volume=0.8]
--  Play sound effect on SE bus â€?fire and forget (no blocking).
-- =============================================================================

function AudioCommands.playse(ctx, params)
    local file = resolve_file(params)
    if not file then
        print("[AudioCmd] playse: no file specified")
        return
    end

    local volume = tonumber(params.volume) or 1.0

    backend.audio_play("se", file, {
        volume = volume,
    })
end

-- =============================================================================
--  [stopse]
--  Stop all currently playing sound effects.
-- =============================================================================

function AudioCommands.stopse(ctx, params)
    backend.audio_stop("se")
end

-- =============================================================================
--  [playvoice storage="line001.ogg"]
--  Play voice line on VOICE bus â€?blocks until complete via coroutine.yield.
--  Each frame, the scheduler resumes and re-checks voice status.
--  When voice finishes (or _CAESURA_AUDIO_EVENT fires), the command returns.
-- =============================================================================

function AudioCommands.playvoice(ctx, params)
    -- [voice_off] mute: skip playback but keep the event flow (the wait
    -- loop still completes instantly -- no stuck dialogue).
    if ctx and ctx.voice_muted then
        _G._CAESURA_AUDIO_EVENT = "voice_end"
        return
    end
    local file = resolve_file(params)
    if not file then
        print("[AudioCmd] playvoice: no file specified")
        return
    end

    -- Clear any stale audio event before starting
    _G._CAESURA_AUDIO_EVENT = nil

    -- Play the voice line
    backend.audio_play("voice", file, {})

    -- Block until voice finishes â€?cooperative yield each frame.
    -- Two exit conditions: SoLoud handle invalid (normal) or C++ edge trigger.
    while backend.audio_is_playing("voice") do
        coroutine.yield()
        -- Check for C++ edge-triggered event (belt-and-suspenders)
        if _G._CAESURA_AUDIO_EVENT == "voice_end" then
            _G._CAESURA_AUDIO_EVENT = nil
            break
        end
    end
    _G._CAESURA_AUDIO_EVENT = nil
end

-- =============================================================================
--  [stopvoice]
--  Immediately stop the current voice line.
-- =============================================================================

function AudioCommands.stopvoice(ctx, params)
    backend.audio_stop("voice")
    -- Signal the voice_end edge-trigger so a pending playvoice wait loop
    -- (which polls _CAESURA_AUDIO_EVENT) unblocks immediately instead of
    -- spinning until SoLoud reports the handle invalid.
    _G._CAESURA_AUDIO_EVENT = "voice_end"
end

-- =============================================================================
--  [waitsound]
--  Block until all SE on the SE bus have finished playing.
-- =============================================================================

function AudioCommands.waitsound(ctx, params)
    while backend.audio_is_playing("se") do
        coroutine.yield()
    end
end

-- =============================================================================
--  [waitbgm]
--  Block until the current BGM finishes (for non-looping tracks).
-- =============================================================================

function AudioCommands.waitbgm(ctx, params)
    while backend.audio_is_playing("bgm") do
        coroutine.yield()
    end
end

-- =============================================================================
--  [setbgmvolume volume=0.8] / [setsevolume volume=0.5] / [setvoicevolume v=1.0]
-- =============================================================================

function AudioCommands.setbgmvolume(ctx, params)
    local vol = tonumber(params.volume) or tonumber(params[1]) or 1.0
    backend.audio_set_bus_volume("bgm", vol)
end

function AudioCommands.setsevolume(ctx, params)
    local vol = tonumber(params.volume) or tonumber(params[1]) or 1.0
    backend.audio_set_bus_volume("se", vol)
end

function AudioCommands.setvoicevolume(ctx, params)
    local vol = tonumber(params.volume) or tonumber(params[1]) or 1.0
    backend.audio_set_bus_volume("voice", vol)
end

return AudioCommands