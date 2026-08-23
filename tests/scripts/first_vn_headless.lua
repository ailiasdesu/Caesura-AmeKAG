-- =============================================================================
--  Caesura (AmeKAG) — first_vn_headless.lua
--  Headless driver for the First-VN E2E project (tests/projects/first_vn/).
--
--  Copy of sample_game_headless.lua with two additions (task book §6):
--    1. FIRST_VN_CHOICE env — 1-based index of the [select] option the
--       auto-clicker picks, so the gate can drive BOTH branches (choice A /
--       choice B) instead of always option 1.
--    2. After the run it prints "ROUTE <value>" — the game variable f.route
--       each branch sets via [set var="f.route" value=...], proving which
--       branch actually executed.
--
--  This is a DRIVER, not a unit test: run_lua_tests.lua does not register
--  it. Mock semantics are identical to sample_game_headless.lua (in
--  particular KAG.load_game returns a non-table, so [load] takes its
--  graceful-miss path and the story continues).
--
--  Usage (from repo root):
--    FIRST_VN_CHOICE=2 external/lua/lua.exe tests/scripts/first_vn_headless.lua
--
--  Env knobs:
--    SAMPLE_STORY    .ks path (default tests/projects/first_vn/story.ks)
--    SAMPLE_ENDING   optional label jump at boot (reachability probe)
--    SAMPLE_FRAMES   frame budget (default 200000)
--    FIRST_VN_CHOICE 1-based select-option index (default: first option)
--
--  Exit: 0 = reached [end] (DONE), 1 = frame limit / fatal, 2 = target label
--  not found.
-- =============================================================================

package.path = "scripts/?.lua;scripts/?/init.lua;scripts/kag/?.lua;scripts/kag/commands/?.lua;" .. package.path

-- ---- Mock C++ bindings (identical to sample_game_headless.lua) ----
local function callable(t)
    return setmetatable(t or {}, {
        __index = function(self, key)
            if type(key) ~= "string" then return nil end
            rawset(self, key, function(...) return true end)
            return self[key]
        end,
    })
end

_G.KAG = callable({
    is_voice_playing  = function() return false end,
    is_bgm_playing    = function() return false end,
    get_active_voices = function() return 0 end,
})
_G.Render  = callable({})
_G.DevCore = callable({})
_G.Engine  = callable({})
_G.backend = callable({
    is_voice_playing  = function() return false end,
    is_bgm_playing    = function() return false end,
    get_active_voices = function() return 0 end,
})

local kag_runner = require("kag_runner")

local STORY  = os.getenv("SAMPLE_STORY")  or "tests/projects/first_vn/story.ks"
local ENDING = os.getenv("SAMPLE_ENDING")
local FMAX   = tonumber(os.getenv("SAMPLE_FRAMES")) or 200000
local CHOICE = tonumber(os.getenv("FIRST_VN_CHOICE") or "")

local started, err = kag_runner.start(STORY)
if not started then
    print("FATAL_START: " .. tostring(err))
    os.exit(1)
end

if ENDING and ENDING ~= "" then
    local ctx    = _G._CAESURA_CTX
    local target = ENDING:sub(1, 1) == "*" and ENDING or ("*" .. ENDING)
    if not kag_runner.stage_label_jump(ctx, target) then
        print("ENDING_NOT_FOUND: " .. target)
        os.exit(2)
    end
    ctx.stop_flag = true
    print("ENDING_JUMP: " .. target)
end

-- Auto-click; when FIRST_VN_CHOICE is set, pick that option (1-based).
local function drive_click()
    local ctx = _G._CAESURA_CTX
    if ctx and ctx._choiceMode then
        _G._GAME_MOUSE_X = 100
        local cbs = ctx._choiceButtonsActive or ctx._choiceButtons
        local idx = 1
        if CHOICE and cbs and #cbs > 0 then
            idx = CHOICE < 1 and 1 or (CHOICE > #cbs and #cbs or CHOICE)
        end
        local ch = cbs and cbs[idx]
        _G._GAME_MOUSE_Y = (ch and tonumber(ch.y) or 450)
                         + (ch and tonumber(ch.h) or 24) / 2
        local kc = _G._KAG_onClick
        if type(kc) == "function" then kc() end
        -- Belt-and-suspenders: force the chosen option if the hit-test missed
        if ctx._choiceMode and type(cbs) == "table" and cbs[idx] then
            ctx._selectedChoice      = cbs[idx]
            ctx._choiceMode          = false
            ctx._choiceButtonsActive = nil
            ctx.waiting_input        = false
        end
        return
    end
    kag_runner.on_click()
end

local frames, clicks = 0, 0
local result = nil
while frames < FMAX do
    frames = frames + 1
    local ok, reason = kag_runner.update(0.016)
    local ctx = _G._CAESURA_CTX
    if reason == "ended" then
        result = "DONE:" .. frames
        break
    end
    if ctx and ctx.tokens and ctx.token_index
       and ctx.token_index > #ctx.tokens then
        result = "DONE(exhausted):" .. frames
        break
    end
    if ctx and (ctx.waiting_input or ctx._choiceMode) then
        clicks = clicks + 1
        drive_click()
    end
end
if not result then result = "FRAME_LIMIT" end

local ctx = _G._CAESURA_CTX
print("RESULT " .. result
      .. " token=" .. tostring(ctx.token_index)
      .. " clicks=" .. clicks
      .. " scene=" .. tostring(ctx.current_scene))

-- Which branch executed? f.route is set by [set var="f.route"] per branch.
local route = (ctx and ctx.f and ctx.f.route) or "none"
print("ROUTE " .. tostring(route))

os.exit(result:sub(1, 4) == "DONE" and 0 or 1)
