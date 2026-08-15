-- =============================================================================
--  test_contracts_runtime.lua — runtime execution of the round-51 contract
--  commands (real handlers + scheduler, headless-safe).
--
--  Guards fixed in round 52: [blur] (RTT alloc missing headless),
--  [saveload] (menu module absent), [listsaves] (list_saves binding
--  missing) — all degrade to a printed notice instead of crashing.
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local tokenizer = require('tokenizer')
local compiler = require('kag.compiler')
local scheduler = require('scheduler')
pcall(require, 'kag.commands.text')
pcall(require, 'kag.commands.system')
pcall(require, 'kag.commands.audio')
pcall(require, 'kag.commands.transition')
pcall(require, 'kag.commands.layer')
pcall(require, 'kag.commands.vfx')
pcall(require, 'kag.commands.video')
pcall(require, 'kag.commands.save')
pcall(require, 'kag.commands.math')

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then passed = passed + 1
    else failed = failed + 1
         print(string.format('  [FAIL] %s -- %s', name, detail or '')) end
end

local function run_scene(src)
    local tokens = tokenizer.parse(src)
    compiler.compile(tokens)
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {}, variables = {},
        current_scene = 't.ks', token_index = 1, tokens = tokens,
        text_state = {}, layer_state = {}, audio_state = {},
        macro_args = {}, call_stack = {}, flag_stack = {},
        backlog = {}, _choiceButtons = {}, unlockedCG = {}, unlockedMusic = {} }
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

print('\n=== Contract Runtime Tests (round 52) ===\n')

-- 1. unlock: writes ctx.unlockedCG / unlockedMusic
do
    local ctx, err = run_scene('[unlock type="cg" id="scene01"]')
    check('unlock cg writes ctx.unlockedCG', ctx and ctx.unlockedCG and ctx.unlockedCG.scene01 == true, err)
    local ctx2, err2 = run_scene('[unlock type="music" id="daily"]')
    check('unlock music writes ctx.unlockedMusic', ctx2 and ctx2.unlockedMusic and ctx2.unlockedMusic.daily == true, err2)
end

-- 2. skip mode
do
    local ctx, err = run_scene('[skip mode="seen"]')
    check('skip mode=seen sets ctx.skip_mode', ctx and ctx.skip_mode == 'seen', err)
end

-- 3. headless guards: blur / saveload / listsaves must not crash
do
    local ok, err = pcall(run_scene, '[blur amount=3]')
    check('blur headless-safe (RTT guard)', ok, err)
    local ok2, err2 = pcall(run_scene, '[saveload mode="save"]')
    check('saveload headless-safe (menu guard)', ok2, err2)
    local ok3, err3 = pcall(run_scene, '[listsaves]')
    check('listsaves headless-safe (binding guard)', ok3, err3)
    local ok4, err4 = pcall(run_scene, '[stopvideo]')
    check('stopvideo headless-safe', ok4, err4)
    local ok5, err5 = pcall(run_scene, '[rollback]')
    check('rollback headless-safe', ok5, err5)
end

-- 4. audio voice/wait commands degrade safely without a backend
do
    for _, cmd in ipairs({ 'playvoice', 'voice', 'stopvoice', 'waitbgm', 'waitsound', 'waitclick' }) do
        -- storage form exercises the real handler path; without an audio
        -- backend it degrades immediately (play_voice unavailable notice).
        local src = (cmd == 'playvoice' or cmd == 'voice')
            and ('[' .. cmd .. ' storage="v.wav"]') or ('[' .. cmd .. ']')
        local ok, err = pcall(run_scene, src)
        check(cmd .. ' headless-safe', ok, err)
    end
end

-- 5. layer/transition fades degrade without layers
do
    local ok, err = pcall(run_scene, '[fade to=100]')
    check('fade headless-safe', ok, err)
    local ok2, err2 = pcall(run_scene, '[layfade to=50]')
    check('layfade headless-safe', ok2, err2)
end

-- 6. choice commands: button/endbutton/select/endselect complete
do
    local ok, err = pcall(run_scene, '[button text="Go"][endbutton]')
    check('button+endbutton completes', ok, err)
    local ok2, err2 = pcall(run_scene, '[select][endselect]')
    check('select+endselect completes', ok2, err2)
end

-- 7. voice / stopvoice / playvoice: observable state on the audio event
--    (guard: audio backend -- real playback blocks until the track ends;
--    headless the backend degrades to a notice and the _CAESURA_AUDIO_EVENT
--    toggle is the observable state). [voice_off] mutes -> playvoice
--    short-circuits and stamps voice_end without touching the backend.
do
    local ctx, err = run_scene('[voice_off on=true][voice storage="v.wav"]')
    check('voice (muted) stamps _CAESURA_AUDIO_EVENT=voice_end',
        ctx and _G._CAESURA_AUDIO_EVENT == 'voice_end', err)

    _G._CAESURA_AUDIO_EVENT = nil
    local ctx2, err2 = run_scene('[voice_off on=true][playvoice storage="v.wav"]')
    check('playvoice (muted) sets voice_end without a backend',
        ctx2 and _G._CAESURA_AUDIO_EVENT == 'voice_end', err2)

    _G._CAESURA_AUDIO_EVENT = nil
    local ok3, err3 = pcall(run_scene, '[playvoice storage="v.wav"]')
    check('playvoice unmuted headless-safe (no audio backend)',
        ok3 and _G._CAESURA_AUDIO_EVENT == nil, err3)

    _G._CAESURA_AUDIO_EVENT = nil
    local ok4, err4 = pcall(run_scene, '[stopvoice]')
    check('stopvoice stamps _CAESURA_AUDIO_EVENT=voice_end',
        ok4 and _G._CAESURA_AUDIO_EVENT == 'voice_end', err4)
    _G._CAESURA_AUDIO_EVENT = nil
end

-- 8. waitbgm / waitsound: bounded waits on the BGM / SE buses
--    (guard: audio backend -- with one present they block until the bus
--    drains; headless audio_is_playing() is false so they yield once and
--    return). Prove the scheduler still advances past the wait.
do
    local ctx, err = run_scene('[waitbgm][unlock type="cg" id="after_waitbgm"]')
    local seen = ctx and ctx.unlockedCG and ctx.unlockedCG.after_waitbgm == true
    check('waitbgm completes; scheduler advances past it', seen, err)

    local ctx2, err2 = run_scene('[waitsound][unlock type="cg" id="after_waitsound"]')
    local seen2 = ctx2 and ctx2.unlockedCG and ctx2.unlockedCG.after_waitsound == true
    check('waitsound completes; scheduler advances past it', seen2, err2)
end

-- 9. waitclick: sets the waiting_input blocking flag, then resumes
--    (guard: none headless -- blocking is driven by ctx.waiting_input, which
--    the runner clears on the following frame, so the scene never hangs).
do
    local sawWaiting = false
    local function run_capture(src)
        local tokens = tokenizer.parse(src)
        compiler.compile(tokens)
        local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {}, variables = {},
            current_scene = 't.ks', token_index = 1, tokens = tokens,
            text_state = {}, layer_state = {}, audio_state = {},
            macro_args = {}, call_stack = {}, flag_stack = {},
            backlog = {}, _choiceButtons = {}, unlockedCG = {}, unlockedMusic = {} }
        local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
        local n = 0
        while coroutine.status(co) ~= 'dead' and n < 800 do
            local ok, perr = coroutine.resume(co, 16)
            if not ok then return nil, perr end
            if ctx.waiting_input then
                sawWaiting = true
                ctx.waiting_input = false
            end
            n = n + 1
        end
        if coroutine.status(co) ~= 'dead' then return nil, 'blocked after 800 iters' end
        return ctx
    end
    sawWaiting = false
    local ctx, err = run_capture('[waitclick][unlock type="cg" id="after_waitclick"]')
    check('waitclick observes waiting_input blocking flag', ctx and sawWaiting, err)
    check('waitclick completes; scheduler advances past it',
        ctx and ctx.unlockedCG and ctx.unlockedCG.after_waitclick == true, err)
end

-- 10. skip: bare [skip] toggles; [skip mode=seen] toggles seen-skip
--     (guard: none -- pure ctx state write).
do
    local ctx, err = run_scene('[skip]')
    check('bare [skip] toggles skip_mode on', ctx and ctx.skip_mode == true, err)

    local ctx2, err2 = run_scene('[skip][skip]')
    check('double [skip] toggles skip_mode off', ctx2 and ctx2.skip_mode == false, err2)

    local ctx3, err3 = run_scene('[skip mode="seen"][skip mode="seen"]')
    check('[skip mode=seen] twice turns it off', ctx3 and ctx3.skip_mode == false, err3)
end

-- 11. unlock: multiple ids accumulate in one scene
--     (guard: none -- pure ctx table write).
do
    local ctx, err = run_scene('[unlock type="cg" id="a"][unlock type="cg" id="b"]')
    check('two cg unlocks both recorded', ctx and ctx.unlockedCG
        and ctx.unlockedCG.a == true and ctx.unlockedCG.b == true, err)
end

-- 12. button / select / endbutton / endselect: staging + dissipation
--     (guard: choice-block -- [endbutton]/[endselect] wait for a pick via
--     coroutine.yield + waiting_input; the runner clears it, so the block
--     dissipates with no jump, mirroring the docs "dissolve" of unselected
--     choices).
do
    local ctx, err = run_scene('[button text="Hello" target="*lbl"]')
    check('[button] stages one choice', ctx and ctx._choiceButtons
        and #ctx._choiceButtons == 1
        and ctx._choiceButtons[1].text == 'Hello'
        and ctx._choiceButtons[1].target == '*lbl', err)

    local ctx2, err2 = run_scene('[select]')
    check('[select] initializes the staging table',
        ctx2 and ctx2._choiceButtons and #ctx2._choiceButtons == 0, err2)

    local ctx3, err3 = run_scene('[button text="Go"][endbutton]')
    check('[endbutton] dissipates (staging consumed, no crash)',
        ctx3 and ctx3._choiceButtons == nil, err3)

    local ctx4, err4 = run_scene('[select][sel text="A" target="*a"][endselect]')
    check('[sel]+[endselect] completes with no pending jump',
        ctx4 and ctx4._selectedChoice == nil and ctx4._pendingJump == nil, err4)
end

-- 13. rollback / stopvideo / fade / layfade: continue past the guarded op
--     (guard: rollback needs the kag_runner rollback() backend, stopvideo
--     needs the video decoder, fade/layfade need the rendering layer stack --
--     headless all degrade to a printed notice; assert the scheduler still
--     reaches a following command).
do
    local ctx, err = run_scene('[rollback][unlock type="cg" id="after_rollback"]')
    check('rollback headless-safe; scheduler advances past it', ctx
        and ctx.unlockedCG and ctx.unlockedCG.after_rollback == true, err)

    local ctx2, err2 = run_scene('[stopvideo][unlock type="cg" id="after_stopvideo"]')
    check('stopvideo headless-safe; scheduler advances past it', ctx2
        and ctx2.unlockedCG and ctx2.unlockedCG.after_stopvideo == true, err2)

    local ctx3, err3 = run_scene('[fade to=100][unlock type="cg" id="after_fade"]')
    check('fade headless-safe; scheduler advances past it', ctx3
        and ctx3.unlockedCG and ctx3.unlockedCG.after_fade == true, err3)

    local ctx4, err4 = run_scene('[layfade opacity=50][unlock type="cg" id="after_layfade"]')
    check('layfade headless-safe; scheduler advances past it', ctx4
        and ctx4.unlockedCG and ctx4.unlockedCG.after_layfade == true, err4)
end
-- 14. math command chain drives real variables through the scheduler
--     ([add] then [mul] reading f.x) and [div] by zero is caught and
--     reported ("[KAG] division by zero ...") without crashing the scene
--     -- the no-op lets the scheduler continue to a following command.
do
    local ctx, err = run_scene('[add name="f.x" value=5][mul name="f.x" value=3]')
    check('add then mul chain builds f.x=15', ctx and ctx.f.x == 15, err)

    local ctx2, err2 = run_scene(
        '[add name="f.x" value=10][div name="f.x" value=0][unlock type="cg" id="after_div0"]')
    check('div-by-zero is caught, no crash, f.x unchanged', ctx2
        and ctx2.f.x == 10, err2)
    check('div-by-zero no-op; scheduler advances past it', ctx2
        and ctx2.unlockedCG and ctx2.unlockedCG.after_div0 == true, err2)

    local ctx3, err3 = run_scene('[dec name="f.n" amount=2]')
    check('dec default/amount writes f.n', ctx3 and ctx3.f.n == -2, err3)
end

-- 15. [textspeed] / [cps] write the exact handler fields: ctx.text_speed
--     (ms/char, floor(1000/cps)) is the kag_runner read point, and
--     ctx.cps holds the observable chars-per-second value.
do
    local c100, e100 = run_scene('[textspeed cps=100]')
    check('[textspeed cps=100] sets ctx.text_speed=10', c100 and c100.text_speed == 10, e100)
    check('[textspeed cps=100] sets ctx.cps=100', c100 and c100.cps == 100, e100)

    local cdef, edef = run_scene('[textspeed]')
    check('[textspeed] default cps=50 -> text_speed=20', cdef and cdef.text_speed == 20, edef)
    check('[textspeed] default records ctx.cps=50', cdef and cdef.cps == 50, edef)

    local ccps, ecps = run_scene('[cps 25]')
    check('[cps 25] sets ctx.text_speed=40', ccps and ccps.text_speed == 40, ecps)
    check('[cps 25] sets ctx.cps=25', ccps and ccps.cps == 25, ecps)
end

-- 16. [notify msg=...] degrades gracefully headless: the toast module may
--     be absent, but the handler pcall-wraps require+show and returns
--     true, so the scene completes (round-52 guard).
do
    local ctx, err = run_scene('[notify msg="saved"]')
    check('[notify] headless-safe (toast degrade)', ctx ~= nil, err)
    local ctx2, err2 = run_scene('[notify msg="x" time=2500]')
    check('[notify] with time= headless-safe', ctx2 ~= nil, err2)
end

-- 17. [preload] contract enum: the valid form (type=texture/audio/scene)
--     with wait=true/path accepted coerces to a table and completes; an
--     unknown type is rejected by schema.coerce with a "must be one of"
--     error (the scene stops, reported not crashed).
do
    local ctx, err = run_scene('[preload type="texture" wait="true" path=""]')
    check('[preload type=texture wait=true] accepted', ctx ~= nil, err)
    local cimg, eimg = run_scene('[preload type="scene" wait="false"]')
    check('[preload type=scene wait=false] accepted', cimg ~= nil, eimg)

    local bad, badErr = run_scene('[preload type="video"]')
    check('[preload] unknown type rejected (schema error)',
        bad == nil and type(badErr) == 'string'
        and badErr:find('must be one of') ~= nil, badErr)
end

-- 18. [sel x="tf.result"] / [endbutton] full pipeline: register choices,
--     block on waiting_input, simulate a pick through the active block,
--     assert the chosen target lands in tf.result and becomes the jump.
--     (Round-74 mechanism mirrored from test_select.lua.)
do
    local function run_select_click(src, pick)
        local tokens = tokenizer.parse(src)
        compiler.compile(tokens)
        local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {}, variables = {},
            _whileIterByScene = { ['t.ks'] = 0 }, macros = nil, macro_args = nil,
            current_scene = 't.ks', token_index = 1, tokens = tokens,
            text_state = {}, layer_state = {}, audio_state = {},
            call_stack = {}, flag_stack = {}, backlog = {},
            _choiceButtons = {}, unlockedCG = {}, unlockedMusic = {} }
        local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
        local n = 0
        while coroutine.status(co) ~= 'dead' and not ctx.waiting_input and n < 50 do
            coroutine.resume(co, 16); n = n + 1
        end
        if ctx.waiting_input and ctx._choiceButtonsActive then
            -- choose the N-th rendered option
            ctx.waiting_input = false
            ctx._selectedChoice = ctx._choiceButtonsActive[pick]
            ctx._choiceMode = false
            ctx._choiceButtonsActive = nil
            while coroutine.status(co) ~= 'dead' and n < 800 do
                coroutine.resume(co, 16); n = n + 1
            end
        end
        if coroutine.status(co) ~= 'dead' then return nil, 'blocked' end
        return ctx
    end

    local ctxR, errR = run_select_click(
        '[sel x="tf.result" text="Route A" target="*a"][sel x="tf.result" text="Route B" target="*b"][endselect]', 2)
    check('sel-registered choices staged active', ctxR and ctxR._choiceButtonsActive == nil
        and ctxR._selectedChoice == nil, errR)
    check('sel x=tf.result holds chosen target', ctxR and ctxR.tf.result == '*b', errR)
    check('sel pick sets pending jump', ctxR and ctxR._pendingJump == '*b', errR)

    local ctxS, errS = run_select_click(
        '[sel x="picked" text="Only" target="*o"][endbutton]', 1)
    check('sel bare x= target -> f scope', ctxS and ctxS.f.picked == '*o', errS)
end

-- 19. nested macro definition e2e: an OUTER macro whose body contains an
--     INNER [macro]...[endmacro] definition; running the outer defines the
--     inner (which is thus invocable afterwards). Round 75 makes the body
--     collection depth-aware so the outer does not stop at the inner's
--     [endmacro]. (The inner definition makes the outer dynamic -- the
--     compiler does not inline it, so this exercises the runtime splice.)
do
    local src = '[macro outer][macro inner][unlock type="cg" id="from_inner"][endmacro][endmacro][outer][inner]'
    local ctx, err = run_scene(src)
    check('nested macro: outer defines inner, inner invocable afterwards',
        ctx and ctx.unlockedCG and ctx.unlockedCG.from_inner == true, err)
end

-- 20. macro depth guard: >1000 sequential calls complete (the runtime
--     stack pops per completed body, so a flat scene never trips), while a
--     self-recursive macro is caught with an expansion-depth error.
do
    -- The scheduler yields once per processed token, so a >1000-call flat
    -- scene needs a larger resume budget than run_scene's 800-iteration
    -- guard -- use a dedicated high-cap loop for the depth exercise.
    local function run_many(src)
        local tokens = tokenizer.parse(src)
        compiler.compile(tokens)
        local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {}, variables = {},
            current_scene = 't.ks', token_index = 1, tokens = tokens,
            text_state = {}, layer_state = {}, audio_state = {},
            macro_args = {}, call_stack = {}, flag_stack = {},
            backlog = {}, _choiceButtons = {}, unlockedCG = {}, unlockedMusic = {} }
        local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
        local n = 0
        while coroutine.status(co) ~= 'dead' and n < 5000 do
            local ok, err = coroutine.resume(co, 16)
            if not ok then return nil, err end
            if ctx.waiting_input then ctx.waiting_input = false end
            n = n + 1
        end
        if coroutine.status(co) ~= 'dead' then return nil, 'blocked after ' .. n .. ' iters' end
        return ctx
    end

    local seq = '[macro bump][unlock type="cg" id="x"][endmacro]'
        .. string.rep('[bump]', 1002)
    local ctx, err = run_many(seq)
    check('1002 sequential macro calls complete (no false depth error)',
        ctx ~= nil and ctx.unlockedCG and ctx.unlockedCG.x == true, err)

    local rec, recErr = run_scene('[macro rec][rec][endmacro][rec]')
    check('self-recursive macro trips expansion-depth guard',
        rec == nil and type(recErr) == 'string'
        and recErr:find('depth') ~= nil, recErr or 'no error raised')
end


print(string.format('\nCONTRACT RUNTIME TESTS: %d passed, %d failed', passed, failed))
if failed > 0 then os.exit(1) end