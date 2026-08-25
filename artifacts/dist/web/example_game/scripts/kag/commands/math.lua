-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/math.lua
--  KAG3-compatible arithmetic commands: [add]/[sub]/[mul]/[div]/[mod]/[dec]
--  Variable arithmetic: target += value (and -=, *=, /=, %=), plus the
--  [inc] twin [dec] (target -= amount, amount default 1).
--  Mirrors scripts/kag/commands/system.lua in three respects:
--    * variable-path resolution (f./sf./tf./mp./lf./ bare -> ctx.f);
--    * value handling (tonumber, NOT expr.evaluate — matches [inc]);
--    * nil-safe read: a missing variable starts from 0.
--  Contract design notes:
--    * [mod]/[div] by a zero operand print a visible "[KAG] division by
--      zero" error and no-op (Lua 5.4 raises on "n % 0", and silent
--      inf/nan from "n / 0" would corrupt state — both are blocked so the
--      script author sees the mistake).
--    * command param is "name" (task spec), not the [inc] "var";
--      positional [add f.x 5] still works via positional_index.
-- =============================================================================

-- _meta-carrying schema requires (blocking=false: no coroutine yield).
local _schema = require("kag.schema")

_schema.define("add", {
    _meta = { category = "system", blocking = false,
              desc = "KAG3-compatible add: var += value" },
    name  = { type = "string", required = true, positional_index = 1 },
    value = { type = "number", required = true, positional_index = 2 },
})
_schema.define("sub", {
    _meta = { category = "system", blocking = false,
              desc = "KAG3-compatible sub: var -= value" },
    name  = { type = "string", required = true, positional_index = 1 },
    value = { type = "number", required = true, positional_index = 2 },
})
_schema.define("mul", {
    _meta = { category = "system", blocking = false,
              desc = "KAG3-compatible mul: var *= value" },
    name  = { type = "string", required = true, positional_index = 1 },
    value = { type = "number", required = true, positional_index = 2 },
})
_schema.define("div", {
    _meta = { category = "system", blocking = false,
              desc = "KAG3-compatible div: var /= value" },
    name  = { type = "string", required = true, positional_index = 1 },
    value = { type = "number", required = true, positional_index = 2 },
})
_schema.define("mod", {
    _meta = { category = "system", blocking = false,
              desc = "KAG3-compatible mod: var %= value (zero operand -> visible error, no-op)" },
    name  = { type = "string", required = true, positional_index = 1 },
    value = { type = "number", required = true, positional_index = 2 },
})
_schema.define("dec", {
    _meta = { category = "system", blocking = false,
              desc = "KAG3-compatible dec (inc twin): var -= amount (default 1)" },
    name   = { type = "string", required = true, positional_index = 1 },
    amount = { type = "number", default = 1, positional_index = 2 },
})

local MathCommands = {}

-- Resolve "f.x"/"sf.x"/"tf.x"/"mp.x"/"lf.x"/bare "x" -> (table, key).
-- Mirrors system.lua resolve_var verbatim (scope accepted, non-table ctx
-- scope -> nil).
local function resolve_var(ctx, var)
    if type(var) ~= "string" or var == "" then return nil end
    local scope, key
    local tname, k = var:match("^([%a_]+)%.([%w_]+)$")
    if tname then
        scope = ({ f = "f", sf = "sf", tf = "tf", mp = "mp", lf = "lf" })[tname]
        key = k
    else
        scope, key = "f", var
    end
    local t = scope and ctx[scope]
    if type(t) ~= "table" then return nil end
    return t, key
end

-- Shared binary-op driver (add/sub/mul/div/mod). Value semantics follow
-- [inc]: tonumber(), nil-safe (missing variable starts at 0). Division-
-- by-zero ("value == 0" for div, and the same guard for mod) prints a
-- visible "[KAG] division by zero" error and no-ops.
local function binop(ctx, params, op)
    local var = params.name
    if type(var) ~= "string" and type(params[1]) == "string" then
        var = params[1]
    end
    local val = tonumber(params.value)
    if val == nil then val = tonumber(params[2]) end
    if val == nil then
        print(string.format("[WARN] [%s] missing/invalid numeric value", op))
        return
    end
    local t, key = resolve_var(ctx, var)
    if not t then
        print(string.format("[WARN] [%s] unknown variable scope: %s", op, tostring(var)))
        return
    end
    if op == "div" or op == "mod" then
        if val == 0 then
            print(string.format(
                "[KAG] division by zero in [%s name=%s value=%s]",
                op, tostring(var), tostring(val)))
            return
        end
    end
    local cur = tonumber(t[key]) or 0
    if op == "add" then
        t[key] = cur + val
    elseif op == "sub" then
        t[key] = cur - val
    elseif op == "mul" then
        t[key] = cur * val
    elseif op == "div" then
        t[key] = cur / val
    elseif op == "mod" then
        t[key] = cur % val
    end
end

function MathCommands.add(ctx, params) binop(ctx, params, "add") end
function MathCommands.sub(ctx, params) binop(ctx, params, "sub") end
function MathCommands.mul(ctx, params) binop(ctx, params, "mul") end
function MathCommands.div(ctx, params) binop(ctx, params, "div") end
function MathCommands.mod(ctx, params) binop(ctx, params, "mod") end

--- [dec name="f.count"] / [dec name="f.count" amount=3] — decrement.
--  The twin of [inc]: target -= amount, amount default 1, nil-safe.
function MathCommands.dec(ctx, params)
    local var = params.name
    if type(var) ~= "string" and type(params[1]) == "string" then
        var = params[1]
    end
    local amount = tonumber(params.amount) or tonumber(params[2]) or 1
    local t, key = resolve_var(ctx, var)
    if not t then
        print("[WARN] [dec] unknown variable scope: " .. tostring(var))
        return
    end
    t[key] = (tonumber(t[key]) or 0) - amount
end

return MathCommands
