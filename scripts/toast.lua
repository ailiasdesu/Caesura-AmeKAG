-- ===========================================================================
--  toast.lua — lightweight in-game notification (VN standard feedback)
--  Shows a brief corner message (e.g. "已保存" after F5 quick save) using
--  the layer system. TTL-based: fades out automatically; no user input.
-- ===========================================================================

local Toast = {}
local backend = require("backend")
local layers = require("layers")

local state = { msg = nil, ttl = 0, maxTtl = 2.0 }

local function solid(r, g, b, a)
    return backend.create_solid_texture(math.floor(r), math.floor(g), math.floor(b), math.floor(a or 255))
end

--- Toast.show(msg, ttlSeconds)
function Toast.show(msg, ttl)
    state.msg = msg
    state.maxTtl = ttl or 2.0
    state.ttl = state.maxTtl
end

--- Toast.update(dtSeconds) — called per frame
function Toast.update(dt)
    if not state.msg or state.ttl <= 0 then
        if state.msg then
            state.msg = nil
            local bg = layers.get_layer("_toast_bg")
            if bg then bg.visible = false end
        end
        return
    end
    state.ttl = state.ttl - (dt or 0.016)
    local ctx = _G._CAESURA_CTX
    local bg = layers.ensure(ctx, "_toast_bg", 201)
    bg.visible = true
    bg.x, bg.y = 1280 - 260, 720 - 44
    bg.w, bg.h = 250, 36
    bg.texture = solid(10, 10, 30, 210)
    backend.render_text(state.msg, 1280 - 250 + 12, 720 - 44 + 8, 140, 220, 140, 255)
end

--- Toast.isVisible()
function Toast.isVisible()
    return state.msg ~= nil and state.ttl > 0
end

return Toast
