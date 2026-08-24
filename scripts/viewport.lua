-- viewport.lua - logical-resolution helpers shared by KAG scene/UI layout.
--
-- "Logical resolution" is the engine render buffer (config.window_width x
-- config.window_height, default 1920x1080), NOT the physical display (the
-- Android OS surface can differ, e.g. 2320x956; the renderer scales the
-- logical scene to the display). All scene-layer and UI defaults must be
-- expressed in logical coordinates so content fills the buffer at any
-- resolution (1920x1080 by default, adjustable via config / --resolution).
--
-- Dependency-free (only pcall-guarded backend access) so both the engine
-- sandbox and the standalone test runner can require it from anywhere.
-- Fallback keeps the historic 1280x720 layouts working if a mock/call
-- error occurs.

local M = {}

-- current logical resolution, or the provided fallback
function M.logical(fallback_w, fallback_h)
    local ok, backend = pcall(require, "backend")
    if ok and type(backend) == "table" and type(backend.get_resolution) == "function" then
        local ok2, w, h = pcall(backend.get_resolution)
        if ok2 and type(w) == "number" and type(h) == "number" and w > 0 and h > 0 then
            return math.floor(w), math.floor(h)
        end
    end
    return fallback_w or 1920, fallback_h or 1080
end

-- viewport width/height with 1920x1080 default
function M.wh()
    return M.logical(1920, 1080)
end

-- scale a 1280x720-reference coordinate horizontally (x_ref at 1280)
function M.x(x_ref)
    local w = M.logical(1920, 1080)
    return math.floor((x_ref / 1280) * w + 0.5)
end

-- scale a 1280x720-reference coordinate vertically (y_ref at 720)
function M.y(y_ref)
    local _, h = M.logical(1920, 1080)
    return math.floor((y_ref / 720) * h + 0.5)
end

return M
