-- =============================================================================
--  Caesura (AmeKAG) — flow.lua
--  Script flow utilities: scene loading, label map building, skip helpers.
--  Used by scheduler.lua for [jump]/[call]/[link] cross-scene navigation.
-- =============================================================================

local flow = {}

-- ── Scene cache ─────────────────────────────────────────────────────────────

flow.scene_cache = {}

-- ── flow.load_scene(path) → {tokens, labels} ────────────────────────────────

-- .ksc bytecode cache (Battle 1b: compile once, reuse many). The cache
-- file sits next to the scene (scene.ks -> scene.ksc). Freshness is
-- checked by source size + cache non-emptiness (cheap proxy; a content
-- hash would be stronger but the tokenizer/compiler are fast enough that
-- a stale cache only costs one recompile). Cache failures never break
-- scene loading — they degrade to parse+compile.

function flow.load_scene(path, prepare_only)
    -- Check cache (cache keyed by the RESOLVED path so a mod override
    -- that appears after a base scene was cached still wins).
    -- Mod resolution: enabled mods may override base scenes
    -- (mods/<name>/<path>); the resolved path is cached independently.
    local mods = require("mods")
    local resolved = mods.resolve_scene(path)
    if not prepare_only and flow.scene_cache[resolved] then
        return flow.scene_cache[resolved]
    end

    local tokenizer = require("tokenizer")
    local compiler = require("kag.compiler")

    -- Cache path: compiled bytecode lives in cache/ksc/ (never next to
    -- the source, so scene asset directories stay clean). The cache key
    -- is the resolved path with separators and extension sanitized.
    local kscPath = "cache/ksc/" .. resolved:gsub("[/\\]+", "_"):gsub("%.ks$", ".ksc")
    local tokens = nil

    -- 1) Try the .ksc cache first (compile once, reuse many).
    local cached = not prepare_only and compiler.readCache(kscPath)
    if cached and #cached > 0 and cached._compiled then
        -- freshness: content hash must match (size-only comparison is
        -- unreliable when a scene is edited without changing length)
        local srcHash = compiler.hashFile(resolved)
        if srcHash and cached._compiled._srcHash == srcHash then
            tokens = cached
        end
    end

    -- 2) Cache miss: parse + compile, then persist.
    if not tokens then
        local ok, tokens_or_err = pcall(tokenizer.parse_file, resolved)
        if not ok then
            print("[Flow] Failed to load scene: " .. resolved .. " - " .. tostring(tokens_or_err))
            return nil, tokens_or_err
        end
        tokens = tokens_or_err
        local compiled, reason=pcall(compiler.compile, tokens)
        if prepare_only and not compiled then return nil,reason end
        if not prepare_only then
            tokens._srcHash = compiler.hashFile(resolved)
            pcall(compiler.writeCache, tokens, kscPath)
        end
    end

    -- Label map from the compiled index (fixes the legacy dead-code path
    -- that scanned record-format tokens with array-format checks).
    local labels = {}
    if tokens._compiled and tokens._compiled.labels then
        for k, v in pairs(tokens._compiled.labels) do labels[k] = v end
    end

    local scene = {tokens = tokens, labels = labels, path = resolved,
                   base_path = path}
    if not prepare_only then flow.scene_cache[resolved] = scene end
    return scene
end

-- Restore preparation must not reuse a token stream changed by live macros or
-- replace the cache entry used by the still-running session.
function flow.prepare_scene(path)
    return flow.load_scene(path, true)
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
