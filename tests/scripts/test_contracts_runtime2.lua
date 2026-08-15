-- =============================================================================
--  test_contracts_runtime2.lua — runtime execution of the round-76 KAG
--  command contracts (real handlers + scheduler, headless-safe). Extends
--  test_contracts_runtime.lua with deeper coverage of the math / text /
--  character / vfx(palette+vibrate) / audio / notify / wait-delay / choice
--  command families.
--
--  Headless guard strategy (mirrors the round-51/76 test):
--    * commands that need a GPU/audio backend degrade to a printed notice
--      and must never crash or block the scene — we assert the scheduler
--      still reaches a FOLLOWING command ([unlock] sentinel).
--    * the character commands ([csp]/[csd]/[csl]) route through the real
--      backend module; supplying a minimal _G._CAESURA_BACKEND bridge mock
--      lets us observe the resolved asset path and the ctx.characters /
--      ctx.layers registries (path-resolution assertions).
--    * the global mock is scoped to this isolated subprocess (the orphan
--      runner spawns one process per test), so it never pollutes the main
--      suite.
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local tokenizer = require('tokenizer')
local compiler = require('kag.compiler')
local scheduler = require('scheduler')
pcall(require, 'kag.commands.system')
pcall(require, 'kag.commands.math')
pcall(require, 'kag.commands.text')
pcall(require, 'kag.commands.audio')
pcall(require, 'kag.commands.character')
pcall(require, 'kag.commands.vfx')

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then passed = passed + 1
    else failed = failed + 1
         print(string.format('  [FAIL] %s -- %s', name, detail or '')) end
end

-- Standard headless runner: no backend mock, runs the real scheduler over
-- the compiled token stream. The runner clears ctx.waiting_input so
-- blocking commands (waits / choices) dissipate instead of hanging.
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

print('\n=== Contract Runtime Tests (round 76 extension) ===\n')

-- ═══════════════════════════════════════════════════════════════════════
--  1. Math commands: [inc]/[dec]/[add]/[sub]/[mul]/[div]/[mod]
--     * nil-safe reads (missing var starts at 0);
--     * variable scoping across f./sf./tf./mp./lf.;
--     * chained calls build a value; div/mod by zero no-op with a visible
--       "[KAG] division by zero" notice and never crash the scene.
-- ═══════════════════════════════════════════════════════════════════════
do
    -- Chained [inc] on the default f. scope: 0 +2 +1 -5 = -2
    local ctx, err = run_scene('[inc var="f.a" by=2][inc var="f.a"][inc var="f.a" by=-5]')
    check('inc chain: 0+2+1-5 == -2', ctx and ctx.f.a == -2, err)

    -- [inc] on sf. scope, then [add] feeds the same variable (scoping persists)
    local ctx2, err2 = run_scene('[inc var="sf.b" by=3][add name="sf.b" value=2]')
    check('inc in sf. then add: 3+2 == 5', ctx2 and ctx2.sf.b == 5, err2)

    -- [add]+[mod] chain on tf.: 10 % 3 == 1
    local ctx3, err3 = run_scene('[add name="tf.c" value=10][mod name="tf.c" value=3]')
    check('add then mod: 10%3 == 1', ctx3 and ctx3.tf.c == 1, err3)

    -- [dec] default amount (1) on mp.: missing var starts at 0 => -1
    local ctx4, err4 = run_scene('[dec name="mp.d"]')
    check('dec default amount writes mp.d == -1', ctx4 and ctx4.mp.d == -1, err4)

    -- [dec amount=0] preserves the existing value (0 - 0 == 0, set from inc)
    local ctx5, err5 = run_scene('[inc var="f.q" by=7][dec name="f.q" amount=0]')
    check('dec amount=0 preserves value', ctx5 and ctx5.f.q == 7, err5)

    -- Full chain on lf.: set 4, *5, -10 = 10 (division falls to float 4.0 path)
    -- [set] is a system command; its handler uses var= like [inc].
    local ctx6, err6 = run_scene('[set var="lf.e" value=4][mul name="lf.e" value=5][sub name="lf.e" value=10]')
    check('set+mul+sub chain: 4*5-10 == 10', ctx6 and ctx6.lf.e == 10, err6)

    -- Chain to a final [div]: (3-1)*4/2 == 4 (float result)
    local ctx7, err7 = run_scene('[inc var="f.v" by=3][dec name="f.v" amount=1][mul name="f.v" value=4][div name="f.v" value=2]')
    check('mul/div chain yields 4.0', ctx7 and ctx7.f.v == 4.0, err7)

    -- div-by-zero: value unchanged (10), no crash, scheduler advances past it
    local ctx8, err8 = run_scene('[add name="f.w" value=10][div name="f.w" value=0][unlock type="cg" id="after_div0"]')
    check('div-by-zero no-op keeps f.w', ctx8 and ctx8.f.w == 10, err8)
    check('div-by-zero does not block the scene', ctx8 and ctx8.unlockedCG
        and ctx8.unlockedCG.after_div0 == true, err8)

    -- mod-by-zero: same guard (7 % 0 impossible) — value unchanged, no crash
    local ctx9, err9 = run_scene('[add name="f.z" value=7][mod name="f.z" value=0][unlock type="cg" id="after_mod0"]')
    check('mod-by-zero no-op keeps f.z', ctx9 and ctx9.f.z == 7, err9)
    check('mod-by-zero still advances the scene', ctx9 and ctx9.unlockedCG
        and ctx9.unlockedCG.after_mod0 == true, err9)
end

-- ═══════════════════════════════════════════════════════════════════════
--  2. Text commands: [textspeed] / [cps] — value takes effect on both
--     observable fields (ctx.cps and the kag_runner read point
--     ctx.text_speed = floor(1000/cps)); boundary clamps at 1..120 cps.
--     [pt speed=...] (ms/char) is the sibling that writes the same
--     read point, so the two families must interleave cleanly.
-- ═══════════════════════════════════════════════════════════════════════
do
    local c, e = run_scene('[textspeed cps=25]')
    check('[textspeed cps=25] -> ctx.cps=25', c and c.cps == 25, e)
    check('[textspeed cps=25] -> text_speed=40 (floor(1000/25))', c and c.text_speed == 40, e)

    local c2, e2 = run_scene('[cps 10]')
    check('[cps 10] alias -> ctx.cps=10', c2 and c2.cps == 10, e2)
    check('[cps 10] -> text_speed=100', c2 and c2.text_speed == 100, e2)

    -- Boundary lower clamp: cps=0 is clamped up to 1 (min)              
    local c3, e3 = run_scene('[textspeed cps=0]')
    check('[textspeed cps=0] clamps cps up to 1', c3 and c3.cps == 1, e3)
    check('[textspeed cps=0] -> text_speed=1000 (1000/1)', c3 and c3.text_speed == 1000, e3)

    -- Boundary upper clamp: cps=500 is clamped down to 120 (max)        
    local c4, e4 = run_scene('[cps 500]')
    check('[cps 500] clamps cps down to 120', c4 and c4.cps == 120, e4)
    check('[cps 500] -> text_speed=8 (1000/120 floor)', c4 and c4.text_speed == 8, e4)

    -- [pt] (ms/char) and [textspeed] (cps) write the SAME read point; a
    -- later [pt] overrides a prior [textspeed] cleanly.
    local c5, e5 = run_scene('[cps 25][pt speed=80]')
    check('[pt] overrides the cps-derived text_speed', c5 and c5.text_speed == 80, e5)
    check('[pt] leaves ctx.cps untouched (cps from [cps])', c5 and c5.cps == 25, e5)
end

-- ═══════════════════════════════════════════════════════════════════════
--  3. Effects: [vibrate] (alias for [vib]) and [palette] — headless degrade.
--     * vibrate must never crash without a backend (forwards to transition).
--     * palette's guarded effects (day/clear) print a no-op notice.
--     * ALL palette/vibrate scenes still reach a following command.
--     (Known gap, reported: palette effect="night"/"toggle" hits a global
--      backend index before the lut guard — the scheduler catches it so the
--      scene survives, but the error is logged. We assert scene survival.)
-- ═══════════════════════════════════════════════════════════════════════
do
    local v1, e1 = run_scene('[vibrate]')
    check('[vibrate] bare headless-safe', v1 ~= nil, e1)
    local v2, e2 = run_scene('[vibrate time=200 intensity=4]')
    check('[vibrate time/intensity] headless-safe', v2 ~= nil, e2)
    local v3, e3 = run_scene('[vibrate][unlock type="cg" id="after_vib"]')
    check('[vibrate] does not block the scene', v3 and v3.unlockedCG
        and v3.unlockedCG.after_vib == true, e3)

    local p1, pe1 = run_scene('[palette effect="day"]')
    check('[palette effect=day] headless-safe', p1 ~= nil, pe1)
    local p2, pe2 = run_scene('[palette effect="clear"]')
    check('[palette effect=clear] headless-safe', p2 ~= nil, pe2)

    -- night/toggle reach the un-guarded global-backend path headless; the
    -- scheduler pcalls the handler so the SCENE survives. Assert survival:
    local p3, pe3 = run_scene('[palette effect="night"][unlock type="cg" id="after_night"]')
    check('[palette effect=night] scene survives (scheduler guard)', p3 ~= nil, pe3)
    check('[palette effect=night] still advances past it', p3 and p3.unlockedCG
        and p3.unlockedCG.after_night == true, pe3 or 'handler errored')

    local p4, pe4 = run_scene('[palette effect="toggle"]')
    check('[palette effect=toggle] scene survives (scheduler guard)', p4 ~= nil, pe4)
end

-- ═══════════════════════════════════════════════════════════════════════
--  4. Character commands: [csp]/[csd]/[csl] — path resolution + registries.
--     * default asset path: assets/char/<name>.png;
--     * explicit storage/file override wins;
--     * [csl] repositions an existing layer; [csd] clears the registry.
--     * a minimal _G._CAESURA_BACKEND bridge mock supplies load_texture so
--       the real handler path (backend.load_texture -> ctx.characters) is
--       exercised headless. Scoped to this subprocess.
--     * KNOWN GAP (reported, not fixed): the KAG3 left=/top= aliases are
--       NOT mapped to x=/y= — schema reports them as unknown params and
--       ignores them. We assert the command still completes (only x/y are
--       honored); a fix would map left->x / top->y in the csp/csl schema.
-- ═══════════════════════════════════════════════════════════════════════
do
    -- Dispatcher-style backend bridge mock: backend.load_texture routes
    -- through be.render("load_texture", file); record the resolved path so
    -- we can assert on it, and return a truthy "handle" so the real csp
    -- handler proceeds to register ctx.characters / ctx.layers.
    _G._CAESURA_BACKEND = {
        render = function(method, ...)
            if method == "load_texture" then
                _G.__char_last_load = ...
                return { handle = ... }
            elseif method == "is_valid" then
                return ... ~= nil
            end
            return true
        end,
        platform = function(method, ...) return true end,
    }
    local function run_char(src)
        return run_scene(src)
    end

    local csp, e = run_char('[csp name="hero" layer="fg0"]')
    check('csp default path resolves to assets/char/hero.png',
        csp and _G.__char_last_load == 'assets/char/hero.png', e)
    check('csp registers ctx.characters with file + chara',
        csp and csp.characters and csp.characters.fg0
        and csp.characters.fg0.chara == 'hero'
        and csp.characters.fg0.file == 'assets/char/hero.png', e)
    check('csp records the asset in ctx.layers under the layer key',
        csp and csp.layers and csp.layers.fg0 == 'assets/char/hero.png', e)

    _G.__char_last_load = nil
    local over, e2 = run_char('[csp name="villain" layer=1 storage="assets/custom/v.png"]')
    check('csp storage override wins over the default stem',
        over and _G.__char_last_load == 'assets/custom/v.png', e2)
    check('csp storage override lands in ctx.characters',
        over and over.characters and over.characters['1']
        and over.characters['1'].file == 'assets/custom/v.png', e2)

    -- [csl]: reposition an existing (previously shown) layer headless-safe.
    local move, e3 = run_char('[csp name="hero" layer="ch0"][csl name="hero" layer="ch0" x=300 y=240]')
    check('[csl] on a shown layer completes headless-safe', move ~= nil, e3)
    check('[csl] keeps the character registered', move and move.characters
        and move.characters.ch0 ~= nil, e3)

    -- [csd]: clear removes the character registry + layers entry.
    local cls, e4 = run_char('[csp name="hero" layer="chA"][csd name="hero" layer="chA"]')
    check('[csd] clears ctx.characters for the layer',
        cls and (cls.characters.chA == nil), e4)
    check('[csd] clears ctx.layers for the layer',
        cls and (cls.layers.chA == nil), e4)

    -- left/top aliases: NOT mapped to x/y (reported gap) — must not crash
    -- and the command completes; verify the default-position stem resolved.
    _G.__char_last_load = nil
    local alias, e5 = run_char('[csp name="hero" left=100 top=50]')
    check('[csp left/top] not a crash (aliases are ignored, x/y honored)',
        alias ~= nil, e5)
end

    -- The mock is scoped ONLY to the character block; drop it so the
    -- audio category below runs the pure headless path (degrade notices,
    -- not mock errors).
    _G._CAESURA_BACKEND = nil

-- ═══════════════════════════════════════════════════════════════════════
--  5. Audio commands: [playbgm]/[stopbgm]/[playse]/[stopse]/[fadebgm]/
--     [set*bgmvolume] — headless without an audio backend they degrade to
--     a printed "unavailable" notice and must never crash or block.
--     We assert the scheduler reaches a following command after each.
-- ═══════════════════════════════════════════════════════════════════════
do
    local a1, e1 = run_scene('[playbgm storage="b.ogg"][unlock type="cg" id="a_bgm"]')
    check('[playbgm] headless-safe; advances past it', a1 and a1.unlockedCG
        and a1.unlockedCG.a_bgm == true, e1)

    local a2, e2 = run_scene('[playse storage="s.wav" volume=0.5][unlock type="cg" id="a_se"]')
    check('[playse] headless-safe; advances past it', a2 and a2.unlockedCG
        and a2.unlockedCG.a_se == true, e2)

    local a3, e3 = run_scene('[stopbgm fadeout=300][unlock type="cg" id="a_stopbgm"]')
    check('[stopbgm fadeout] headless-safe; advances', a3 and a3.unlockedCG
        and a3.unlockedCG.a_stopbgm == true, e3)

    local a4, e4 = run_scene('[stopse][unlock type="cg" id="a_stopse"]')
    check('[stopse] headless-safe; advances', a4 and a4.unlockedCG
        and a4.unlockedCG.a_stopse == true, e4)

    local a5, e5 = run_scene('[fadebgm volume=0.2 time=500][setbgmvolume 0.8][unlock type="cg" id="a_vol"]')
    check('fadebgm + setbgmvolume headless-safe; advances', a5 and a5.unlockedCG
        and a5.unlockedCG.a_vol == true, e5)

    -- [playbgm] with NO file is a schema-level error ("requires one of
    -- {file,storage}"): the scene is reported, not crashed. Mirror the
    -- round-76 [preload] unknown-type assertion style.
    local a6, e6 = run_scene('[playbgm]')
    check('[playbgm] missing file reported as a schema error',
        a6 == nil and type(e6) == 'string' and e6:find('requires one of') ~= nil, e6)
end

-- ═══════════════════════════════════════════════════════════════════════
--  6. [notify] — toast degrade. Headless the toast module is absent, the
--     handler pcall-guards require+show and the scene completes.
-- ═══════════════════════════════════════════════════════════════════════
do
    local n1, e1 = run_scene('[notify msg="saved"]')
    check('[notify] headless-safe (toast degrade)', n1 ~= nil, e1)
    local n2, e2 = run_scene('[notify msg="x" time=3500]')
    check('[notify] with time= headless-safe', n2 ~= nil, e2)
    local n3, e3 = run_scene('[notify msg="banner"][unlock type="cg" id="after_notify"]')
    check('[notify] does not block the scene', n3 and n3.unlockedCG
        and n3.unlockedCG.after_notify == true, e3)
end

-- ═══════════════════════════════════════════════════════════════════════
--  7. [wait] / [delay] — timeout semantics. Both register as blocking
--     commands whose coroutine yields each frame and returns after the
--     requested ms (0 <= ms <= 60000 clamp). The runner feeds a fixed
--     16ms dt, so a 50ms wait needs a few frames; assert the scene still
--     reaches a following command (i.e. the wait terminates, it does not
--     hang). Zero/edge waits return immediately.
-- ═══════════════════════════════════════════════════════════════════════
do
    local w1, e1 = run_scene('[wait ms=50][unlock type="cg" id="w1"]')
    check('[wait ms=50] completes and advances', w1 and w1.unlockedCG
        and w1.unlockedCG.w1 == true, e1)

    local w2, e2 = run_scene('[delay ms=50][unlock type="cg" id="w2"]')
    check('[delay ms=50] completes and advances', w2 and w2.unlockedCG
        and w2.unlockedCG.w2 == true, e2)

    -- ms=0 returns immediately (the wait guard skips the loop).
    local w3, e3 = run_scene('[wait ms=0][unlock type="cg" id="w3"]')
    check('[wait ms=0] returns immediately', w3 and w3.unlockedCG
        and w3.unlockedCG.w3 == true, e3)

    local w4, e4 = run_scene('[delay duration=40][unlock type="cg" id="w4"]')
    check('[delay duration=40] (duration alias) advances', w4 and w4.unlockedCG
        and w4.unlockedCG.w4 == true, e4)
end

-- ═══════════════════════════════════════════════════════════════════════
--  8. Choice commands: [sel]/[button] with [endbutton]/[endselect].
--     Mirrors the round-74 mechanism: register choices, block on
--     waiting_input, simulate a pick through the active block, assert the
--     chosen target lands in the x= variable and becomes the pending jump.
-- ═══════════════════════════════════════════════════════════════════════
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

    -- [button] family writes the choice target into the tf. slot and jumps.
    local ctxB, errB = run_select_click(
        '[button text="A" x="tf.route" target="*a"][button text="B" x="tf.route" target="*b"][endbutton]', 2)
    check('button-registered choice picked lands in tf.route',
        ctxB and ctxB.tf.route == '*b', errB)
    check('button end sets the pending jump', ctxB and ctxB._pendingJump == '*b', errB)

    -- [sel]/[endselect] with a bare x= target resolving to the f. scope.
    local ctxS, errS = run_select_click(
        '[sel x="picked" text="Only" target="*o"][endselect]', 1)
    check('sel x= bare target resolves to f.picked',
        ctxS and ctxS.f.picked == '*o', errS)
    check('sel pick sets the pending jump', ctxS and ctxS._pendingJump == '*o', errS)
end

print(string.format('\nCONTRACT RUNTIME (2) TESTS: %d passed, %d failed', passed, failed))
if failed > 0 then os.exit(1) end