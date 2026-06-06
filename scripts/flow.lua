-- =============================================================================
--  Caesura (AmeKAG) — flow.lua
--  Script flow utilities: scene loading, label map building, skip helpers.
--  Used by scheduler.lua for [jump]/[call]/[link] cross-scene navigation.
-- =============================================================================

local flow = {}

-- ── Scene cache ─────────────────────────────────────────────────────────────

flow.scene_cache = {}

-- ── flow.load_scene(path) → {tokens, labels} ────────────────────────────────

function flow.load_scene(path)
    -- Check cache
    if flow.scene_cache[path] then
        return flow.scene_cache[path]
    end

    local tokenizer = require("tokenizer")
    local tokens = tokenizer.parse_file(path)

    -- Build label map: label_name → token_index
    local labels = {}
    for i, tok in ipairs(tokens) do
        if tok[1] == "label" and tok[2] and tok[2].name then
            labels[tok[2].name] = i
        end
    end

    local scene = {tokens = tokens, labels = labels, path = path}
    flow.scene_cache[path] = scene
    return scene
end

-- ── flow.reload_scene(path) — force reload (for hot reload) ──────────────────

function flow.reload_scene(path)
    flow.scene_cache[path] = nil
    return flow.load_scene(path)
end

-- ── flow.clear_cache() — hot reload support ──────────────────────────────────

function flow.clear_cache()
    flow.scene_cache = {}
end

-- ── flow.skip_to(tokens, start, targets) → index ────────────────────────────

function flow.skip_to(tokens, start_idx, target_cmds)
    local depth = 1
    local opens = target_cmds.opens or {}
    local targets = {}
    for _, t in ipairs(target_cmds) do targets[t] = true end

    for i = start_idx + 1, #tokens do
        local cmd = tokens[i][1]
        if targets[cmd] and depth == 1 then
            return i
        elseif opens[cmd] then
            depth = depth + 1
        elseif targets[cmd] then
            depth = depth - 1
        end
    end
    return #tokens
end

-- ── flow.find_label(tokens, name) → index ────────────────────────────────────

function flow.find_label(tokens, name)
    for i, tok in ipairs(tokens) do
        if tok[1] == "label" and tok[2] and tok[2].name == name then
            return i
        end
    end
    return nil
end

return flow
