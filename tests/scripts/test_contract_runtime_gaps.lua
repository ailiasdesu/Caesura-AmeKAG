-- =============================================================================
--  test_contract_runtime_gaps.lua — runtime execution for KAG commands that
--  had ONLY a schema contract (no runtime handler execution anywhere in the
--  suite). Round audit (README/ROADMAP gap sweep): these handlers existed
--  and were registered, but no test ever invoked them — schema.coerce /
--  type(KAG.x)==function registration checks alone (e.g. test_volume,
--  test_layer_cmds, test_vib_camera, test_sprite_family, test_schema,
--  test_kag3_compat, test_playbgmstop) never exercised the handler BODY.
--
--  Commands covered (12): [cancel] [setvoicevolume] [setsevolume]
--  [playbgmstop] [playstop] [waitforclick] [moveto] [camera] [sprite_fade]
--  [sprite_move] [sprite_scale] [sprite_swap].
--
--  Strategy (mirrors test_contracts_runtime.lua/2): run each through the
--  REAL scheduler (run_scene) headless-safe, assert the scene advances past
--  it (an [unlock] sentinel: the command must execute + not block/crash),
--  and add 1-2 semantic assertions via recording backend/layers mocks.
--
--  This file creates global mocks (_G._CAESURA_BACKEND, package.loaded
--  layers/backend) so it MUST run in the orphan suite in its own isolated
--  subprocess (run_orphan_tests.lua). It must never be merged into the
--  sandbox-locked main suite.
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;scripts/?/init.lua;" .. package.path

-- ---- recording backend / layers mocks (installed BEFORE requiring kag so
--  the command modules capture them) -----------------------------------------
local audioCalls, renderCalls = {}, {}
local sprNode = { x = 0, y = 0, opacity = 255, scaleX = 1.0, scaleY = 1.0,
    texture = nil, visible = true }
local moveArgs, opacityArgs, markCount = {}, {}, 0
local setPosArgs = {}
package.loaded["layers"] = {
    get = function(name)
        if name == "_char_A" or name == "_char_Hero" then return sprNode end
        return nil
    end,
    find = function(name)
        if name == "_char_A" or name == "_char_Hero" then return sprNode end
        return nil
    end,
    set_layer_opacity = function(node, o)
        opacityArgs[#opacityArgs + 1] = o
        node.opacity = o
    end,
    move_layer = function(node, x, y)
        moveArgs[#moveArgs + 1] = { x, y }
        node.x, node.y = x, y
    end,
    mark_dirty = function() markCount = markCount + 1 end,
    set_position = function(layer, x, y, scale, unit)
        setPosArgs[#setPosArgs + 1] = { layer, x, y, scale, unit }
    end,
    set_options = function() end,
    get_layer = function() return nil end,
    add_layer = function() return { visible = true } end,
    ensure = function() return { visible = true } end,
}
_G._CAESURA_BACKEND = {
    audio = function(m, ...)
        audioCalls[#audioCalls + 1] = { m, { n = select('#', ...), arg = { ... } } }
        return true
    end,
    render = function(m, ...)
        renderCalls[#renderCalls + 1] = { m, { n = select('#', ...), arg = { ... } } }
        if m == "load_texture" then return 99 end
        return true
    end,
    platform = function() return true end,
}

-- ---- hard deps (no backend/layers relationship) ----------------------------
local tokenizer = require('tokenizer')
local compiler = require('kag.compiler')
local scheduler = require('scheduler')
require('kag')  -- registers the 12 gap handlers (kag.lua + command modules)

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then passed = passed + 1
    else failed = failed + 1
         print(string.format('  [FAIL] %s -- %s', name, detail or '')) end
end

-- Standard headless runner (mirrors the contract-runtime harness).
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

-- clear recording state between assertions
local function reset_records()
    audioCalls, renderCalls, moveArgs, opacityArgs, markCount, setPosArgs =
        {}, {}, {}, {}, 0, {}
    sprNode.x, sprNode.y, sprNode.opacity = 0, 0, 255
    sprNode.scaleX, sprNode.scaleY, sprNode.texture = 1.0, 1.0, nil
end

print('\n=== Contract Runtime Gap Tests (runtime-exercise audit) ===\n')

-- =============================================================
-- 1. [cancel] — stop voice + cancel_all + clear waiting_input
-- =============================================================
do
    reset_records()
    local ctx, err = run_scene('[cancel][unlock type="cg" id="cancel_ok"]')
    check('[cancel] executes; scene advances past it',
        ctx and ctx.unlockedCG and ctx.unlockedCG.cancel_ok == true, err)
    local stopVoice = false
    for _, c in ipairs(audioCalls) do
        if c[1] == "stop_voice" then stopVoice = true end
    end
    check('[cancel] stops the voice bus (audio_stop voice)',
        stopVoice, 'audioCalls=' .. table.concat((function()
            local t = {} for i, c in ipairs(audioCalls) do t[i] = c[1] end return t end)(), ','))
end

-- =============================================================
-- 2. [setvoicevolume] / 3. [setsevolume] — audio_set_bus_volume
--    (volume clamped 0..1.5; positional + named forms)
-- =============================================================
do
    reset_records()
    local ctx, err = run_scene('[setvoicevolume 0.5][unlock type="cg" id="svv_ok"]')
    check('[setvoicevolume] executes; scene advances',
        ctx and ctx.unlockedCG and ctx.unlockedCG.svv_ok == true, err)
    local svv = false
    for _, c in ipairs(audioCalls) do
        if c[1] == "set_bus_volume" and c[2].arg[1] == "voice" then svv = c[2].arg[2] end
    end
    check('[setvoicevolume] routes volume to voice bus',
        svv == 0.5, 'vol=' .. tostring(svv))

    reset_records()
    local ctx2, err2 = run_scene('[setsevolume volume=0.4][unlock type="cg" id="sse_ok"]')
    check('[setsevolume] executes; scene advances',
        ctx2 and ctx2.unlockedCG and ctx2.unlockedCG.sse_ok == true, err2)
    local sse = false
    for _, c in ipairs(audioCalls) do
        if c[1] == "set_bus_volume" and c[2].arg[1] == "se" then sse = c[2].arg[2] end
    end
    check('[setsevolume] routes volume to se bus',
        sse == 0.4, 'vol=' .. tostring(sse))
end

-- =============================================================
-- 4. [playbgmstop] — stop bgm (fadeout path) then optional play
-- =============================================================
do
    reset_records()
    local ctx, err = run_scene('[playbgmstop file="b.ogg" fadeout=300][unlock type="cg" id="pbs_ok"]')
    check('[playbgmstop] executes; scene advances',
        ctx and ctx.unlockedCG and ctx.unlockedCG.pbs_ok == true, err)
    local stopped = false
    for _, c in ipairs(audioCalls) do
        if c[1] == "stop_bgm" then stopped = true end
    end
    check('[playbgmstop] stops the bgm bus', stopped,
        'audioCalls=' .. table.concat((function()
            local t = {} for i, c in ipairs(audioCalls) do t[i] = c[1] end return t end)(), ','))
end

-- =============================================================
-- 5. [playstop] — delegates to stopbgm (KAG3 alias)
-- =============================================================
do
    reset_records()
    local ctx, err = run_scene('[playstop][unlock type="cg" id="ps_ok"]')
    check('[playstop] executes; scene advances',
        ctx and ctx.unlockedCG and ctx.unlockedCG.ps_ok == true, err)
    local stopped = false
    for _, c in ipairs(audioCalls) do
        if c[1] == "stop_bgm" then stopped = true end
    end
    check('[playstop] stops the bgm bus', stopped,
        'audioCalls=' .. table.concat((function()
            local t = {} for i, c in ipairs(audioCalls) do t[i] = c[1] end return t end)(), ','))
end

-- =============================================================
-- 6. [waitforclick] — blocking on waiting_input; runner clears it
-- =============================================================
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
            if ctx.waiting_input then sawWaiting = true; ctx.waiting_input = false end
            n = n + 1
        end
        if coroutine.status(co) ~= 'dead' then return nil, 'blocked after 800 iters' end
        return ctx
    end
    sawWaiting = false
    local ctx, err = run_capture('[waitforclick][unlock type="cg" id="wfc_ok"]')
    check('[waitforclick] raises waiting_input blocking flag', ctx and sawWaiting, err)
    check('[waitforclick] completes; scene advances past it',
        ctx and ctx.unlockedCG and ctx.unlockedCG.wfc_ok == true, err)
end

-- =============================================================
-- 7. [moveto] — routes through layers.set_position (KAG3 alias)
--    (left/top map onto x/y; default layer fg)
-- =============================================================
do
    reset_records()
    local ctx, err = run_scene('[moveto left=5 top=6][unlock type="cg" id="mv_ok"]')
    check('[moveto] executes; scene advances',
        ctx and ctx.unlockedCG and ctx.unlockedCG.mv_ok == true, err)
    check('[moveto] calls layers.set_position on fg',
        #setPosArgs == 1 and setPosArgs[1][1] == "fg"
        and setPosArgs[1][2] == 5 and setPosArgs[1][3] == 6,
        'setPos=' .. table.concat((function()
            local t = {} for i, a in ipairs(setPosArgs) do
                t[i] = '[' .. tostring(a and a[1]) .. ',' .. tostring(a and a[2]) .. ']' end
            return t end)(), ','))
end

-- =============================================================
-- 8. [camera] — screen-offset pan (time=0 -> immediate position)
-- =============================================================
do
    reset_records()
    local ctx, err = run_scene('[camera x=10 y=5 time=0][unlock type="cg" id="cam_ok"]')
    check('[camera] executes; scene advances',
        ctx and ctx.unlockedCG and ctx.unlockedCG.cam_ok == true, err)
    local off = false
    for _, c in ipairs(renderCalls) do
        if c[1] == "set_screen_offset" then off = { c[2].arg[1], c[2].arg[2] } end
    end
    check('[camera time=0] sets screen offset (10,5)',
        off and off[1] == 10 and off[2] == 5,
        'offset=' .. tostring(off and off[1]) .. ',' .. tostring(off and off[2]))
end

-- =============================================================
-- 9. [sprite_fade] — set layer opacity (time=0 immediate)
-- =============================================================
do
    reset_records()
    local ctx, err = run_scene('[sprite_fade speaker="A" to=128 time=0][unlock type="cg" id="sf_ok"]')
    check('[sprite_fade] executes; scene advances',
        ctx and ctx.unlockedCG and ctx.unlockedCG.sf_ok == true, err)
    check('[sprite_fade] sets _char_A opacity to 128',
        #opacityArgs == 1 and opacityArgs[1] == 128 and sprNode.opacity == 128,
        'opacity=' .. tostring(sprNode.opacity))
end

-- =============================================================
-- 10. [sprite_move] — move the _char_<speaker> layer (time=0 immediate)
-- =============================================================
do
    reset_records()
    local ctx, err = run_scene('[sprite_move speaker="A" x=100 y=50 time=0][unlock type="cg" id="sm_ok"]')
    check('[sprite_move] executes; scene advances',
        ctx and ctx.unlockedCG and ctx.unlockedCG.sm_ok == true, err)
    check('[sprite_move] moves _char_A to (100,50)',
        #moveArgs == 1 and moveArgs[1][1] == 100 and moveArgs[1][2] == 50
        and sprNode.x == 100 and sprNode.y == 50,
        'move=' .. (moveArgs[1] and (moveArgs[1][1] .. ',' .. moveArgs[1][2]) or 'none'))
end

-- =============================================================
-- 11. [sprite_scale] — set _char_ layer scale (time=0 immediate)
-- =============================================================
do
    reset_records()
    local ctx, err = run_scene('[sprite_scale speaker="A" scale=2 time=0][unlock type="cg" id="scl_ok"]')
    check('[sprite_scale] executes; scene advances',
        ctx and ctx.unlockedCG and ctx.unlockedCG.scl_ok == true, err)
    check('[sprite_scale] sets _char_A scale to 2.0',
        sprNode.scaleX == 2.0 and sprNode.scaleY == 2.0 and markCount >= 1,
        'scale=' .. tostring(sprNode.scaleX) .. ',marks=' .. markCount)
end

-- =============================================================
-- 12. [sprite_swap] — re-dress: load texture + assign to layer
-- =============================================================
do
    reset_records()
    local ctx, err = run_scene('[sprite_swap speaker="Hero" sprite="hero.png"][unlock type="cg" id="ssw_ok"]')
    check('[sprite_swap] executes; scene advances',
        ctx and ctx.unlockedCG and ctx.unlockedCG.ssw_ok == true, err)
    local loaded = 0
    local loadedName = nil
    for _, c in ipairs(renderCalls) do
        if c[1] == "load_texture" then loaded = loaded + 1; loadedName = c[2].arg[1] end
    end
    check('[sprite_swap] loads the sprite texture',
        loaded >= 1 and loadedName == "hero.png", 'loaded=' .. loaded)
    check('[sprite_swap] assigns texture to the _char_Hero layer',
        sprNode.texture == 99, 'texture=' .. tostring(sprNode.texture))
end

print(string.format('\nCONTRACT RUNTIME GAP TESTS: %d passed, %d failed', passed, failed))
if failed > 0 then os.exit(1) end
