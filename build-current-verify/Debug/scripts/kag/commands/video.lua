-- =============================================================================
--  Caesura (AmeKAG) ¡ª kag/commands/video.lua
--  Phase 4: KAG video tag handlers ¡ª [video], [stopvideo]
--  Delegates to the C++ pl_mpeg video player via backend.
--  Spec [5.1]: PTS audio sync, click-to-skip, CancelToken support.
-- =============================================================================

local CancelToken = require("kag.cancel_token")
local backend     = require("backend")

local VideoCommands = {}

local function resolve_file(params)
    return params.storage or params.path or params.file or params[1]
end

-- ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
--  [video storage="opening.mpg" loop=false volume=1.0]
--  Play a video file. Blocks coroutine until video ends or user clicks.
--  Spec [10.2.2]: PTS sync via SoLoud audio position.
--  Click during video triggers CancelToken ¡ú stop video ¡ú resume script.
-- ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T

function VideoCommands.video(ctx, params)
    local file   = resolve_file(params)
    local loop   = params.loop
    if loop == nil or loop == "false" then loop = false
    elseif loop == "true" then loop = true end
    local volume = tonumber(params.volume) or 1.0

    if not file then
        print("[VideoCmd] video: no file specified")
        return
    end

    -- Create cancel token for user skip (click/space) and [jump]/[link]
    local ct = CancelToken.new()
    ctx.active_operations = ctx.active_operations or {}
    table.insert(ctx.active_operations, ct)

    -- Start video playback via backend
    local ok = backend.video_play and backend.video_play(file, {
        loop   = loop,
        volume = volume,
    })
    if not ok then
        print("[VideoCmd] video: failed to play " .. file)
        return
    end

    -- Block until video ends or user cancels
    while backend.video_is_playing and backend.video_is_playing() and not ct.cancelled do
        coroutine.yield()
    end

    -- Cleanup: stop video and free decoder resources
    if backend.video_stop then
        backend.video_stop()
    end

    for i = #ctx.active_operations, 1, -1 do
        if ctx.active_operations[i] == ct then
            table.remove(ctx.active_operations, i)
            break
        end
    end
end

-- ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
--  [stopvideo]
--  Immediately stop video playback and free decoder resources.
--  Used mid-scene or before [jump]/[link].
-- ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T

function VideoCommands.stopvideo(ctx, params)
    if backend.video_stop then
        backend.video_stop()
    end
end

return VideoCommands