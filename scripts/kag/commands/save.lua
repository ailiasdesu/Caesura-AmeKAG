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
-- C++ bindings live on the global KAG table (set by KAGBinding.cpp);
-- direct-API contexts (tests, editor) have none -- resolve like
-- backend.lua does (audit fix).
local function kag_binding(name)
    local g = rawget(_G, "KAG")
    local v = g and g[name]
    if type(v) == "function" then return v end
    return nil
end

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
                -- Pre-localize source: lets the language hot-switch
                -- redraw re-localize a restored backlog too. Absent in
                -- older saves -- those entries keep their stored text.
                src         = entry.src,
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

    -- Live loop/branch stacks (round 75): scheduler.run hoisted them to
    -- ctx so a save taken INSIDE a [for]/[while] body (or an [if]/[case]
    -- chain) can restore them on [load] -- otherwise the re-spawned run
    -- starts with empty stacks and the enclosing loop silently ends
    -- (round-74 defect). Entries are plain JSON-able values (var/endv/
    -- step/pos/ended for for; pos/ended for while; booleans for if and
    -- switch). Deep-copy defensively; missing/non-table = no restore.
    local function copyStack(t)
        if type(t) ~= "table" or #t == 0 then return nil end
        local out = {}
        for i2, e in ipairs(t) do out[i2] = e end
        return out
    end
    state.loop_stacks = {
        for_ = copyStack(ctx._forStack),
        while_ = copyStack(ctx._whileStack),
        if_ = copyStack(ctx._ifStack),
        switch = copyStack(ctx._switchStack),
    }

    -- Schema version (engine-defined)
    state.schema_version = 2  -- bumped: added unlock state

    -- [R6-FIX] Persist skip/auto mode so they survive save/load cycles
    state.skip_mode = ctx.skip_mode or false
    state.auto_mode = ctx.auto_mode or false
    -- [nvl] mode persists with the save (a reloaded save must resume the
    -- same full-screen text mode, not fall back to the message window).
    state.nvl_mode = ctx.nvl_mode or false
    -- [voice_off] muting persists with the save (audit: it was the only
    -- playback setting NOT captured -- reload reset the mute).
    state.voice_muted = ctx.voice_muted or false
    -- Persist the UI language (settings hot-switch) so a reloaded save
    -- restores the player's locale instead of resetting to zh.
    state.language = (ctx.settingsValues and ctx.settingsValues.language)
        or require("i18n").current or "zh"

    -- [R7-FIX] Persist seen flags for Read Skip
    state.seen_scenes = ctx.seen_scenes or {}
    state.seen_endings = ctx.seen_endings or {}

    -- Visual/text state (audit: saves restored logic but not the
    -- message-window style or font -- a reloaded save reset the look).
    -- Only plain string/number fields are captured (no handles/tables).
    if ctx.text_state and _type(ctx.text_state) == "table" then
        local ts = ctx.text_state
        local vis = {}
        if _type(ts.font_face) == "string" then vis.font_face = ts.font_face end
        if _type(ts.font_size) == "number" then vis.font_size = ts.font_size end
        if _type(ts.font_color) == "string" then vis.font_color = ts.font_color end
        if next(vis) then state.text_state = vis end
    end
    if ctx.textbox_style and _type(ctx.textbox_style) == "table" then
        local st = ctx.textbox_style
        if _type(st.w) == "number" and _type(st.h) == "number" then
            state.textbox_style = {
                x = st.x or 0, y = st.y or 0, w = st.w, h = st.h,
                color = _type(st.color) == "string" and st.color or "0,0,0",
                opacity = st.opacity or 200,
                visible = st.visible ~= false,
            }
        end
    end

    return state
end

-- ═══════════════════════════════════════════════════════════════════════════
-- Neo-Genesis contracts: slot typed + bounded 0..99 (matches the C++
-- SaveManager guard; out-of-range values clamp to the bound).
require("kag.schema").define("save", {
    _meta = { category = "save", blocking = true, desc = "KAG3-compatible save command" },
    -- NO default: coerce would inject slot=0 and shadow the bare
    -- positional [save 3] (same pattern as the wait aliases).
    -- min = -2: named negative slots (system -1/-2) must flow through
    -- to the C++ guard, not clamp to 0 (review should-fix).
    slot = { type = "number", min = -2, max = 99 },
})
require("kag.schema").define("load", {
    _meta = { category = "save", blocking = true, desc = "KAG3-compatible load command" },
    -- NO default: coerce would inject slot=0 and shadow the bare
    -- positional [save 3] (same pattern as the wait aliases).
    -- min = -2: named negative slots (system -1/-2) must flow through
    -- to the C++ guard, not clamp to 0 (review should-fix).
    slot = { type = "number", min = -2, max = 99 },
})

-- Resolve the save slot: named slot= (schema-typed 0..99), else the
-- KAG3 bare positional [save 1], clamped here too (the numeric key
-- bypasses schema coerce -- same pattern as the wait clamp).
SaveCommands.capture_state = capture_state

local function resolve_slot(params)
    -- tonumber(params.slot): a direct caller could pass a string
    -- (review nit -- coerce + all callers pass numbers today)
    local slot = tonumber(params.slot) or tonumber(params[1]) or 0
    -- NEGATIVE slots are SYSTEM slots (quicksave=-1, autosave=-2):
    -- do NOT clamp them to 0 -- that would make F5/autosave silently
    -- overwrite the player's manual slot 0 (security review warn).
    -- They pass through; the C++ SaveManager 0..99 guard rejects them,
    -- so system slots stay inert until a dedicated mapping lands.
    if slot < 0 then return slot end
    if slot > 99 then return 99 end
    return math.floor(slot)
end

function SaveCommands.save(ctx, params)
    local slot = resolve_slot(params)
    local desc = params.desc or params.description or ""

    -- Capture context state
    ctx.saveDescription = desc
    local state = capture_state(ctx)



    -- Thumbnail: capture if available (engine provides via ctx; the
    -- KAG.capture_thumbnail binding is the fallback -- audit: saves
    -- previously had NO thumbnail, the C++ capture path was never
    -- wired into the save flow)
    local thumbnail = params.thumbnail or ""
    if #thumbnail == 0 then
        if ctx.captureThumbnail then
            thumbnail = ctx.captureThumbnail() or ""
        else
            local okT, thumb = pcall(function()
                return kag_binding("capture_thumbnail")()
            end)
            if okT and type(thumb) == "string" and #thumb > 0 then
                thumbnail = thumb
            end
        end
    end

    -- Call C++ SaveManager via KAG binding
    local sceneName = ctx.current_scene or ctx.currentScene or "unknown"
    local tokenIdx  = ctx.token_index or 1

    local ok = kag_binding("save_game")(slot, state, sceneName, tokenIdx, thumbnail)
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
    local slot = resolve_slot(params)

    -- Call C++ SaveManager via KAG binding
    local state, meta = kag_binding("load_game")(slot)    if not state or type(state) ~= "table" then
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
    -- Whitelist-normalize (security LOW): `or false` is a nil-guard, not
    -- a type-guard -- a crafted save could inject a truthy non-boolean
    -- (behaviorally equivalent, but the pattern should not spread).
    local sm = state.skip_mode
    ctx.skip_mode = (sm == true or sm == "seen") and sm or false
    ctx.auto_mode = (state.auto_mode == true)
    ctx.voice_muted = (state.voice_muted == true)
    ctx.nvl_mode = (state.nvl_mode == true)
    -- Restore visual/text state (audit completion)
    if type(state.text_state) == "table" then
        ctx.text_state = ctx.text_state or {}
        if type(state.text_state.font_face) == "string" then
            ctx.text_state.font_face = state.text_state.font_face
        end
        if type(state.text_state.font_size) == "number" then
            ctx.text_state.font_size = state.text_state.font_size
        end
        if type(state.text_state.font_color) == "string" then
            ctx.text_state.font_color = state.text_state.font_color
        end
    end
    if _type(state.textbox_style) == "table" then
        local st = state.textbox_style
        -- EVERY numeric field type-guarded (review warn: h/x/y/opacity
        -- were nil-guards only -- a crafted save injected a string/table
        -- that crashed the next [cl] rebuild via math.floor(opacity))
        if _type(st.w) == "number" and _type(st.h) == "number"
           and _type(st.x) == "number" and _type(st.y) == "number"
           and _type(st.opacity) == "number" then
            ctx.textbox_style = {
                x = st.x, y = st.y, w = st.w, h = st.h,
                color = _type(st.color) == "string" and st.color or "0,0,0",
                opacity = st.opacity,
                visible = st.visible ~= false,
            }
        end
    end

    -- Restore the UI language and hot-switch the locale
    if type(state.language) == "string" and #state.language > 0 then
        ctx.settingsValues = ctx.settingsValues or {}
        ctx.settingsValues.language = state.language
        pcall(function() require("i18n").load(state.language) end)
    end

    -- [R7-FIX] Restore seen flags -- type-guarded: a crafted save with a
    -- non-table seen_scenes would crash the first on_click (security LOW).
    ctx.seen_scenes =
        (type(state.seen_scenes) == "table") and state.seen_scenes or {}
    ctx.seen_endings =
        (type(state.seen_endings) == "table") and state.seen_endings or {}

    -- Restore live loop/branch stacks (round 75): the re-spawned
    -- scheduler.run consumes this marker at entry. Type-guarded -- a
    -- crafted save may carry anything here; entries are only replayed
    -- if they look like the arrays capture_state wrote.
    local ls = state.loop_stacks
    if type(ls) == "table" then
        local marker = {}
        for k2, arr in pairs(ls) do
            if type(arr) == "table" then marker[k2] = arr end
        end
        if next(marker) then ctx._resumeLoopStacks = marker end
    end

    -- Set token position for resume
    ctx.token_index = math.max(1, tonumber(state.token_index) or 1)

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
-- Round 51 contract: [saveload] (audit: handler lacked a schema).
require("kag.schema").define("saveload", {
    _meta = { category = "save", blocking = true, desc = "open the save/load menu (mode: save|load)" },
    mode = { type = "string" },
})

function SaveCommands.saveload(ctx, params)
    -- Round 52 audit: headless/editor environments have no UI overlay —
    -- saveload_menu may not be loadable at all, so pcall the require.
    local okL, SaveLoad = pcall(require, "saveload_menu")
    if not okL then SaveLoad = nil end
    -- string guard on the bare mode (audit: a pair table from named
    -- params must not reach the menu as the mode)
    local mode = params.mode
    if type(mode) ~= "string" then
        mode = (type(params[1]) == "string") and params[1] or nil
    end
    -- Round 52 audit: headless/editor environments have no UI overlay —
    -- require("saveload_menu") may be absent or lack .show.
    if type(SaveLoad) ~= "table" or type(SaveLoad.show) ~= "function" then
        print("[SaveCmd] saveload: menu unavailable (headless) — skipping")
        return
    end
    local chosen = SaveLoad.show(ctx, mode or "save")
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
    -- Round 52 audit: headless/web environments may lack the list_saves
    -- binding (no C++ SaveManager) — degrade to an empty list instead of
    -- crashing on a nil call.
    local fn = kag_binding("list_saves")
    if type(fn) ~= "function" then
        print("[SaveCmd] listsaves: binding unavailable (headless) — empty list")
        ctx.sf = ctx.sf or {}
        ctx.sf.save_list = {}
        ctx.tf = ctx.tf or {}
        ctx.tf.save_list = {}
        return
    end
    local saves = fn()
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
