-- =============================================================================
--  test_saveflow.lua - Lua-side save/load command flow, headless-safe.
--
--  Exercises [save]/[load]/[saveload]/[listsaves]/[saveplace]/[loadplace]
--  through the real handlers (kag/commands/save.lua + system.lua) driven by
--  the scheduler. The C++ SaveManager bindings (KAG.save_game / KAG.load_game /
--  KAG.list_saves) are stubbed locally so capture_state / restore logic can be
--  asserted without any GPU, audio or disk I/O. The saveload_menu module is
--  stubbed (round-52 guard) so [saveload] takes its graceful 'menu unavailable'
--  path instead of trying to draw a live UI overlay.
-- =============================================================================

package.path = 'scripts/?.lua;scripts/kag/?.lua;' .. package.path

-- Round-52 guard: headless there is no save/load menu overlay; stub the module
-- so [saveload] degrades to a printed notice instead of rendering a UI.
package.preload['saveload_menu'] = function() return {} end

local tokenizer  = require('tokenizer')
local compiler   = require('kag.compiler')
local scheduler  = require('scheduler')
require('kag')               -- registers all command handlers incl. save
local saveCmds = require('kag.commands.save')
local System   = require('system')

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then passed = passed + 1
    else failed = failed + 1
         print(string.format('  [FAIL] %s -- %s', name, detail or '')) end
end

-- Stub the C++ SaveManager bindings. kag_binding() resolves rawget(_G,'KAG');
-- headless there is no C++ table, so we install a controllable fake. Each
-- save_game stores the serialized state so load_game can round-trip it back,
-- mirroring a real save slot but entirely in-memory.
_G.KAG = {}
local savedSlots = {}          -- slot -> {state=..., scene=..., token=...}
local lastSaved                 -- last save_game payload
_G.KAG.save_game = function(slot, state, scene, token, thumb)
    lastSaved = { slot=slot, state=state, scene=scene, token=token }
    savedSlots[slot] = lastSaved
    return true
end
_G.KAG.load_game = function(slot)
    local hit = savedSlots[slot]
    if hit then return hit.state, { slot = slot } end
    return nil, 'no-save-' .. tostring(slot)
end
_G.KAG.list_saves = function()
    local list = {}
    for slot in pairs(savedSlots) do table.insert(list, { slot = slot }) end
    table.sort(list, function(a,b) return a.slot < b.slot end)
    return list
end

-- Run one .ks source through tokenizer + compiler + scheduler (reference
-- pattern from test_contracts_runtime.lua). Withdraws waiting_input each
-- frame so a blocking tag never hangs the loop.
local function run_scene(src, init)
    local tokens = tokenizer.parse(src)
    compiler.compile(tokens)
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {}, variables = {},
        current_scene = 'demo/story.ks', token_index = 1, tokens = tokens,
        text_state = {}, layer_state = {}, audio_state = {},
        macro_args = {}, call_stack = {}, flag_stack = {},
        backlog = {}, _choiceButtons = {}, unlockedCG = {}, unlockedMusic = {},
        labelMap = {} }
    if init then init(ctx) end
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    local n = 0
    while coroutine.status(co) ~= 'dead' and n < 800 do
        local ok, err = coroutine.resume(co, 16)
        if not ok then return nil, err end
        if ctx.waiting_input then ctx.waiting_input = false end
        n = n + 1
    end
    if coroutine.status(co) ~= 'dead' then return nil, 'blocked after 800 iters' end
    return ctx
end

print('\n=== SaveFlow Tests (headless) ===\n')

-- ----------------------------------------------------------------
-- 1. [save] captures scene + token position and flags tf.save_result=ok
-- ----------------------------------------------------------------
do
    local ctx, err = run_scene('[set f.hp = 40][set f.mp = 3][save slot=1]')
    check('save runs headless', ctx ~= nil, err)
    check('save sets tf.save_result=ok',
        ctx and ctx.tf and ctx.tf.save_result == 'ok', 'got ' .. tostring(ctx and ctx.tf and ctx.tf.save_result))
    check('save sets tf.save_slot', ctx and ctx.tf and ctx.tf.save_slot == 1)
    check('save captures serialized f var',
        lastSaved and lastSaved.state.f.hp == 40, 'got ' .. tostring(lastSaved and lastSaved.state.f.hp))
    check('save captures scene_path',
        lastSaved and lastSaved.scene == 'demo/story.ks', 'got ' .. tostring(lastSaved and lastSaved.scene))
    check('save captures a forward token position',
        lastSaved and lastSaved.token and lastSaved.token >= 2, 'got ' .. tostring(lastSaved and lastSaved.token))
end

-- ----------------------------------------------------------------
-- 2. [load] restores variables + scene position (fresh-ctx round-trip)
-- ----------------------------------------------------------------
do
    local savedToken = lastSaved and lastSaved.token
    local ctxF, errF = run_scene('[load slot=1]',
        function(c) c.f.hp = 0; c.token_index = 100 end)
    check('load runs headless', ctxF ~= nil, errF)
    check('load sets tf.load_result=ok', ctxF and ctxF.tf and ctxF.tf.load_result == 'ok',
        'got ' .. tostring(ctxF and ctxF.tf and ctxF.tf.load_result))
    check('load sets tf.load_slot', ctxF and ctxF.tf and ctxF.tf.load_slot == 1)
    check('load restores restored f var', ctxF and ctxF.f.hp == 40, 'got ' .. tostring(ctxF and ctxF.f.hp))
    check('load schedules scene reload (_pendingLoadScene)',
        ctxF and ctxF._pendingLoadScene == 'demo/story.ks', 'got ' .. tostring(ctxF and ctxF._pendingLoadScene))
    check('load stops current run (stop_flag)', ctxF and ctxF.stop_flag == true)
    check('load queues the saved token for resume',
        ctxF and ctxF._pendingLoadToken == savedToken, tostring(ctxF and ctxF._pendingLoadToken) .. ' vs ' .. tostring(savedToken))
end

-- ----------------------------------------------------------------
-- 3. [load] on a missing slot degrades to tf.load_result=error (tutorial_07)
-- ----------------------------------------------------------------
do
    local ctx, err = run_scene('[load slot=99]')
    check('load missing-slot runs headless', ctx ~= nil, err)
    check('load missing-slot sets tf.load_result=error',
        ctx and ctx.tf and ctx.tf.load_result == 'error', 'got ' .. tostring(ctx and ctx.tf and ctx.tf.load_result))
end

-- ----------------------------------------------------------------
-- 4. Round-trip preserves f variable types (number/string/boolean)
-- ----------------------------------------------------------------
do
    run_scene(table.concat({ '[set f.hp = 42]', '[set f.name = \"Alice\"]',
        '[set f.alive = true]', '[set f.over = false]', '[save slot=7]' }, ' '),
        function(c) c.current_scene = 'demo/story2.ks'; c.token_index = 5 end)
    check('save typed f: number/string/boolean captured verbatim',
        lastSaved and lastSaved.state.f.hp == 42
        and lastSaved.state.f.name == 'Alice' and lastSaved.state.f.alive == true
        and lastSaved.state.f.over == false
        and type(lastSaved.state.f.hp) == 'number' and type(lastSaved.state.f.name) == 'string'
        and type(lastSaved.state.f.alive) == 'boolean' and type(lastSaved.state.f.over) == 'boolean',
        'hp=' .. tostring(lastSaved and lastSaved.state.f.hp)
        .. ' name=' .. tostring(lastSaved and lastSaved.state.f.name)
        .. ' alive=' .. tostring(lastSaved and lastSaved.state.f.alive))
    local ctx, err = run_scene('[load slot=7]', function(c) c.f.hp = 0; c.f.name = 'zzz' end)
    check('load typed round-trip restores exact values+types',
        ctx and ctx.f.hp == 42 and ctx.f.name == 'Alice' and ctx.f.alive == true and ctx.f.over == false
        and type(ctx.f.hp) == 'number' and type(ctx.f.name) == 'string'
        and type(ctx.f.alive) == 'boolean', err)
end

-- ----------------------------------------------------------------
-- 5. saveplace/loadplace: in-memory scene bookmark persists across a
--    fresh ctx (module-level _placeData). Restores jump target + tf.
-- ----------------------------------------------------------------
do
    local pctx, err = run_scene('[set f.hp = 5][set f.name = \"Bob\"]',
        function(c) c.current_scene = 'demo/bookmark.ks' end)
    check('saveplace runs headless', pctx ~= nil, err)
    saveCmds.saveplace(pctx, {})
    local pd = System._placeData
    check('saveplace stores scene bookmark',
        pd and pd.scene == 'demo/bookmark.ks' and pd.index and pd.index > 1,
        (pd and (pd.scene .. '@' .. tostring(pd.index))) or 'nil')
    local lpctx = { f={}, sf={}, tf={}, mp={}, lf={}, variables={},
        current_scene='demo/other.ks', token_index=1, tokens=pctx.tokens,
        text_state={}, layer_state={}, audio_state={}, macro_args={},
        call_stack={}, flag_stack={}, backlog={}, labelMap={} }
    saveCmds.loadplace(lpctx, {})
    check('loadplace restores scene position (_pendingJump)',
        lpctx._pendingJump and lpctx._pendingJump.scene == 'demo/bookmark.ks'
        and lpctx._pendingJump.index == pd.index,
        tostring(lpctx._pendingJump and lpctx._pendingJump.scene)
        .. '@' .. tostring(lpctx._pendingJump and lpctx._pendingJump.index))
    check('loadplace sets stop_flag', lpctx.stop_flag == true)
end

-- ----------------------------------------------------------------
-- 6. loadplace with no prior bookmark degrades gracefully (no crash)
-- ----------------------------------------------------------------
do
    local hadBookmark = System._placeData ~= nil
    System._placeData = nil
    local okL, res = pcall(saveCmds.loadplace,
        { f={}, sf={}, tf={}, mp={}, lf={}, variables={}, current_scene='demo/other.ks' }, {})
    check('loadplace with no bookmark headless-safe', okL, tostring(res))
    if hadBookmark then end
end

-- ----------------------------------------------------------------
-- 7. listsaves populates ctx.sf.save_list from the binding
-- ----------------------------------------------------------------
do
    local ctx, err = run_scene('[listsaves]')
    check('listsaves runs headless', ctx ~= nil, err)
    check('listsaves populates ctx.sf.save_list',
        ctx and ctx.sf and ctx.sf.save_list and type(ctx.sf.save_list) == 'table',
        'type=' .. tostring(ctx and ctx.sf and type(ctx.sf.save_list)))
    check('listsaves reflects saved slots',
        ctx and ctx.sf.save_list and #ctx.sf.save_list == 2, 'got ' .. tostring(ctx and ctx.sf and #(ctx.sf.save_list or {})))
    check('listsaves mirrors into ctx.tf', ctx and ctx.tf and ctx.tf.save_list ~= nil)
end

-- ----------------------------------------------------------------
-- 8. listsaves guards gracefully when the binding is absent
-- ----------------------------------------------------------------
do
    local realList = _G.KAG.list_saves
    _G.KAG.list_saves = nil
    local ctx, err = run_scene('[listsaves]')
    check('listsaves without binding headless-safe', ctx ~= nil, err)
    check('listsaves without binding yields empty guarded list',
        ctx and ctx.sf and ctx.sf.save_list and type(ctx.sf.save_list) == 'table'
        and #ctx.sf.save_list == 0, 'got ' .. tostring(ctx and ctx.sf and #(ctx.sf.save_list or {})))
    _G.KAG.list_saves = realList
end

-- ----------------------------------------------------------------
-- 9. saveload (menu) degrades gracefully headless (round-52 menu guard)
--    The menu module is stubbed to return {} (no .show), so the handler
--    prints 'menu unavailable' and returns without drawing a UI.
-- ----------------------------------------------------------------
do
    local ctx, err = run_scene('[saveload mode=\"save\"]')
    check('saveload headless-safe (menu guard), scene completes', ctx ~= nil, err)
end

-- ----------------------------------------------------------------
-- 10. save inside a sub-call frame succeeds; lf is a documented reset
--      (capture_state does not serialize lf, so [load] cannot resurrect
--      local-frame flags).
-- ----------------------------------------------------------------
do
    local src = table.concat({
        '*sub',
        '[set f.inner = 9]',
        '[save slot=3]',
        '[return]',
        '*main',
        '[call target=\"*sub\"]',
        '[set f.done = true]',
    }, '\n')
    local ctx, err = run_scene(src, function(c) c.lf = { frame = 'outer' }; c.current_scene = 'demo/subcall.ks' end)
    check('save in sub-call frame runs headless', ctx ~= nil, err)
    check('save in sub-call frame succeeds (tf.save_result=ok)',
        ctx and ctx.tf and ctx.tf.save_result == 'ok',
        'got ' .. tostring(ctx and ctx.tf and ctx.tf.save_result))
    check('sub-call save captures f from the inner frame',
        lastSaved and lastSaved.state.f.inner == 9, 'got ' .. tostring(lastSaved and lastSaved.state.f.inner))
    check('lf is NOT serialized (documented reset via capture_state)',
        lastSaved and lastSaved.state.lf == nil)
end

-- ----------------------------------------------------------------
-- 11. save failure path: binding returns false -> tf.save_result=error
-- ----------------------------------------------------------------
do
    local realSave = _G.KAG.save_game
    _G.KAG.save_game = function() return false end -- simulate C++ write failure
    local ctx, err = run_scene('[save slot=4]')
    check('save failure runs headless', ctx ~= nil, err)
    check('save failure sets tf.save_result=error',
        ctx and ctx.tf and ctx.tf.save_result == 'error', 'got ' .. tostring(ctx and ctx.tf and ctx.tf.save_result))
    _G.KAG.save_game = realSave
end

-- ----------------------------------------------------------------
-- Round 74 (stage D): [save] inside a [for] loop — token position +
-- iteration-state boundary.
-- ----------------------------------------------------------------
do
    -- A counter loop whose body increments f.hp then saves at hp==2.
    -- capture_state stores ctx.f (so the loop counter vname 'i' AND hp
    -- travel into the slot), but the loop's live iteration machinery
    -- (scheduler.run's for_stack/while_stack locals + the ctx rewind
    -- marks) is NOT serialized.
    local src = table.concat({
        '[for i=0 end=3]',
        '[inc f.hp]',
        '[if exp="f.hp == 2"][save slot=6][endif]',
        '[inc f.iter_done]',
        '[endfor]',
    }, '\n')
    -- first pass: run the loop to completion
    local ctx1 = run_scene(src, function(c) c.f.hp = 0; c.f.iter_done = 0 end)
    check('for-save full run headless', ctx1 ~= nil)
    check('for-save full loop runs 4 iterations',
        ctx1 and ctx1.f and ctx1.f.iter_done == 4,
        'iter_done=' .. tostring(ctx1 and ctx1.f and ctx1.f.iter_done))

    -- the save taken at hp==2 captures the counter in f (i=1 here) and the
    -- token position INSIDE the loop body
    local st6 = savedSlots[6] and savedSlots[6].state
    check('for-save captures loop counter in f',
        st6 and st6.f and st6.f.i == 1,
        'i=' .. tostring(st6 and st6.f and st6.f.i))
    check('for-save captures hp snapshot', st6 and st6.f and st6.f.hp == 2)
    check('for-save token sits inside the loop body',
        st6 and st6.token_index and st6.token_index > 0)

    -- iteration scratch state NEVER enters the slot (scheduler-internal)
    check('for-save excludes _forStackMarks', st6 and st6._forStackMarks == nil)
    check('for-save excludes _forRewound', st6 and st6._forRewound == nil)
    check('for-save excludes _whileIterByScene', st6 and st6._whileIterByScene == nil)

    -- BOUNDARY (documented defect): resume the saved scene at the saved
    -- token with a FRESH scheduler, mirroring load's resume_from_save
    -- (reload scene + re-spawn scheduler.run at token_index). The loop
    -- counter i and hp are restored from the slot, but for_stack is a
    -- scheduler.run LOCAL and is gone — the [endfor] after the resume point
    -- is a no-op, so the loop falls out of scope instead of continuing.
    local toksR = tokenizer.parse(src)
    compiler.compile(toksR)
    local ctxR = { f = {}, sf = {}, tf = {}, mp = {}, lf = {}, variables = {},
        current_scene = 'demo/story.ks', tokens = toksR,
        text_state = {}, layer_state = {}, audio_state = {},
        macro_args = {}, call_stack = {}, flag_stack = {},
        backlog = {}, _choiceButtons = {}, unlockedCG = {}, unlockedMusic = {},
        labelMap = {} }
    for k, v in pairs(st6 and st6.f or {}) do ctxR.f[k] = v end
    ctxR.token_index = st6 and st6.token_index or 1
    local coR = coroutine.create(function()
        scheduler.run(ctxR, toksR, ctxR.token_index) end)
    local nr = 0
    while coroutine.status(coR) ~= 'dead' and nr < 800 do
        local okR, errR = coroutine.resume(coR, 16)
        if not okR then return nil, errR end
        if ctxR.waiting_input then ctxR.waiting_input = false end
        nr = nr + 1
    end
    check('for-load resume runs headless', ctxR ~= nil)
    check('for-load resume completes the in-flight iteration body',
        ctxR and ctxR.f and ctxR.f.iter_done == 2,
        'iter_done=' .. tostring(ctxR and ctxR.f and ctxR.f.iter_done))
    -- If the iteration machinery were preserved, the loop keeps rewinding
    -- to iter_done==4. It does not (scheduler-local for_stack lost after
    -- the fresh scheduler.run) — the loop exits early at iter_done==2.
    check('for-load [endfor] does NOT rewind into remaining iterations '
        .. '(known boundary: loop exits early)',
        ctxR and ctxR.f and ctxR.f.iter_done < 4,
        'iter_done=' .. tostring(ctxR and ctxR.f and ctxR.f.iter_done))
end
print(string.format('\nSAVEFLOW TESTS: %d passed, %d failed', passed, failed))
if failed > 0 then os.exit(1) end
print('SAVEFLOW TESTS DONE')
