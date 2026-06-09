-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/save.lua
--  Phase 6: KAG save/load tag handlers — [save], [load]
--  Serializes KAG context to JSON → C++ SaveManager → disk.
--  All I/O goes through KAG.save_game / KAG.load_game (C++ bindings).
-- =============================================================================

-- Cache sandbox-vulnerable globals before lockdown
local _type    = type

local SaveCommands = {}

-- ═══════════════════════════════════════════════════════════════════════════
--  Internal: serialize KAG context values to a flat Lua table
--  Captures: f (global flags), sf (system flags), token_index, scene_path
--  Does NOT capture: tf (temp flags), co, call_stack (rebuild on load)
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

    -- Scene path
    state.scene_path = ctx.currentScene or ""

    -- Backlog (recent N entries)
    state.backlog = {}
    if ctx.backlog then
        for i = math.max(1, #ctx.backlog - 99), #ctx.backlog do
            local entry = ctx.backlog[i]
            state.backlog[#state.backlog + 1] = {
                text      = entry.text or "",
                voice     = entry.voice or "",
                timestamp = entry.timestamp or 0,
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

    -- Schema version (engine-defined)
    state.schema_version = 1

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
    local sceneName = ctx.currentScene or "unknown"
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
    local state, meta = KAG.load_game(slot)`r`n    if not state or type(state) ~= "table" then
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

    -- Restore backlog
    if state.backlog then
        ctx.backlog = {}
        for _, entry in ipairs(state.backlog) do
            ctx.backlog[#ctx.backlog + 1] = entry
        end
    end

    -- Set token position for resume
    ctx.token_index = state.token_index or 1

    -- Set scene path for reload
    if state.scene_path and #state.scene_path > 0 then
        ctx.currentScene = state.scene_path
        -- Set stop_flag so the current script execution stops
        -- and the engine reloads from the saved scene
        ctx.stop_flag = true
        ctx._pendingLoadScene = state.scene_path
        ctx._pendingLoadToken = state.token_index
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

function SaveCommands.listsaves(ctx, params)
    local saves = KAG.list_saves()
    ctx.sf = ctx.sf or {}
    ctx.sf.save_list = saves
    -- Also set as tf for immediate access
    ctx.tf = ctx.tf or {}
    ctx.tf.save_list = saves
end

return SaveCommands
