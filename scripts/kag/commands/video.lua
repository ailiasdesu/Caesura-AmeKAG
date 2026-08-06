-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/video.lua
--  Phase 4: KAG video tag handlers — [video], [stopvideo]
--  Delegates to the C++ pl_mpeg video player via backend.
--  Spec [5.1]: PTS audio sync, click-to-skip, CancelToken support.
-- =============================================================================

local Operation   = require("kag.operation")
local backend     = require("backend")

local VideoCommands = {}

local function resolve_file(params)
    return params.storage or params.path or params.file or params[1]
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [video storage="opening.mpg" loop=false volume=1.0]
--  Play a video file. Blocks coroutine until video ends or user clicks.
--  Spec [10.2.2]: PTS sync via SoLoud audio position.
--  Click during video triggers CancelToken → stop video → resume script.
-- ═══════════════════════════════════════════════════════════════════════════

-- Neo-Genesis contract: typed + clamped via kag/schema.
require("kag.schema").define("video", {
    file = { type = "string", required = true },
    volume = { type = "number", default = 1.0, min = 0, max = 1.5 },
    loop = { type = "boolean", default = false },
    x = { type = "number", default = 0 },
    y = { type = "number", default = 0 },
    w = { type = "number", default = 0, min = 0, max = 8192 },
    h = { type = "number", default = 0, min = 0, max = 8192 },
})

function VideoCommands.video(ctx, params)
    local file   = resolve_file(params)
    -- schema already coerces loop to a boolean (the string forms below
    -- were dead after the schema contract landed -- audit cleanup)
    local loop   = params.loop == true
    local volume = params.volume  -- schema-typed

    if not file then
        print("[VideoCmd] video: no file specified")
        return
    end

    local operation <close> = Operation.start(ctx)
    local ct = operation.token
    local function stop_video()
        if backend.video_stop then
            backend.video_stop()
        end
    end
    ct:register(stop_video)

    -- Start video playback via backend
    local ok = backend.video_play and backend.video_play(file, {
        loop   = loop,
        volume = volume,
    })
    if not ok then
        print("[VideoCmd] video: failed to play " .. file)
        return
    end

    -- Block until video ends, user cancels, or the 60s cap (a stuck
    -- decoder must not hang the runner -- same bound as waitsound).
    -- loop=true videos therefore auto-stop after the cap; scripts that
    -- need longer loops should re-issue [video] or use [stopvideo].
    local elapsed = 0
    while backend.video_is_playing and backend.video_is_playing()
          and not ct.cancelled and elapsed < 60000 do
        elapsed = elapsed + (coroutine.yield() or 16)
    end

    -- Cleanup: stop video and free decoder resources
    stop_video()
    if not ct.cancelled then
        operation:complete()
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [stopvideo]
--  Immediately stop video playback and free decoder resources.
--  Used mid-scene or before [jump]/[link].
-- ═══════════════════════════════════════════════════════════════════════════

function VideoCommands.stopvideo(ctx, params)
    if backend.video_stop then
        backend.video_stop()
    end
end

return VideoCommands
