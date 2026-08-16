-- test_settings_layout_pilot.lua — coordinate-equivalence check for the
-- R107-A [layout] pilot in settings.lua (the 3-bus volume slider band).
-- Confirms the declarative vbox _volumeBand resolves to EXACTLY the same
-- row x/y as the historic hand-rolled formula for a given panel geometry.
-- Standalone / order-safe: seeds mock backend+layers, no file IO.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- Mock backend under _G._CAESURA_BACKEND (settings calls backend.* via require)
_G._CAESURA_BACKEND = {
    render = function() return true end,
    platform = function(cmd)
        if cmd == "get_resolution" then return 1280, 720 end
        if cmd == "set_input_focus" then return true end
        return true
    end,
    create_solid_texture = function() return { _mock = true } end,
    destroy_texture = function() return true end,
    render_text = function() end,
}
package.loaded["layers"] = {
    ensure = function() return { visible = true } end,
    find = function() return nil end,
}
package.loaded["audio"] = { set_bgm_volume = function() return true end,
                            set_se_volume = function() return true end,
                            set_voice_volume = function() return true end }
package.loaded["i18n"] = { current = "zh", t = function(k) return k end,
                           available = function() return { "zh", "en" } end,
                           load = function() return true end }

local Settings = require("settings")

-- Drive show() to populate state.panel* then capture _volumeBand rows.
local ctx = { f = {}, sf = {}, tf = {}, mp = {}, variables = {}, settingsValues = {} }
local co = coroutine.create(function() Settings.show(ctx) end)
coroutine.resume(co)   -- first resume runs show() up to the input loop

-- After show()'s first resume, state.panelX/Y/W/H are set, and calling
-- _volumeBand must yield the historic coordinates.
local geo = Settings._rowGeometry(ctx)
local band = Settings._volumeBand(ctx)

-- show() centers the panel: panelW=680, panelH=420 at 1280x720 ->
-- panelX=(1280-680)/2=300, panelY=(720-420)/2=150; startY=150+50=200,
-- lineH=30; row x = panelX+30 = 330.
local expectedY = { 200, 230, 260 }   -- startY = 200; +30 increments
local expectedX = 330                 -- px+30 = 300+30
check("volume row1 y == 200", geo.rowY(1) == expectedY[1])
check("volume row2 y == 230", geo.rowY(2) == expectedY[2])
check("volume row3 y == 260", geo.rowY(3) == expectedY[3])
check("volume row1 x == 330", geo.rowX(1) == expectedX)
check("volume row2 x == 330", geo.rowX(2) == expectedX)
check("volume row3 x == 330", geo.rowX(3) == expectedX)
-- non-volume row (index 4) still uses the historic formula (unchanged)
check("row4 y == startY+90", geo.rowY(4) == 200 + 3 * 30)
-- row rects carry the slot w/h reported by the layout calculator.
-- The live panel is panelW=680 centered, so bandW = 680-100 = 580.
check("row1 w == bandW (= 680-100=580)", band.row1 and band.row1.w == 580)
check("row1 h == 30 (lineH)", band.row1 and band.row1.h == 30)
-- row1 x is the container origin (px+30=330), row1 y is the container origin y.
check("row1 x/y origin", band.row1 and band.row1.x == 330 and band.row1.y == 200)

-- ESC closes
coroutine.resume(co)

print(string.format("SETTINGS LAYOUT PILOT TESTS DONE: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end