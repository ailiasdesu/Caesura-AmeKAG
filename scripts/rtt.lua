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

-- ─── Pooled lifecycle (perf: layer add/remove/resize no longer allocates
-- ─── GPU viewports every time; same-size layers reuse handles). ───────────
local pool = {}  -- "WxH" -> { handle, ... }

function RTT.acquire(width, height)
    width, height = width or 1280, height or 720
    local key = width .. "x" .. height
    local bucket = pool[key]
    if bucket and #bucket > 0 then
        return table.remove(bucket)
    end
    return RTT.create(width, height)
end

function RTT.release(handle)
    if not handle then return end
    -- Return to pool (bounded: keep at most 8 per size).
    local w, h = backend.get_viewport_size and backend.get_viewport_size(handle)
    local key = (w or 1280) .. "x" .. (h or 720)
    local bucket = pool[key] or {}
    if #bucket < 8 then
        bucket[#bucket + 1] = handle
        pool[key] = bucket
    else
        RTT.destroy(handle)
    end
end

function RTT.poolSize()
    local n = 0
    for _, b in pairs(pool) do n = n + #b end
    return n
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
