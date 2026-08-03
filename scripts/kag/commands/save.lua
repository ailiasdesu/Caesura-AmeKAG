-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/save.lua
--  Phase 6: KAG save/load tag handlers — [save], [load]
--  Serializes KAG context to JSON → C++ SaveManager → disk.
--  All I/O goes through KAG.save_game / KAG.load_game (C++ bindings).
-- =============================================================================

-- Cache sandbox-vulnerable globals before lockdown
local _type    = type

local System = require("system")

local SaveCommands = {}

-- Scene-path allowlist (pure, unit-tested). Real layout: production entry
-- is scripts/demo_story.ks and scheduler jumps write assets/script/<t>.ks
-- (singular); assets/scripts and demo/ are also accepted. Any ".." segment
-- rejects so traversal cannot smuggle an arbitrary file past the prefix
-- check. Non-string/empty -> false.
function SaveCommands._safeScenePath(sp)
    if _type(sp) ~= "string" or #sp == 0 then return false end
    if sp:find("..", 1, true) then return false end
    if sp:find("^scripts/") == 1 or sp:find("^assets/script/") == 1
        or sp:find("^assets/scripts/") == 1 or sp:find("^demo/") == 1
        or sp:find("^tests/scripts/") == 1 then
        return sp:find("%.ks$") ~= nil
    end
    return false
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Internal: serialize KAG context values to a flat Lua table
--  Captures: f (global flags), sf (system flags), token_index, scene_path
--  Does NOT capture: tf (temp flags), co
-- ═══════════════════════════════════════════════════════════════════════════

local function capture_state(ctx)
    local state = {}

    -- f-variables (global flags)
    state.f = {}
    if ctx.f then
        for k, v in pairs(ctx.f) do
            if _type(k) == "string" then
                state.f[k] = v
            end
        end
    end

    -- sf-variables (system flags)
    state.sf = {}
    if ctx.sf then
        for k, v in pairs(ctx.sf) do
            if _type(k) == "string" then
                state.sf[k] = v
            end
        end
    end

    -- Token position
    state.token_index = ctx.token_index or 1

    -- Scene path: scheduler writes current_scene (snake_case) on
    -- jump/call/link; currentScene is the legacy camelCase alias.
    state.scene_path = ctx.current_scene or ctx.currentScene or ""

    -- Backlog (recent N entries)
    state.backlog = {}
    if ctx.backlog then
        for i = math.max(1, #ctx.backlog - 99), #ctx.backlog do
            local entry = ctx.backlog[i]
            state.backlog[#state.backlog + 1] = {
                name        = entry.name or "",
                text        = entry.text or "",
                voice       = entry.voice or "",
                timestamp   = entry.timestamp or 0,
                scene       = entry.scene or "",
                token_index = entry.token_index or 1,
            }
        end
    end

    -- Label map (for jump targets)
    state.label_map = {}
    if ctx.labelMap then
        for k, v in pairs(ctx.labelMap) do
            if _type(k) == "string" and _type(v) == "number" then
                state.label_map[k] = v
            end
        end
    end

    -- Save description
    state.description = ctx.saveDescription or ""

    -- Unlock state (gallery CGs + music room tracks)
    state.unlockedCG = {}
    if ctx.unlockedCG then
        for k, v in pairs(ctx.unlockedCG) do
            state.unlockedCG[k] = v
        end
    end
    state.unlockedMusic = {}
    if ctx.unlockedMusic then
        for k, v in pairs(ctx.unlockedMusic) do
            state.unlockedMusic[k] = v
        end
    end

    -- Call stack (for [call]/[return] nested execution)
    state.call_stack = {}
    if ctx.call_stack then
        for _, frame in ipairs(ctx.call_stack) do
            table.insert(state.call_stack, {
                tokens = frame.tokens,
                index  = frame.index or 1,
            })
        end
    end

    -- Schema version (engine-defined)
    state.schema_version = 2  -- bumped: added unlock state

    -- [R6-FIX] Persist skip/auto mode so they survive save/load cycles
    state.skip_mode = ctx.skip_mode or false
    state.auto_mode = ctx.auto_mode or false
    -- Persist the UI language (settings hot-switch) so a reloaded save
    -- restores the player's locale instead of resetting to zh.
    state.language = (ctx.settingsValues and ctx.settingsValues.language)
        or require("i18n").current or "zh"

    -- [R7-FIX] Persist seen flags for Read Skip
    state.seen_scenes = ctx.seen_scenes or {}

    return state
end

-- ═══════════════════════════════════════════════════════════════════════════
function SaveCommands.save(ctx, params)
    local slot = tonumber(params.slot or params[1] or 0)
    local desc = params.desc or params.description or ""

    -- Capture context state
    ctx.saveDescription = desc
    local state = capture_state(ctx)



    -- Thumbnail: capture if available (engine provides via ctx)
    local thumbnail = params.thumbnail or ""
    if #thumbnail == 0 and ctx.captureThumbnail then
        thumbnail = ctx.captureThumbnail() or ""
    end

    -- Call C++ SaveManager via KAG binding
    local sceneName = ctx.current_scene or ctx.currentScene or "unknown"
    local tokenIdx  = ctx.token_index or 1

    local ok = KAG.save_game(slot, state, sceneName, tokenIdx, thumbnail)
    -- Phase G8-U1: explicit GC collect after save
    pcall(function() collectgarbage("collect") end)

    if ok then
        print("[SaveCmd] Saved to slot " .. slot .. " (" .. sceneName .. ")")
        -- Set save result flag for UI feedback
        ctx.tf = ctx.tf or {}
        ctx.tf.save_result = "ok"
        ctx.tf.save_slot   = slot
    else
        print("[SaveCmd] Save failed for slot " .. slot)
        ctx.tf = ctx.tf or {}
        ctx.tf.save_result = "error"
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [load slot=0]
--  Load save data → JSON decode → restore ctx state → resume scene
--  After restoring variables, re-executes scene script from the saved position.
-- ═══════════════════════════════════════════════════════════════════════════

function SaveCommands.load(ctx, params)
    local slot = tonumber(params.slot or params[1] or 0)

    -- Call C++ SaveManager via KAG binding
    local state, meta = KAG.load_game(slot)    if not state or type(state) ~= "table" then
        print("[SaveCmd] Load failed for slot " .. slot .. ": " .. tostring(meta or "unknown error"))
        ctx.tf = ctx.tf or {}
        ctx.tf.load_result = "error"
        return
    end



    -- Restore f-variables
    if state.f then
        ctx.f = ctx.f or {}
        for k, v in pairs(state.f) do
            ctx.f[k] = v
        end
    end

    -- Restore sf-variables
    if state.sf then
        ctx.sf = ctx.sf or {}
        for k, v in pairs(state.sf) do
            ctx.sf[k] = v
        end
    end

    -- Restore label map
    if state.label_map then
        ctx.labelMap = ctx.labelMap or {}
        for k, v in pairs(state.label_map) do
            ctx.labelMap[k] = v
        end
    end

    -- Restore unlock state (gallery + music room)
    if state.unlockedCG then
        ctx.unlockedCG = ctx.unlockedCG or {}
        for k, v in pairs(state.unlockedCG) do
            ctx.unlockedCG[k] = v
        end
    end
    if state.unlockedMusic then
        ctx.unlockedMusic = ctx.unlockedMusic or {}
        for k, v in pairs(state.unlockedMusic) do
            ctx.unlockedMusic[k] = v
        end
    end

    -- Restore call stack
    if state.call_stack and #state.call_stack > 0 then
        ctx.call_stack = {}
        for _, frame in ipairs(state.call_stack) do
            table.insert(ctx.call_stack, frame)
        end
    end

    -- Restore backlog (capped: history UI iterates it per frame, and a
    -- crafted save could inflate it to stall the loop)
    if state.backlog then
        ctx.backlog = {}
        local cap = ctx.backlog_max or 500
        for _, entry in ipairs(state.backlog) do
            if #ctx.backlog >= cap then break end
            ctx.backlog[#ctx.backlog + 1] = entry
        end
    end

    -- [R6-FIX] Restore skip/auto mode
    ctx.skip_mode = state.skip_mode or false
    ctx.auto_mode = state.auto_mode or false
    -- Restore the UI language and hot-switch the locale
    if type(state.language) == "string" and #state.language > 0 then
        ctx.settingsValues = ctx.settingsValues or {}
        ctx.settingsValues.language = state.language
        pcall(function() require("i18n").load(state.language) end)
    end

    -- [R7-FIX] Restore seen flags
    ctx.seen_scenes = state.seen_scenes or {}

    -- Set token position for resume
    ctx.token_index = state.token_index or 1

    -- Set scene path for reload (both aliases: the runner reads the
    -- snake_case variant during the coroutine-death window). The path is
    -- attacker-controlled in crafted saves: allowlist it to a .ks under
    -- assets/scripts (or demo/) so [load] cannot point the tokenizer at
    -- an arbitrary readable local file.
    local sp = state.scene_path or ""
    if SaveCommands._safeScenePath(sp) then
        ctx.currentScene = sp
        ctx.current_scene = sp
        -- Set stop_flag so the current script execution stops
        -- and the engine reloads from the saved scene
        ctx.stop_flag = true
        -- The save JSON may carry a crafted call_stack; resume_from_save
        -- would clear it, but drop it here too so a valid-path crafted save
        -- cannot [return] into a forged frame before the resume runs.
        ctx.call_stack = nil
        ctx._pendingLoadScene = state.scene_path
        ctx._pendingLoadToken = state.token_index
    else
        -- Rejected path: still harden state so a crafted save cannot
        -- leave a stale call_stack / non-table seen_scenes live (the
        -- continuing script could hit [return] into a crafted frame).
        ctx.stop_flag = true
        ctx.call_stack = nil
        if type(ctx.seen_scenes) ~= "table" then ctx.seen_scenes = {} end
    end

    -- Set load result flag for UI feedback
    ctx.tf = ctx.tf or {}
    ctx.tf.load_result = "ok"
    ctx.tf.load_slot    = slot
    ctx.tf.load_meta    = meta

    print("[SaveCmd] Loaded slot " .. slot .. " (scene: " ..
          (state.scene_path or "?") .. ", token: " .. (state.token_index or 0) .. ")")
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [listsaves] — populate ctx.sf.save_list with available saves
--  Used by save/load UI to display slots.
-- ═══════════════════════════════════════════════════════════════════════════

-- [saveload mode=save|load] — slot-selection UI (scheduler-driven)
function SaveCommands.saveload(ctx, params)
    local SaveLoad = require("saveload_menu")
    local chosen = SaveLoad.show(ctx, params.mode or params[1] or "save")
    if chosen then
        if chosen.action == "save" then
            SaveCommands.save(ctx, { slot = chosen.slot })
        else
            SaveCommands.load(ctx, { slot = chosen.slot })
        end
    end
    return chosen
end

function SaveCommands.listsaves(ctx, params)
    local saves = KAG.list_saves()
    ctx.sf = ctx.sf or {}
    ctx.sf.save_list = saves
    -- Also set as tf for immediate access
    ctx.tf = ctx.tf or {}
    ctx.tf.save_list = saves
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [saveplace] / [loadplace] — in-memory scene bookmarks
--  Independent of save slots.  No disk writes.
--  Delegates to System.saveplace / System.loadplace (scripts/system.lua).
-- ═══════════════════════════════════════════════════════════════════════════

function SaveCommands.saveplace(ctx, params)
    System.saveplace(ctx)
end

function SaveCommands.loadplace(ctx, params)
    System.loadplace(ctx)
end

return SaveCommands
