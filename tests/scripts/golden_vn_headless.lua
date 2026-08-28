-- =============================================================================
--  Caesura (AmeKAG) — golden_vn_headless.lua
--  Headless driver for the Golden Project v1 fixture (tests/projects/golden_vn/),
--  modeled on first_vn_headless.lua with three v1 additions:
--    1. GOLDEN_ROUTE env — 1-based index of the [select] option the
--       auto-clicker picks (route A = forest / route B = city), so BOTH
--       branches are driven end-to-end.
--    2. Cross-scene [jump] seam for GOLDEN_CROSS=1 — the scheduler resolves a
--       non-* storage target as assets/script/<target>.ks (scripts/scheduler.lua
--       is_safe_scene_path); the engine loads scenes from that logical runtime
--       location, but this fixture keeps its sources under
--       tests/projects/golden_vn/. So the driver remaps the logical name to the
--       fixture file BEFORE any jump executes (the ctx.load_tokens callback is
--       read at jump time). The remap is the ONLY seam: the scheduler's
--       cross-scene machinery (path building, safety check, switch budget,
--       token/label swap, fresh local frame) runs for real. Cross-scene runs
--       use the dedicated golden_cross.ks starter so the scheduler's deferred
--       choice pending-jump (consumed at the next coroutine death) never
--       overlaps the scene switch.
--    3. Feature flags after the run: EVAL_OK / LOAD_MISS_OK / MACRO_OK /
--       XSCENE_OK plus a FLAGS line with f.energy, f.after_load, f.sceneB,
--       f.crossFallback, f.savedByStory, f.macroUsed — gates grep these.
--
--  This is a DRIVER, not a unit test: run_lua_tests.lua does not register
--  it. Mock semantics are identical to sample_game_headless.lua (in
--  particular KAG.load_game returns a non-table, so [load slot=99] takes its
--  graceful-miss path and the story continues past it).
--
--  Usage (from repo root):
--    GOLDEN_ROUTE=1 build/lua/Debug/lua.exe tests/scripts/golden_vn_headless.lua
--    GOLDEN_ROUTE=2 build/lua/Debug/lua.exe tests/scripts/golden_vn_headless.lua
--    GOLDEN_CROSS=1   build/lua/Debug/lua.exe tests/scripts/golden_vn_headless.lua
--
--  Env knobs:
--    SAMPLE_STORY      .ks path (default story.ks, or golden_cross.ks with
--                      GOLDEN_CROSS=1)
--    SAMPLE_ENDING     optional label jump at boot (reachability probe)
--    SAMPLE_FRAMES     frame budget (default 200000)
--    GOLDEN_ROUTE      1-based select-option index (default: first option)
--    GOLDEN_CROSS      1 = drive tests/projects/golden_vn/golden_cross.ks
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

local CROSS  = os.getenv("GOLDEN_CROSS") == "1"
local STORY  = os.getenv("SAMPLE_STORY")
    or (CROSS and "tests/projects/golden_vn/golden_cross.ks"
                  or "tests/projects/golden_vn/story.ks")
local ENDING = os.getenv("SAMPLE_ENDING")
local FMAX   = tonumber(os.getenv("SAMPLE_FRAMES")) or 200000
local CHOICE = tonumber(os.getenv("GOLDEN_ROUTE") or "")

local started, err = kag_runner.start(STORY)
if not started then
    print("FATAL_START: " .. tostring(err))
    os.exit(1)
end

-- ---- Cross-scene [jump] seam (see header comment) --------------------------
-- Read by scheduler at jump time; the logical runtime name of the v1 scene B
-- is assets/script/golden_scene_b.ks (golden_cross.ks jumps there), which
-- maps to the fixture source file.
local ctx0 = _G._CAESURA_CTX
if CROSS then
    local real_load = ctx0.load_tokens
    ctx0.load_tokens = function(path)
        if path == "assets/script/golden_scene_b.ks" then
            print("XSCENE_REMAP " .. path .. " -> tests/projects/golden_vn/scene_b.ks")
            return real_load("tests/projects/golden_vn/scene_b.ks")
        end
        return real_load(path)
    end
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

-- Auto-click; when GOLDEN_ROUTE is set, pick that option (1-based).
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

-- ---- v1 feature flags ------------------------------------------------------
local f = ctx and ctx.f or {}
print("FLAGS energy=" .. tostring(f.energy)
      .. " after_load=" .. tostring(f.after_load)
      .. " sceneB=" .. tostring(f.sceneB)
      .. " crossFallback=" .. tostring(f.crossFallback)
      .. " savedByStory=" .. tostring(f.savedByStory)
      .. " macroUsed=" .. tostring(f.macroUsed))
if f.energy and f.energy >= 15 then print("EVAL_OK") end
if f.after_load == 1 then print("LOAD_MISS_OK") end
if f.macroUsed == 1 then print("MACRO_OK") end
if f.sceneB == 1 then print("XSCENE_OK") end

os.exit(result:sub(1, 4) == "DONE" and 0 or 1)
