-- =============================================================================
--  Caesura (AmeKAG) — kag/expr.lua
--  KAG Neo-Genesis expression language.
--
--  Legacy KAG3 scripts use TJS-style operators in [if]/[while]/[eval]:
--      &&  ||  !   !=   cond ? a : b
--  Lua uses:      and or not ~= (a and b or c)
--  This module translates TJS expressions into Lua before compilation, so
--  old scenes run as-is instead of silently failing (previously a TJS
--  expression failed to compile, eval_expr swallowed the error and the
--  [if] silently took the else branch).
--
--  Translation is string-literal aware: operators inside '...' / "..." are
--  left untouched. Ternary is translated to `(cond and (a) or (b))`, which
--  is exact for numbers/strings and for booleans where the then-branch is
--  truthy. Nested ternaries are supported.
--
--  Error visibility: compile/runtime failures now print a scene:line
--  diagnostic instead of returning false silently.
-- =============================================================================

local expr = {}

-- Expression chunk cache (translated source -> compiled function).
-- Keyed by translated source + env identity (ctx.f), so cached chunks never
-- pin an old env table. Bounded like the scheduler's own cache.
local cache = {}
local CACHE_MAX = 128

-- AOT bytecode cache (Battle 1c): translated source -> string.dump chunk.
-- Loading a dumped chunk with `load(bc, name, "b", env)` skips Lua's
-- lexer+parser (measured ~6x faster than source load). The compiler
-- pre-generates dumps for compiled streams (_compiled.exprDumps); this
-- table back-fills lazily for hand-built/deserialized streams, so the
-- second env identity for the same source is already AOT.
local dump_cache = {}
local DUMP_CACHE_MAX = 128

-- ---------------------------------------------------------------------------
-- Character-level scanner helpers (string-literal aware)
-- ---------------------------------------------------------------------------

-- Find the first occurrence of ch at paren-depth 0, outside string literals.
-- Returns byte index or nil.
local function find_top(src, ch, from)
    local depth = 0
    local i = from or 1
    local n = #src
    local quote = nil
    while i <= n do
        local c = src:sub(i, i)
        if quote then
            if c == "\\" then
                i = i + 2
            elseif c == quote then
                quote = nil
                i = i + 1
            else
                i = i + 1
            end
        else
            if c == '"' or c == "'" then
                quote = c
                i = i + 1
            elseif c == "(" or c == "[" then
                depth = depth + 1
                i = i + 1
            elseif c == ")" or c == "]" then
                depth = depth - 1
                i = i + 1
            elseif c == ch and depth == 0 then
                return i
            else
                i = i + 1
            end
        end
    end
    return nil
end

-- Find the top-level ':' that pairs with a '?' at position q_pos, tolerating
-- nested ternaries (a ? b ? c : d : e -> the LAST top-level ':').
local function match_colon(src, q_pos)
    local depth = 0
    local nested = 0
    local i = q_pos + 1
    local n = #src
    local quote = nil
    while i <= n do
        local c = src:sub(i, i)
        if quote then
            if c == "\\" then
                i = i + 2
            elseif c == quote then
                quote = nil
                i = i + 1
            else
                i = i + 1
            end
        else
            if c == '"' or c == "'" then
                quote = c
                i = i + 1
            elseif c == "(" or c == "[" then
                depth = depth + 1
                i = i + 1
            elseif c == ")" or c == "]" then
                depth = depth - 1
                i = i + 1
            elseif c == "?" and depth == 0 then
                nested = nested + 1
                i = i + 1
            elseif c == ":" and depth == 0 then
                if nested == 0 then
                    return i
                end
                nested = nested - 1
                i = i + 1
            else
                i = i + 1
            end
        end
    end
    return nil
end

-- ---------------------------------------------------------------------------
-- Operator translation (TJS -> Lua), string-literal aware
-- ---------------------------------------------------------------------------

local function translate_operators(src)
    local out = {}
    local i = 1
    local n = #src
    local quote = nil

    -- Trim trailing whitespace of the last emitted chunk, then append the
    -- operator with single surrounding spaces, skipping whitespace that
    -- follows the operator in the source. "a && b" / "a&&b" both -> "a and b".
    local function emit_op(op)
        -- Trim trailing whitespace of the last emitted chunk (and a leading
        -- space when nothing was emitted yet, e.g. "!a"), then append the
        -- operator with single surrounding spaces, skipping whitespace that
        -- follows the operator in the source.
        if #out > 0 and out[#out]:match("%s$") then
            out[#out] = out[#out]:gsub("%s+$", "")
        elseif #out == 0 and op:match("^%s") then
            op = op:gsub("^%s+", "")
        end
        out[#out + 1] = op
        while i <= n and src:sub(i, i):match("%s") do
            i = i + 1
        end
    end

    while i <= n do
        local c = src:sub(i, i)
        if quote then
            out[#out + 1] = c
            if c == "\\" then
                out[#out + 1] = src:sub(i + 1, i + 1)
                i = i + 2
            else
                if c == quote then quote = nil end
                i = i + 1
            end
        else
            if c == '"' or c == "'" then
                quote = c
                out[#out + 1] = c
                i = i + 1
            elseif c == "&" and src:sub(i + 1, i + 1) == "&" then
                i = i + 2
                emit_op(" and ")
            elseif c == "|" and src:sub(i + 1, i + 1) == "|" then
                i = i + 2
                emit_op(" or ")
            elseif c == "!" and src:sub(i + 1, i + 1) == "=" then
                i = i + 2
                emit_op(" ~= ")
            elseif c == "!" then
                i = i + 1
                emit_op(" not ")
            elseif c == "?" and src:sub(i + 1, i + 1) == "?" then
                -- TJS null-coalescing: a ?? b keeps a when it is not
                -- nil/false, else b. Lua 'or' is exactly this value
                -- fallback (0 and "" are truthy, so numbers and empty
                -- strings survive; only nil/false fall through).
                -- (round 53: the doc mentioned ?? but it never translated)
                i = i + 2
                emit_op(" or ")
            else
                out[#out + 1] = c
                i = i + 1
            end
        end
    end
    return table.concat(out)
end

-- ---------------------------------------------------------------------------
-- Full translation: ternary first (recursive), then operators
-- ---------------------------------------------------------------------------

local translate

local function trim(s)
    return (s:gsub("^%s+", ""):gsub("%s+$", ""))
end

local function translate_ternary(src)
    local q = find_top(src, "?", 1)
    if not q then
        return translate_operators(src)
    end
    local colon = match_colon(src, q)
    if not colon then
        -- Unbalanced '?' -- leave as-is; the compile error below will be
        -- reported visibly by expr.evaluate.
        return translate_operators(src)
    end
    local cond = trim(src:sub(1, q - 1))
    local then_part = trim(src:sub(q + 1, colon - 1))
    local else_part = trim(src:sub(colon + 1))
    return "((" .. translate(cond) .. ") and (" .. translate(then_part)
        .. ") or (" .. translate(else_part) .. "))"
end

translate = translate_ternary

-- ---------------------------------------------------------------------------
-- Public API
-- ---------------------------------------------------------------------------

--- expr.translate(source) -> Lua source. Exported for tests/tooling.
function expr.translate(source)
    if type(source) ~= "string" then return tostring(source) end
    return translate(source)
end

--- expr.evaluate(ctx, source) -> ok, value
--  Compiles the translated expression in the ctx variable environment
--  (ctx.f / ctx.sf / ctx.tf / ctx.mp / ctx.lf). On compile or runtime error
--  prints a scene:line diagnostic and returns false, nil -- the [if] then
--  takes the else branch, but the author sees the error.
function expr.evaluate(ctx, source)
    if type(source) ~= "string" then return true, source end
    local translated = translate(source)
    return expr.evaluateTranslated(ctx, translated, source)
end

--- expr.evaluateTranslated(ctx, translated, original) → ok, value
--  Same as evaluate() but SKIPS the TJS->Lua translation: the compile-time
--  front-end (kag/compiler.lua) already translated the source once, so the
--  runtime hot path avoids re-scanning it on every [if]/[while] evaluation
--  (measured: 400-if scene ~30% faster on the eval path). `original` is
--  used only for error diagnostics (source shown to the author).
--  Optional `dump` (Battle 1c): precompiled string.dump bytecode from the
--  compiler (_compiled.exprDumps). Loading bytecode skips Lua's lexer+
--  parser (~6x faster than source load, measured). The dump is bound to
--  the caller's env at load time (mode "b" + env), so the same dump
--  serves every env identity — a session-wide AOT win, not per-env.
function expr.evaluateTranslated(ctx, translated, original, dump)
    if type(translated) ~= "string" then return true, translated end
    local f = ctx and ctx.f or {}
    -- Dual-style environment:
    --   * bare identifiers (score > 5) resolve in ctx.f via __index
    --   * TJS/KAG3 style (f.hp, tf.flag, sf.x, mp.z, lf.y) resolve via the
    --     named tables -- lf is the call-stack variable frame (empty when
    --     no [call] frame is active, see scheduler.lua).
    local env = {
        f  = f,
        sf = ctx and ctx.sf or {},
        tf = ctx and ctx.tf or {},
        mp = ctx and ctx.mp or {},
        lf = ctx and ctx.lf or {},
    }
    setmetatable(env, { __index = f })
    local key = translated .. "\0" .. tostring(f)
    local fn = cache[key]
    if not fn then
        -- AOT path: use the precompiled bytecode when available (either
        -- supplied by the compiler or back-filled in dump_cache). Fall
        -- back to source load on any dump failure (e.g. a dump produced
        -- by a different Lua build) — correctness beats speed.
        local bc = dump or dump_cache[translated]
        local okChunk, chunk
        if bc then
            okChunk, chunk = pcall(load, bc, "=kag_expr", "b", env)
            if okChunk and chunk and not dump_cache[translated] then
                -- Share the compiler-supplied dump session-wide: a
                -- deserialized (.ksc) stream evaluating the same
                -- expression gets the AOT path too.
                dump_cache[translated] = bc
                local n = 0
                for _ in pairs(dump_cache) do n = n + 1 end
                if n > DUMP_CACHE_MAX then
                    local keys = {}
                    for k in pairs(dump_cache) do keys[#keys + 1] = k end
                    for j = 1, math.floor(#keys / 2) do
                        dump_cache[keys[j]] = nil
                    end
                end
            end
        end
        if not bc or not okChunk or not chunk then
            okChunk, chunk = pcall(load, "return " .. translated,
                                   "=kag_expr", "t", env)
            if okChunk and chunk and not dump_cache[translated] then
                -- Back-fill: a hand-built/deserialized stream has no
                -- compiler-supplied dump; make the NEXT env identity AOT.
                local okDump, dumped = pcall(string.dump, chunk, true)
                if okDump and dumped then
                    dump_cache[translated] = dumped
                    local n = 0
                    for _ in pairs(dump_cache) do n = n + 1 end
                    if n > DUMP_CACHE_MAX then
                        local keys = {}
                        for k in pairs(dump_cache) do keys[#keys + 1] = k end
                        for j = 1, math.floor(#keys / 2) do
                            dump_cache[keys[j]] = nil
                        end
                    end
                end
            end
        end
        if okChunk and chunk then
            fn = chunk
            cache[key] = fn
            local n = 0
            for _ in pairs(cache) do n = n + 1 end
            if n > CACHE_MAX then
                local keys = {}
                for k in pairs(cache) do keys[#keys + 1] = k end
                for j = 1, math.floor(#keys / 2) do
                    cache[keys[j]] = nil
                end
            end
        else
            local where = "?"
            if ctx then
                where = (ctx.current_scene or ctx.currentScene or "?")
                    .. ":" .. tostring(ctx.token_index or ctx.tokenIndex or "?")
            end
            print(string.format(
                "[KAG] expression error in %s: %s (source: %s)",
                where, tostring(chunk), tostring(original or translated)))
            return false, nil
        end
    end
    local ok, value = pcall(fn)
    if not ok then
        local where = "?"
        if ctx then
            where = (ctx.current_scene or ctx.currentScene or "?")
                .. ":" .. tostring(ctx.token_index or ctx.tokenIndex or "?")
        end
        print(string.format(
            "[KAG] expression runtime error in %s: %s (source: %s)",
            where, tostring(value), tostring(original or translated)))
        return false, nil
    end
    return true, value
end

--- expr.reset_cache() — test/tooling hook.
function expr.reset_cache()
    cache = {}
    dump_cache = {}
end

return expr
