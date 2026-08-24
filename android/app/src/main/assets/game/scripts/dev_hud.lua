-- ===========================================================================
--  dev_hud.lua — Developer HUD (differentiator: built-in perf overlay)
--  Toggle with F4 (dispatched by the engine, allowlisted). Renders a
--  corner overlay with frame time, FPS, GPU avg ms, Lua memory and the
--  engine version, using the layer system (settings.lua pattern).
--  Zero C++ changes: consumes engine globals the engine already publishes
--  (_CAESURA_GPU_AVG_MS, _CAESURA_GPU_DEGRADED) + os.clock/collectgarbage.
-- ===========================================================================

local DevHUD = {}
local backend = require("backend")
local layers = require("layers")

local state = {
    visible = false,
    lastToggle = 0,
}

local function solid(r, g, b, a)
    return backend.create_solid_texture(math.floor(r), math.floor(g), math.floor(b), math.floor(a or 255))
end

-- Frame-time smoothing (EMA)
local emaFrameMs = 16.0
local lastTick = os.clock()

--- DevHUD.toggle()
function DevHUD.toggle()
    state.visible = not state.visible
    if not state.visible then
        local bg = layers.get_layer("_hud_bg")
        if bg then bg.visible = false end
    end
end

--- DevHUD.isVisible()
function DevHUD.isVisible()
    return state.visible
end

--- DevHUD.update(dtMs) — called per frame when visible
function DevHUD.update(dtMs)
    if not state.visible then return end
    local bg = layers.ensure(ctx and ctx or _G._CAESURA_CTX, "_hud_bg", 200)
    bg.visible = true
    bg.x, bg.y = 0, 0
    bg.w, bg.h = 300, 88
    bg.texture = solid(0, 0, 0, 180)

    -- EMA frame ms
    if dtMs and dtMs > 0 then
        emaFrameMs = emaFrameMs * 0.9 + dtMs * 0.1
    end
    local fps = emaFrameMs > 0 and math.floor(1000 / emaFrameMs) or 0

    local gpuMs = _G._CAESURA_GPU_AVG_MS or 0
    local degraded = _G._CAESURA_GPU_DEGRADED and "YES" or "no"
    local memKB = collectgarbage("count")
    local engineVer = "Caesura (AmeKAG)"

    local lines = {
        engineVer,
        string.format("Frame %5.1f ms  FPS %d", emaFrameMs, fps),
        string.format("GPU %5.1f ms  degraded %s", gpuMs, degraded),
        string.format("Lua mem %6.1f KB", memKB),
    }
    local y = 6
    for _, line in ipairs(lines) do
        backend.render_text(line, 10, y, 120, 255, 120, 255)
        y = y + 20
    end
end

return DevHUD
