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

local function coerceValue(name, spec, raw, where, ctx)
    local v = raw
    if spec.type == "number" then
        if type(v) == "number" then
            -- already numeric (embedded eval may pass numbers)
        elseif type(v) == "string" and v:match("^%s*%-?%d+%.?%d*%s*$") then
            v = tonumber(v)
        else
            error(string.format(
                "%s: param '%s' expects a number, got %q", where, name, tostring(raw)), 0)
        end
        if spec.min and v < spec.min then
            print(string.format("[schema] %s: '%s' clamped %s -> %s (min)",
                where, name, tostring(v), tostring(spec.min)))
            v = spec.min
        elseif spec.max and v > spec.max then
            print(string.format("[schema] %s: '%s' clamped %s -> %s (max)",
                where, name, tostring(v), tostring(spec.max)))
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
                    "%s: param '%s' expects boolean, got %q", where, name, tostring(raw)), 0)
            end
        else
            error(string.format(
                "%s: param '%s' expects boolean, got %q", where, name, tostring(raw)), 0)
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
            v = v:gsub("%${([^{}]+)}", function(expr)
                -- env exposes the ctx variable tables directly: the
                -- expression f.hp*2 works (env.f = ctx.f, etc).
                local env = { f = ctx and ctx.f, sf = ctx and ctx.sf,
                              tf = ctx and ctx.tf, mp = ctx and ctx.mp,
                              lf = ctx and ctx.lf }
                local f2 = load("return (" .. expr .. ")", "=ks_interp", "t", env)
                if not f2 then return "${" .. expr .. "}" end  -- syntax error
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
    end
    if spec.choices and not spec.choices[v] then
        error(string.format("%s: param '%s' must be one of {%s}, got %q",
            where, name, table.concat(spec.choices, ","), tostring(raw)), 0)
    end
    return v
end

--- Schema.coerce(cmd, params, ctx) → coerced params table (or raw on unmigrated)
--  Throws (caller pcall) with a structured message on contract violation.
function Schema.coerce(cmd, params, ctx)
    local specs = registry[cmd]
    if not specs then return params end  -- unmigrated: pass-through

    local where = string.format("cmd [%s]@%s:%s",
        cmd, ctx and (ctx.current_scene or ctx.currentScene) or "?",
        ctx and ctx.token_index or "?")
    local out = {}

    -- Any-of requirement: at least one of these params must be present.
    if specs._require_any then
        local found = false
        for _, n in ipairs(specs._require_any) do
            local raw = params[n]
            if raw ~= nil and raw ~= "" then found = true break end
        end
        if not found then
            error(where .. ": requires one of {" .. table.concat(specs._require_any, ",") .. "}", 0)
        end
    end

    -- Coerce declared params.
    for name, spec in pairs(specs) do
        local raw = params[name]
        if raw == nil or raw == "" then
            if spec.required then
                error(where .. ": missing required param '" .. name .. "'", 0)
            end
            if spec.default ~= nil then out[name] = spec.default end
        else
            out[name] = coerceValue(name, spec, raw, where, ctx)
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
