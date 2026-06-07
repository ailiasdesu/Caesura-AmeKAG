-- ═══════════════════════════════════════════════════════════════════════
--  Caesura (AmeKAG) — rtt.lua
--  Lua-side Render-To-Texture wrapper around C++ Render bindings.
--  Manages viewport handles, blitting, and lifecycle.
-- ═══════════════════════════════════════════════════════════════════════

local backend = require("backend")

local RTT = {}

-- ═══════════════════════════════════════════════════════════════════════
-- RTT.create(width, height) → handleId
-- ═══════════════════════════════════════════════════════════════════════

function RTT.create(width, height)
    return backend.create_viewport(width or 1280, height or 720)
end

-- ═══════════════════════════════════════════════════════════════════════
-- RTT.destroy(handleId)
-- ═══════════════════════════════════════════════════════════════════════

function RTT.destroy(handleId)
    backend.destroy_viewport(handleId)
end

-- ═══════════════════════════════════════════════════════════════════════
-- RTT.blit(handleId, x, y, w, h) → draw RTT texture to VIEW_MAIN
-- ═══════════════════════════════════════════════════════════════════════

function RTT.fill(handleId, r, g, b, a)
    return backend.fill_viewport(handleId, r or 100, g or 100, b or 200, a or 255)
end

function RTT.blit(handleId, x, y, w, h)
    backend.draw_viewport(handleId, x or 0, y or 0, w or 1280, h or 720)
end

-- ═══════════════════════════════════════════════════════════════════════
-- RTT.get_resolution() → width, height
-- ═══════════════════════════════════════════════════════════════════════

function RTT.get_resolution()
    return backend.get_resolution()
end

return RTT
