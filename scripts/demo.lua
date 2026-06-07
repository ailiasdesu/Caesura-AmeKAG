local layers  = require("layers")
local backend = require("backend")

local frame = 0
local w, h = backend.get_resolution()
if not w then w, h = 1280, 720 end

local C = { bgDark = {8,12,28,255}, white = {240,240,250,255} }

local function draw_text(x, y, text, color)
    backend.render_text(text, x, y, 1.0,
        color[1]/255, color[2]/255, color[3]/255, color[4]/255)
end

function engine_render()
end

function engine_update(dt)
    frame = frame + 1
    if frame <= 3 then
        draw_text(100, 100, "Test frame " .. frame, C.white)
        print("[TEST] Frame " .. frame .. " - draw_text called")
    end
    if frame % 60 == 0 then
        print("[TEST] Frame " .. frame .. " OK")
    end
end

function _KAG_onClick(x, y) end
function _KAG_onKey(key, mods) end

print("[TEST] Loaded. Testing render_text path.")
