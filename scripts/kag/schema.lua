-- ═══════════════════════════════════════════════════════════════════════
--  kag/schema.lua — Declarative command contracts (next-gen KAG rules)
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

--- Schema.define(cmd, specs) — register the contract for one command.
function Schema.define(cmd, specs)
    if type(cmd) ~= "string" or type(specs) ~= "table" then
        error("schema.define: cmd(string) and specs(table) required", 2)
    end
    registry[cmd] = specs
    migrated[cmd] = true
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
        -- Next-gen interpolation: "$f.name" / "$sf.x" / "$tf.y" / "$mp.z"
        -- expand from the ctx variable tables (KAG3 needed [eval] glue).
        if spec.interpolate and type(v) == "string" and v:find("$", 1, true) then
            v = v:gsub("%$(%a+)%.([%w_]+)", function(tbl, key)
                local vars = ({ f = "f", sf = "sf", tf = "tf", mp = "mp" })[tbl]
                local t = vars and ctx and ctx[vars]
                if type(t) == "table" then
                    local val = t[key]
                    if val ~= nil then return tostring(val) end
                end
                return "$" .. tbl .. "." .. key  -- leave unresolved as-is
            end)
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
            print(string.format("[schema] %s: unknown param '%s' ignored",
                cmd, tostring(name)))
            out[name] = v  -- pass through for compat
        end
    end
    return out
end

--- Schema.isMigrated(cmd) → boolean
function Schema.isMigrated(cmd)
    return migrated[cmd] == true
end

--- Schema.dumpContracts() → { cmd = { param = spec } } — public snapshot
--  of the registry for doc generation / editor tooling. The contracts
--  are the single source of truth; docs and editors consume this.
function Schema.dumpContracts()
    local out = {}
    for cmd, specs in pairs(registry) do
        out[cmd] = specs
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
