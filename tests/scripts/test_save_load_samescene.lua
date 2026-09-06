-- U11: every load entry restores the saved continuation independently of the
-- command that requested it. Run alone: this fixture owns its native mocks.
package.path = 'scripts/?.lua;scripts/?/init.lua;scripts/kag/?.lua;scripts/kag/commands/?.lua;' .. package.path

local DIR_A = 'demo/t62_saveload'
local DIR_B = 'tests/projects/caesura-t62'
local function callable(fields)
    return setmetatable(fields or {}, { __index = function(self, key)
        if type(key) ~= 'string' then return nil end
        local fn = function() return true end
        rawset(self, key, fn)
        return fn
    end })
end
local function copy(value)
    if type(value) ~= 'table' then return value end
    local result = {}
    for key, child in pairs(value) do result[key] = copy(child) end
    return result
end

-- Storage is a value-copy fake; parsing, scheduler, save handlers, scene
-- preparation and native runner commit are production implementations.
local savedSlots, writes = {}, {}
_G.KAG = callable({
    save_game = function(slot, state, scene, token)
        savedSlots[slot] = { state = copy(state), scene = scene, token = token }
        writes[slot] = (writes[slot] or 0) + 1
        return true
    end,
    load_game = function(slot)
        local hit = savedSlots[slot]
        if hit then return copy(hit.state), { slot = slot } end
        return nil, 'no-save-' .. tostring(slot)
    end,
    list_saves = function() return {} end,
    is_voice_playing = function() return false end,
    is_bgm_playing = function() return false end,
    get_active_voices = function() return 0 end,
})
_G.Render, _G.DevCore, _G.Engine = callable(), callable(), callable()
_G.backend = callable({
    is_voice_playing = function() return false end,
    is_bgm_playing = function() return false end,
    get_active_voices = function() return 0 end,
})

-- Only fixture scene reads are virtual. Real flow/tokenizer compile these
-- sources; their disposable bytecode caches are disabled, with no disk writes.
local sources, original_open = {}, io.open
io.open = function(path, mode)
    if type(path) == 'string' and path:match('^cache/ksc/')
        and (path:find('t62_saveload', 1, true) or path:find('caesura%-t62')) then
        return nil, 'fixture cache disabled'
    end
    local source = sources[path]
    if not source then return original_open(path, mode) end
    if mode and not mode:match('^r') then return nil, 'read-only fixture' end
    local offset = 0
    return {
        seek = function(_, origin, delta)
            offset = (origin == 'end' and #source or origin == 'cur' and offset or 0) + (delta or 0)
            return offset
        end,
        read = function(_, amount)
            if type(amount) ~= 'number' then
                local result = source:sub(offset + 1)
                offset = #source
                return result
            end
            if offset >= #source then return nil end
            local result = source:sub(offset + 1, offset + amount)
            offset = offset + #result
            return result
        end,
        close = function() return true end,
    }
end

-- Stub only the user's menu selection, keeping the real [saveload] handler.
local menu_calls, menu_slot = {}, 1
package.loaded['saveload_menu'] = { show = function(_, mode)
    menu_calls[#menu_calls + 1] = mode
    return { action = mode, slot = menu_slot }
end }
local runner = require('kag_runner')
local save = require('kag.commands.save')
local passed, failed = 0, 0
local function check(name, condition, detail)
    if condition then passed = passed + 1
    else
        failed = failed + 1
        print('  [FAIL] ' .. name .. ' -- ' .. tostring(detail or ''))
    end
end

local STORY = [[
[set var="f.before" value=1]
[save slot=1]
[set var="f.replayed" value=1]
[ch name="N" text="MARKER"]
[if exp="tf.load_result == 'ok'"]
[set var="f.branch" value=1]
[else]
[set var="f.future" value=1]
[p]
[if exp="tf.menu == true"]
[saveload mode=load]
[else]
[load slot=1]
[endif]
[endif]
[set var="f.after" value=1]
[end]
]]
sources[DIR_A .. '/story.ks'], sources[DIR_B .. '/story.ks'] = STORY, STORY
sources[DIR_A .. '/loader.ks'] = '[load slot=1]\n[set var="f.loader_tail" value=1]\n[end]'

local function step()
    local ok, reason = runner.update(0.016)
    return runner.get_ctx(), reason, ok
end
local function drive_tick()
    local current = runner.get_ctx()
    if current and current.waiting_input then
        local ok, reason = runner.on_click()
        return runner.get_ctx(), reason, ok
    end
    return step()
end
local function until_true(predicate, limit)
    for _ = 1, limit or 300 do
        if predicate(runner.get_ctx()) then return true end
        local _, reason = drive_tick()
        if reason == 'ended' then return predicate(runner.get_ctx()) end
    end
    return predicate(runner.get_ctx())
end
local function finish()
    for _ = 1, 300 do
        local current, reason = drive_tick()
        if reason == 'ended' then return true, current end
    end
    return false, runner.get_ctx()
end
local function markers(ctx)
    local result = {}
    for _, entry in ipairs(ctx and ctx.backlog or {}) do
        if entry.text == 'MARKER' then result[#result + 1] = entry.text end
    end
    return table.concat(result, '|')
end

print('=== Native saved-continuation entry equivalence + allowlist ===')
for _, entry in ipairs({ 'inline', 'external', 'cross-scene', 'menu', 'project-path' }) do
    runner.stop()
    savedSlots, writes, menu_calls = {}, {}, {}
    local path = (entry == 'project-path' and DIR_B or DIR_A) .. '/story.ks'
    assert(runner.start(path))
    runner.get_ctx().tf.menu = entry == 'menu'
    local parked = until_true(function(ctx)
        return ctx and ctx.waiting_input and ctx._executing_command == 'p'
            and ctx.f.future == 1
    end)
    check(entry .. ': producer pauses before requesting load', parked)
    local owner, hit = runner.get_ctx(), savedSlots[1]
    assert(hit, 'fixture must produce a real SaveCommands snapshot')
    check(entry .. ': [save] captures its next token', hit.state.token_index == 3, hit.state.token_index)
    check(entry .. ': source executes marker before loading', owner.f.replayed == 1 and markers(owner) == 'MARKER')
    check(entry .. ': future variable exists before loading', owner.f.future == 1)
    local old_co = owner.co
    if entry == 'cross-scene' then
        runner.stop()
        assert(runner.start(DIR_A .. '/loader.ks'))
        owner = runner.get_ctx()
        old_co = owner.co
        owner.f.future = 1
    elseif entry == 'external' then
        assert(save.load(owner, { slot = 1 }))
    end
    local committed = until_true(function(ctx) return ctx ~= owner and ctx.tf.load_result == 'ok' end)
    local restored = runner.get_ctx()
    check(entry .. ': restore publishes a fresh session', committed)
    check(entry .. ': commit uses saved cursor', restored._resume_index == hit.state.token_index,
        tostring(restored._resume_index) .. ' vs ' .. tostring(hit.state.token_index))
    check(entry .. ': commit removes future values before replay',
        restored.f.future == nil and restored.f.replayed == nil and restored.f.before == 1)
    check(entry .. ': commit restores empty control stacks',
        #(restored._ifStack or {}) == 0 and #(restored._forStack or {}) == 0)
    check(entry .. ': old coroutine closed', coroutine.status(old_co) == 'dead')
    step()
    check(entry .. ': first resumed command performs saved continuation',
        restored.f.replayed == 1 and restored.token_index == hit.state.token_index,
        'replayed=' .. tostring(restored.f.replayed) .. ', token=' .. tostring(restored.token_index))
    local ended, final = finish()
    check(entry .. ': explicit load-result branch finishes', ended and final.f.after == 1)
    check(entry .. ': restored branch executes', final.f.branch == 1, final.f.branch)
    check(entry .. ': restored visible marker replays exactly once', markers(final) == 'MARKER', markers(final))
    check(entry .. ': future-only and loader-tail values stay absent',
        final.f.future == nil and final.f.loader_tail == nil)
    check(entry .. ': resume does not rewrite the save slot', writes[1] == 1, writes[1])
    if entry == 'menu' then
        check('menu: real saveload command requests one load selection', #menu_calls == 1 and menu_calls[1] == 'load')
    end
    print('  [' .. entry .. '] saved=' .. hit.state.token_index
        .. ', committed=' .. tostring(committed) .. ', marker=' .. markers(final))
end

-- A menu save completes the saveload command. Restoring it must not open the
-- menu again; the snapshot points at the next script command, like [save].
do
    runner.stop()
    menu_calls, menu_slot = {}, 3
    sources[DIR_A .. '/menu-save.ks'] = '[saveload mode=save]\n[set var="f.menu_next" value=1]\n[end]'
    assert(runner.start(DIR_A .. '/menu-save.ks'))
    local ended = finish()
    local hit = savedSlots[3]
    check('menu-save: confirmed selection writes once', ended and writes[3] == 1)
    check('menu-save: snapshot skips completed saveload command', hit and hit.state.token_index == 2)
    assert(save.load(runner.get_ctx(), { slot = 3 }))
    local restored = runner.get_ctx()
    check('menu-save: restored cursor is next command', restored._resume_index == 2)
    step()
    check('menu-save: next command executes without reopening menu', restored.f.menu_next == 1 and #menu_calls == 1)
end

-- Unconditional load-back is a script loop. Observe two restores then cancel
-- externally; no engine cursor skip may fabricate execution of the tail.
do
    runner.stop()
    sources[DIR_A .. '/repeat.ks'] = [[
[save slot=2]
[set var="f.replayed" value=1]
[load slot=2]
[set var="f.unreachable" value=1]
[end]
]]
    assert(runner.start(DIR_A .. '/repeat.ks'))
    local previous, restores, exact, tail_executed = runner.get_ctx(), 0, true, false
    for _ = 1, 80 do
        local current, reason = step()
        if current.f.unreachable ~= nil then tail_executed = true end
        if current ~= previous then
            restores = restores + 1
            local hit = savedSlots[2]
            exact = exact and current._resume_index == hit.state.token_index
                and current.f.replayed == nil
            previous = current
            if restores == 2 then break end
        end
        if reason == 'ended' then break end
    end
    local co = runner.get_ctx().co
    runner.stop()
    check('repeat: reaches two real restores within bounded drive', restores == 2, restores)
    check('repeat: each commit returns exactly to saved continuation', restores == 2 and exact)
    check('repeat: no command after unconditional load is fabricated', not tail_executed)
    check('repeat: external cancellation closes repeated-load session', not co or coroutine.status(co) == 'dead')
    check('repeat: loop replays load, not save', writes[2] == 1, writes[2])
end

-- Preserve the original packaged-scene allowlist and traversal coverage.
check('allowlist: tests/projects/golden_vn/story.ks accepted', save._safeScenePath('tests/projects/golden_vn/story.ks') == true)
check('allowlist: projects/my_game/story.ks accepted', save._safeScenePath('projects/my_game/story.ks') == true)
check('allowlist: tests/projects traversal rejected', save._safeScenePath('tests/projects/../secret.ks') == false)
check('allowlist: projects traversal rejected', save._safeScenePath('projects/../evil.ks') == false)
check('allowlist: non-.ks rejected', save._safeScenePath('tests/projects/foo.txt') == false)
check('allowlist: absolute path rejected', save._safeScenePath('/etc/passwd') == false)
io.open = original_open
print(string.format('save/load same-scene + allowlist tests: %d passed, %d failed', passed, failed))
os.exit(failed == 0 and 0 or 1)
