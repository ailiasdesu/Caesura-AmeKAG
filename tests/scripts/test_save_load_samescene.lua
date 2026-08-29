-- =============================================================================
--  test_save_load_samescene.lua -- native [save]+[load] same-scene resume guard
--  + scene-path allowlist extension (t52/t58 findings, t62 fix).
--
--  Defect A: kag_runner.resume_from_save honored the saved token without the
--  round-94 self-reference guard -- a [save] then [load] back-to-back in the
--  SAME scene restored the cursor at/before the consumed [load], re-executing
--  the [save]/[load] block forever. (The web bridge carries the guard at
--  web/bridge.js 650-655 and 1024-1029; the native runner did not.)
--  Defect B: _safeScenePath (scripts/kag/commands/save.lua:30-39) lacked
--  ^tests/projects/ and ^projects/, so packaged-game story saves (BUILD-INFO
--  entry = projects/<game>/story.ks) were silently rejected on [load].
--
--  Orphan suite (global mocks like test_saveflow + kag_runner drive harness).
--  Exit: 0 = all checks passed, 1 = any check failed.
-- =============================================================================

package.path = 'scripts/?.lua;scripts/?/init.lua;scripts/kag/?.lua;scripts/kag/commands/?.lua;' .. package.path

local DIR_A = 'demo/t62_saveload'
local DIR_B = 'tests/projects/caesura-t62'

-- ---- Mock C++ bindings (audio callable + REAL in-memory save stubs) ----
local function callable(t)
    return setmetatable(t or {}, {
        __index = function(self, key)
            if type(key) ~= 'string' then return nil end
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
-- C++ SaveManager stubs (mirror test_saveflow: slot -> serialized state)
local savedSlots = {}
_G.KAG.save_game = function(slot, state, scene, token, thumb)
    savedSlots[slot] = { state = state, scene = scene, token = token }
    return true
end
_G.KAG.load_game = function(slot)
    local hit = savedSlots[slot]
    if hit then return hit.state, { slot = slot } end
    return nil, 'no-save-' .. tostring(slot)
end
_G.KAG.list_saves = function()
    local l = {}
    for slot in pairs(savedSlots) do table.insert(l, { slot = slot }) end
    table.sort(l, function(a, b) return a.slot < b.slot end)
    return l
end
_G.Render  = callable({})
_G.DevCore = callable({})
_G.Engine  = callable({})
_G.backend = callable({
    is_voice_playing  = function() return false end,
    is_bgm_playing    = function() return false end,
    get_active_voices = function() return 0 end,
})

local kag_runner = require('kag_runner')
local saveCmds   = require('kag.commands.save')

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
    else
        failed = failed + 1
        print(string.format('  [FAIL] %s -- %s', name, detail or ''))
    end
end

local function wf(dir, name, text)
    pcall(os.execute, 'mkdir "' .. dir .. '" 2>nul')
    local f = io.open(dir .. '/' .. name, 'w')
    if not f then return false end
    f:write(text)
    f:close()
    return true
end

-- ---- kag_runner drive harness (auto-click [p]); FRAME_LIMIT guard ----
local function drive(storyPath)
    local started, err = kag_runner.start(storyPath)
    if not started then return 'FATAL_START:' .. tostring(err), nil end
    local FMAX, frames, result = 5000, 0, nil
    while frames < FMAX do
        frames = frames + 1
        local ok, reason = kag_runner.update(0.016)
        local ctx = _G._CAESURA_CTX
        if reason == 'ended' then result = 'DONE:' .. frames break end
        if ctx and ctx.tokens and ctx.token_index
           and ctx.token_index > #ctx.tokens then
            result = 'DONE(exhausted):' .. frames
            break
        end
        if ctx and ctx.waiting_input then
            kag_runner.on_click()
        end
    end
    if not result then result = 'FRAME_LIMIT' end
    return result, _G._CAESURA_CTX
end

print('=== Native [save]+[load] same-scene resume + allowlist Tests ===')

-- ---- fixtures ----
local A_KS = [[
*start
[set var="f.before" value=1]
[ch name="N" text="before"]
[p]
[save slot=1]
[set var="f.after_save" value=1]
[load slot=1]
[set var="f.after" value=1]
[end]
]]

local B_KS = [[
*start
[set var="f.route" value="proj"]
[ch name="N" text="p"]
[p]
[save slot=2]
[set var="f.mid" value=1]
[load slot=2]
[set var="f.done" value=1]
[end]
]]

wf(DIR_A, 'story.ks', A_KS)
wf(DIR_B, 'story.ks', B_KS)

-- ---- Case A: same-scene [save]->[load] must NOT loop (t62 detector) ----
local aRes, aCtx = drive(DIR_A .. '/story.ks')
print('  [A] ' .. aRes)
check('A: same-scene save->load reaches DONE (no loop / FRAME_LIMIT detector)',
      aRes:sub(1, 4) == 'DONE', aRes)
check('A: flow continues past the [load] (f.after=1)',
      aCtx and aCtx.f and aCtx.f.after == 1, aCtx and tostring(aCtx.f and aCtx.f.after) or 'no-ctx')
check('A: the replayed save marker ran (f.after_save=1)',
      aCtx and aCtx.f and aCtx.f.after_save == 1, aCtx and tostring(aCtx.f and aCtx.f.after_save) or 'no-ctx')

-- ---- Case B: tests/projects/ path saves resume (allowlist extension) ----
local bRes, bCtx = drive(DIR_B .. '/story.ks')
print('  [B] ' .. bRes)
check('B: tests/projects/ save->load reaches DONE (allowlist path accepted)',
      bRes:sub(1, 4) == 'DONE', bRes)
check('B: restored flow ran to [end] (f.done=1)',
      bCtx and bCtx.f and bCtx.f.done == 1, bCtx and tostring(bCtx.f and bCtx.f.done) or 'no-ctx')

-- ---- Case B2: traversal still rejected at the allowlist boundary ----
check('B2: tests/projects/golden_vn/story.ks accepted',
      saveCmds._safeScenePath('tests/projects/golden_vn/story.ks') == true)
check('B2: projects/my_game/story.ks accepted',
      saveCmds._safeScenePath('projects/my_game/story.ks') == true)
check('B2: tests/projects/../secret.ks rejected (.. traversal)',
      saveCmds._safeScenePath('tests/projects/../secret.ks') == false)
check('B2: projects/../evil.ks rejected (.. traversal)',
      saveCmds._safeScenePath('projects/../evil.ks') == false)
check('B2: non-.ks under tests/projects rejected',
      saveCmds._safeScenePath('tests/projects/foo.txt') == false)
check('B2: unreachable path rejected',
      saveCmds._safeScenePath('/etc/passwd') == false)

pcall(os.execute, 'rmdir /s /q "' .. DIR_A .. '" 2>nul')
pcall(os.execute, 'rmdir /s /q "' .. DIR_B .. '" 2>nul')

print('')
print(string.format('save/load same-scene + allowlist tests: %d passed, %d failed', passed, failed))
os.exit(failed == 0 and 0 or 1)
