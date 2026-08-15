-- ═══════════════════════════════════════════════════════════════════════
--  kag/schema.lua — Declarative command contracts (KAG Neo-Genesis rules)
--
--  KAG3 commands read params as raw strings and each handler re-parses
--  (tonumber(params.x) or default) with silent fallbacks on bad input.
--  This module replaces that with a declarative contract:
--
--    Schema.define("pt", {
--        speed = { type = "number", default = 50, min = 8, max = 5000 },
--    })
--
--  The scheduler coerces params BEFORE dispatch: types are converted,
--  ranges clamped, unknown params warned, bad values reported with the
--  command/scene/token location instead of being swallowed. Handlers can
--  then read params.speed as a plain number.
--
--  Incremental: commands migrate one at a time; unmigrated commands pass
--  through unchanged (no behavior change).
-- ═══════════════════════════════════════════════════════════════════════

local Schema = {}

-- cmd -> { paramName = spec } ; spec: {type, default, min, max, choices, required}
local registry = {}
local migrated = {}  -- set of migrated command names
-- Command metadata (category / blocking / description) -- the schema
-- registry stores typed param contracts; _meta carries tooling facts:
--   category : text|audio|layer|transition|vfx|resource|save|system|video
--   blocking : true when the command waits for completion (duration/input)
--   desc     : one-line human summary (editor tooltips / docs)
local registry_meta = {}

-- ${expr} interpolation chunk cache: the expression text is a pure
-- function of the scene source (compiled once), so load() per evaluation
-- is waste -- the chunk is cached by expression string (bounded like the
-- expr module's cache). The env is a SHARED table: Lua 5.4 binds _ENV at
-- load time, so chunks load once against it and the f/sf/tf/mp/lf fields
-- are updated to the current ctx tables before each evaluation (the
-- scheduler is single-threaded -- no concurrent evaluation can observe a
-- torn env).
local interp_cache = {}
local INTERP_CACHE_MAX = 128
local interp_env = { f = {}, sf = {}, tf = {}, mp = {}, lf = {} }

--- Schema.define(cmd, specs) — register the contract for one command.
--  `specs._meta = { category=..., blocking=..., desc=... }` is optional and
--  stored separately (never treated as a parameter contract).
function Schema.define(cmd, specs)
    if type(cmd) ~= "string" or type(specs) ~= "table" then
        error("schema.define: cmd(string) and specs(table) required", 2)
    end
    if specs._meta ~= nil then
        registry_meta[cmd] = specs._meta
        local clean = {}
        for k, v in pairs(specs) do
            if k ~= "_meta" then clean[k] = v end
        end
        specs = clean
    end
    registry[cmd] = specs
    migrated[cmd] = true
end

--- Schema.meta(cmd) → metadata table or nil
function Schema.meta(cmd)
    return registry_meta[cmd]
end

local function coerceValue(name, spec, raw, whereFn, ctx)
    local v = raw
    if spec.type == "number" then
        if type(v) == "number" then
            -- already numeric (embedded eval may pass numbers)
        elseif type(v) == "string" and v:match("^%s*%-?%d+%.?%d*%s*$") then
            v = tonumber(v)
        else
            error(string.format(
                "%s: param '%s' expects a number, got %q", whereFn(), name, tostring(raw)), 0)
        end
        if spec.min and v < spec.min then
            print(string.format("[schema] %s: '%s' clamped %s -> %s (min)",
                whereFn(), name, tostring(v), tostring(spec.min)))
            v = spec.min
        elseif spec.max and v > spec.max then
            print(string.format("[schema] %s: '%s' clamped %s -> %s (max)",
                whereFn(), name, tostring(v), tostring(spec.max)))
            v = spec.max
        end
    elseif spec.type == "boolean" then
        if type(v) == "boolean" then
            -- pass
        elseif type(v) == "string" then
            local low = v:lower()
            if low == "true" or low == "1" or low == "yes" then v = true
            elseif low == "false" or low == "0" or low == "no" then v = false
            else
                error(string.format(
                    "%s: param '%s' expects boolean, got %q", whereFn(), name, tostring(raw)), 0)
            end
        else
            error(string.format(
                "%s: param '%s' expects boolean, got %q", whereFn(), name, tostring(raw)), 0)
        end
    elseif spec.type == "string" then
        v = tostring(v)
        -- Neo-Genesis interpolation: "$f.name" / "$sf.x" / "$tf.y" / "$mp.z"
        -- / "$lf.y" expand from the ctx variable tables; legacy KAG3's
        -- %var% syntax ("%f.hp%") is supported too. ${expr} evaluates a
        -- full expression (beyond KAG3's eval-glue).
        if spec.interpolate and type(v) == "string"
            and (v:find("$", 1, true) or v:find("%", 1, true)) then
            -- ${expr}: full expression evaluated in a sandbox env with the
            -- ctx variable tables (f/sf/tf/mp/lf) -- beyond KAG3's eval-glue.
            -- Chunk cache: Lua 5.4 binds _ENV at load time, so the chunk is
            -- loaded ONCE against a shared env table whose f/sf/tf/mp/lf
            -- fields are updated to the current ctx tables before every
            -- evaluation (the scheduler is single-threaded/serial, so no
            -- concurrent evaluation can observe a torn env).
            v = v:gsub("%${([^{}]+)}", function(expr)
                local f2 = interp_cache[expr]
                if not f2 then
                    interp_env.f = ctx and ctx.f or {}
                    interp_env.sf = ctx and ctx.sf or {}
                    interp_env.tf = ctx and ctx.tf or {}
                    interp_env.mp = ctx and ctx.mp or {}
                    interp_env.lf = ctx and ctx.lf or {}
                    -- TJS -> Lua translation (&& || ! != ?:) so
                    -- ${expr} accepts the same expression language as
                    -- [if]/[eval] (round 50 audit: a ternary was left
                    -- verbatim — Lua load() rejects TJS operators).
                    local exprLua = expr
                    pcall(function()
                        local ex = require("kag.expr")
                        if ex and ex.translate then exprLua = ex.translate(expr) end
                    end)
                    f2 = load("return (" .. exprLua .. ")", "=ks_interp", "t",
                              interp_env)
                    if not f2 then return "${" .. expr .. "}" end  -- syntax error
                    interp_cache[expr] = f2
                    local n = 0
                    for _ in pairs(interp_cache) do n = n + 1 end
                    if n > INTERP_CACHE_MAX then
                        local keys = {}
                        for k in pairs(interp_cache) do keys[#keys + 1] = k end
                        for j = 1, math.floor(#keys / 2) do
                            interp_cache[keys[j]] = nil
                        end
                    end
                else
                    -- update the shared env to the current ctx tables
                    interp_env.f = ctx and ctx.f or {}
                    interp_env.sf = ctx and ctx.sf or {}
                    interp_env.tf = ctx and ctx.tf or {}
                    interp_env.mp = ctx and ctx.mp or {}
                    interp_env.lf = ctx and ctx.lf or {}
                end
                local ok2, val2 = pcall(f2)
                if ok2 then return tostring(val2) end
                return "${" .. expr .. "}"
            end)
            -- $tbl.key / %tbl.key% variable lookup (f/sf/tf/mp/lf). The
            -- %...% form is KAG3-compatible; bare %ident% stays untouched
            -- (macro placeholders are expanded earlier by the scheduler).
            local varLookup = function(tbl, key)
                local vars = ({ f = "f", sf = "sf", tf = "tf", mp = "mp", lf = "lf" })[tbl]
                local t = vars and ctx and ctx[vars]
                if type(t) == "table" then
                    local val = t[key]
                    if val ~= nil then return tostring(val) end
                end
                return "$" .. tbl .. "." .. key  -- leave unresolved as-is
            end
            v = v:gsub("%$(%a+)%.([%w_]+)", varLookup)
            v = v:gsub("%%(%a+)%.([%w_]+)%%", varLookup)
        end
    elseif spec.type == "list" then
        -- Comma-separated value -> array, optionally typed per element.
        -- e.g. colors="red,green,blue" -> {"red","green","blue"}
        if type(v) == "table" then
            -- already a list (programmatic callers may pass arrays)
        elseif type(v) == "string" then
            local out = {}
            for raw_item in v:gmatch("[^,]+") do
                local item = raw_item:match("^%s*(.-)%s*$")  -- trim
                if #item > 0 then
                    if spec.item_type == "number" then
                        local n = tonumber(item)
                        if not n then
                            error(string.format(
                                "%s: param '%s' list element expects a number, got %q",
                                whereFn(), name, item), 0)
                        end
                        item = n
                    elseif spec.item_type == "boolean" then
                        local low = item:lower()
                        if low == "true" or low == "1" or low == "yes" then item = true
                        elseif low == "false" or low == "0" or low == "no" then item = false
                        else
                            error(string.format(
                                "%s: param '%s' list element expects boolean, got %q",
                                whereFn(), name, item), 0)
                        end
                    end
                    out[#out + 1] = item
                end
            end
            v = out
        else
            error(string.format(
                "%s: param '%s' expects a list, got %q", whereFn(), name, tostring(raw)), 0)
        end
    elseif spec.type == "enum" then
        -- Explicit enum type: value must be one of spec.values (or the
        -- legacy `choices` map). Kept as a string after validation.
        local allowed = spec.values or spec.choices
        if not allowed then
            error(string.format(
                "%s: param '%s' enum missing values", whereFn(), name), 0)
        end
        local ok = false
        if type(allowed) == "table" then
            if allowed[v] then
                ok = true
            else
                for _, av in ipairs(allowed) do
                    if tostring(av) == tostring(v) then ok = true break end
                end
            end
        end
        if not ok then
            local list = {}
            if type(allowed) == "table" then
                for k in pairs(allowed) do list[#list + 1] = tostring(k) end
            end
            table.sort(list)
            error(string.format(
                "%s: param '%s' must be one of {%s}, got %q",
                whereFn(), name, table.concat(list, ","), tostring(raw)), 0)
        end
        v = tostring(v)
    elseif spec.type == "file" then
        -- Asset path cross-validation: normalize to string; reject empty
        -- and path traversal (static, no ctx needed); when a ctx with a
        -- resolver is present, additionally verify the file exists.
        v = tostring(v)
        if v == "" then
            error(string.format(
                "%s: param '%s' file path must not be empty", whereFn(), name), 0)
        end
        if v:find("..", 1, true) or v:find("\\", 1, true) or v:sub(1, 1) == "/" then
            error(string.format(
                "%s: param '%s' invalid file path: %q (no traversal/absolute)",
                whereFn(), name, tostring(raw)), 0)
        end
        if ctx and ctx.resolve_file and type(v) == "string" and #v > 0 then
            local okF, resolved = pcall(ctx.resolve_file, v)
            if okF and resolved == nil then
                error(string.format(
                    "%s: param '%s' file not found: %q (asset root)",
                    whereFn(), name, tostring(raw)), 0)
            end
        end
    end
    if spec.type ~= "list" and spec.choices and not spec.choices[v] then
        error(string.format("%s: param '%s' must be one of {%s}, got %q",
            whereFn(), name, table.concat(spec.choices, ","), tostring(raw)), 0)
    end
    return v
end

--- Schema.coerce(cmd, params, ctx) → coerced params table (or raw on unmigrated)
--  Throws (caller pcall) with a structured message on contract violation.
function Schema.coerce(cmd, params, ctx)
    local specs = registry[cmd]
    if not specs then return params end  -- unmigrated: pass-through

    -- Lazily-built location string: the common path (params valid) never
    -- formats it; only error paths pay for the string.format.
    local where
    local function W()
        if not where then
            where = string.format("cmd [%s]@%s:%s",
                cmd, ctx and (ctx.current_scene or ctx.currentScene) or "?",
                ctx and ctx.token_index or "?")
        end
        return where
    end
    local out = {}

    -- Any-of requirement: at least one of these params must be present.
    if specs._require_any then
        local found = false
        for _, n in ipairs(specs._require_any) do
            local raw = params[n]
            if raw ~= nil and raw ~= "" then found = true break end
        end
        if not found then
            error(W() .. ": requires one of {" .. table.concat(specs._require_any, ",") .. "}", 0)
        end
    end

    -- Coerce declared params.
    for name, spec in pairs(specs) do
        local raw = params[name]
        if raw == nil or raw == "" then
            -- `positional_index = N`: the param may also arrive as the Nth
            -- bare positional arg (KAG3 style, e.g. [set f.hp 30]); the
            -- required check is skipped while that positional slot is filled.
            local pos_filled = spec.positional_index
                and params[spec.positional_index] ~= nil
                and params[spec.positional_index] ~= ""
            if spec.required and not pos_filled then
                error(W() .. ": missing required param '" .. name .. "'", 0)
            end
            -- When the positional slot is filled (pos_filled), do NOT
            -- write spec.default -- the handler falls back to params[N]
            -- itself, and a default would shadow the positional value.
            if spec.default ~= nil and not pos_filled then out[name] = spec.default end
        else
            out[name] = coerceValue(name, spec, raw, W, ctx)
        end
    end
    -- Copy undeclared params through (compat), but warn on unknown names.
    for name, v in pairs(params) do
        if specs[name] == nil then
            -- numeric keys (bare positional args) pass through silently;
            -- named unknowns still warn
            if type(name) ~= "number" then
                print(string.format("[schema] %s: unknown param '%s' ignored",
                    cmd, tostring(name)))
            end
            out[name] = v  -- pass through for compat
        end
    end
    return out
end

--- Schema.isMigrated(cmd) → boolean
function Schema.isMigrated(cmd)
    return migrated[cmd] == true
end

--- Schema.specs(cmd) → contract specs table or nil (LIVE reference, no
--  deep copy -- tooling that only reads positional_index etc. uses this;
--  dumpContracts() remains the deep-copy API for doc generation).
function Schema.specs(cmd)
    return registry[cmd]
end

--- Schema.dumpContracts() → { cmd = { param = spec } } — public DEEP copy
--  of the registry for doc generation / editor tooling. The contracts
--  are the single source of truth; docs and editors consume this.
--  Deep-copied so a caller cannot mutate live clamping/coercion.
function Schema.dumpContracts()
    local out = {}
    for cmd, specs in pairs(registry) do
        local copy = {}
        for name, spec in pairs(specs) do
            local sc = {}
            for k, v in pairs(spec) do
                if type(v) == "table" then
                    local t = {}
                    for kk, vv in pairs(v) do t[kk] = vv end
                    sc[k] = t
                else
                    sc[k] = v
                end
            end
            copy[name] = sc
        end
        if registry_meta[cmd] then
            copy._meta = {}
            for k, v in pairs(registry_meta[cmd]) do copy._meta[k] = v end
        end
        out[cmd] = copy
    end
    return out
end

--- Schema.registrySize() → number (for tests)
function Schema.registrySize()
    local n = 0
    for _ in pairs(registry) do n = n + 1 end
    return n
end

return Schema
