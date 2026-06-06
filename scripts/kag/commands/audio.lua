-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/audio.lua
--  KAG audio tag handlers: [playbgm], [stopbgm], [playse], [playvoice],
--  [fadebgm], [xfadebgm]
--  All audio calls route through backend.lua (unified C++ proxy).
--  Voice playback uses coroutine.yield (cooperative multitasking, no polling).
-- =============================================================================

local backend = require("backend")

local AudioCommands = {}

-- =============================================================================
--  [playbgm storage="file.ogg" volume=0.8 fadein=2000 loop=true]
--  Load + play on BGM bus with optional fade-in and loop.
-- =============================================================================

function AudioCommands.playbgm(ctx, params)
    local file = params.storage or params.file or params[1]
    if not file then
        print("[AudioCmd] playbgm: no file specified")
        return
    end

    local volume = tonumber(params.volume) or 1.0
    local fadein = tonumber(params.fadein) or 0

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
    local fadeout = tonumber(params.fadeout) or 0

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
    local file = params.storage or params.file or params[1]
    local fadeout = tonumber(params.fadeout) or 0
    local fadein  = tonumber(params.fadein)  or 0

    if fadeout > 0 then
        backend.audio_fade_volume("bgm", 0, fadeout / 1000.0)
        backend.audio_stop("bgm", { fadeout = fadeout / 1000.0 + 0.1 })
    else
        backend.audio_stop("bgm")
    end

    if file then
        backend.audio_play("bgm", file, {
            fadein = fadein / 1000.0,
            volume = tonumber(params.volume) or 1.0,
        })
    end
end

-- =============================================================================
--  [fadebgm volume=0 time=2000]
--  Fade BGM bus volume to target without stopping playback.
--  KAG time is in milliseconds; backend uses seconds.
-- =============================================================================

function AudioCommands.fadebgm(ctx, params)
    local target = tonumber(params.volume) or 1.0
    local time   = tonumber(params.time)   or 1000

    backend.audio_fade_volume("bgm", target, time / 1000.0)
end

-- =============================================================================
--  [xfadebgm storage="file.ogg" time=2000]
--  Cross-fade: fade out current BGM, then start new BGM with fade-in.
-- =============================================================================

function AudioCommands.xfadebgm(ctx, params)
    local file  = params.storage or params.file or params[1]
    local time  = tonumber(params.time) or 2000

    backend.audio_xfade("bgm", file, time / 1000.0)
end

-- =============================================================================
--  [playse storage="click.wav" volume=0.8]
--  Play sound effect on SE bus — fire and forget (no blocking).
-- =============================================================================

function AudioCommands.playse(ctx, params)
    local file = params.storage or params.file or params[1]
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
--  Play voice line on VOICE bus — blocks until complete via coroutine.yield.
--  Each frame, the scheduler resumes and re-checks voice status.
--  When voice finishes (or _CAESURA_AUDIO_EVENT fires), the command returns.
-- =============================================================================

function AudioCommands.playvoice(ctx, params)
    local file = params.storage or params.file or params[1]
    if not file then
        print("[AudioCmd] playvoice: no file specified")
        return
    end

    -- Clear any stale audio event before starting
    _G._CAESURA_AUDIO_EVENT = nil

    -- Play the voice line
    backend.audio_play("voice", file, {})

    -- Block until voice finishes — cooperative yield each frame.
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