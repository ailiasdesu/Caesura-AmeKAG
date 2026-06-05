-- =============================================================================
--  Caesura Debug API  Lua-side wrapper
--  Provides friendly wrappers around the C++ Debug.* bindings.
--  All calls are safe when CAESURA_DEBUG is off (they return nil/empty).
-- =============================================================================

local DebugModule = {}

-- Check if Debug C module is available
function DebugModule.is_available()
    return rawget(_G, "Debug") ~= nil
end

-- Shorthand for calling a C Debug function
local function call(fn, ...)
    if not DebugModule.is_available() then return nil end
    local ok, result = pcall(_G.Debug[fn], ...)
    if not ok then
        print("[debug_api] Debug." .. fn .. " failed: " .. tostring(result))
        return nil
    end
    return result
end

-- =========================================================================
--  Public API
-- =========================================================================

-- Returns the last error entry as a table, or nil
function DebugModule.last_error()
    return call("get_last_error")
end

-- Returns total error count since engine startup
function DebugModule.error_count()
    return call("get_error_count") or 0
end

-- Returns stats for a subsystem: "render", "audio", "scripting", "input", "platform", "engine"
function DebugModule.subsystem_stats(name)
    return call("get_subsystem_stats", name)
end

-- Writes a full JSON diagnostic report to logs/ and returns the JSON string
function DebugModule.dump_report()
    return call("dump_report")
end

-- Returns render info table: { backend, width, height, views, textures, rtts, shader_ready }
function DebugModule.render_info()
    return call("get_render_info")
end

-- Returns audio info table: { initialized, active_voices, active_bgm, global_volume, ... }
function DebugModule.audio_info()
    return call("get_audio_info")
end

-- Returns input info table: { focus, kag_callbacks, game_callbacks, click_pending }
function DebugModule.input_info()
    return call("get_input_info")
end

-- Returns the log file path
function DebugModule.log_path()
    return call("get_log_path")
end

-- =========================================================================
--  Convenience: print a full status summary to console
-- =========================================================================
function DebugModule.print_status()
    local render  = DebugModule.render_info()
    local audio   = DebugModule.audio_info()
    local input   = DebugModule.input_info()
    local errors  = DebugModule.error_count()

    print("========================================")
    print("  Caesura (AmeKAG) Debug Status")
    print("========================================")
    print(string.format("  Errors: %d", errors))

    if render then
        print(string.format("  Render: %s %dx%d, shader=%s",
            render.backend or "?", render.width or 0, render.height or 0,
            render.shader_ready and "OK" or "FAIL"))
    end
    if audio then
        print(string.format("  Audio: init=%s, voices=%d, bgm=%d, vol=%.2f",
            audio.initialized and "OK" or "FAIL",
            audio.active_voices or 0, audio.active_bgm or 0,
            audio.global_volume or 0))
    end
    if input then
        print(string.format("  Input: focus=%s, kagCb=%d, gameCb=%d, click=%s",
            input.focus or "?", input.kag_callbacks or 0,
            input.game_callbacks or 0,
            input.click_pending and "Y" or "N"))
    end

    local log = DebugModule.log_path()
    if log then
        print(string.format("  Log: %s", log))
    end
    print("========================================")
end

-- =========================================================================
--  Expose on global for easy REPL access
-- =========================================================================
_G.DEBUG = DebugModule

return DebugModule
