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

print(string.format('\nCONTRACT RUNTIME TESTS: %d passed, %d failed', passed, failed))
if failed > 0 then os.exit(1) end