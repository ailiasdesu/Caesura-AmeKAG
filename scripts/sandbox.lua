-- =============================================================================
--  Caesura (AmeKAG) — scripts/sandbox.lua
--  Phase 9: Lua sandbox for [eval] and [emb] KAG commands.
--  Spec [10.2.47]: _ENV whitelist with Dev/Release dual mode.
--  Provides sandbox.create() and sandbox.execute() with CancelToken support.
-- =============================================================================

local Sandbox = {}

-- ═══════════════════════════════════════════════════════════════════════
--  Whitelist / Blacklist — spec [10.2.47]
-- ═══════════════════════════════════════════════════════════════════════

-- Allowed globals (whitelist)
local WHITELIST = {
    ["math"]     = math,
    ["string"]   = string,
    ["table"]    = table,
    ["pairs"]    = pairs,
    ["ipairs"]   = ipairs,
    ["next"]     = next,
    ["type"]     = type,
    ["tostring"] = tostring,
    ["tonumber"] = tonumber,
    ["pcall"]    = pcall,
    ["error"]    = error,
    ["assert"]   = assert,
    ["select"]   = select,
    ["xpcall"]   = xpcall,
    ["rawget"]   = rawget,
    ["rawset"]   = rawset,
    ["rawequal"] = rawequal,
    ["rawlen"]   = rawlen,
    ["unpack"]   = unpack or table.unpack,
}

-- Blacklisted globals
local BLACKLIST = {
    ["os"]      = true,
    ["io"]      = true,
    ["package"] = true,
    ["debug"]   = true,
    ["require"] = true,
    ["dofile"]  = true,
    ["loadfile"] = true,
    ["_G"]      = true,
    ["load"]    = true,
    ["collectgarbage"] = true,
    ["coroutine"]      = true,
}

-- Forbidden KAG commands (cannot be smuggled into sandbox eval/emb)
local FORBIDDEN_KAG = {
    jump     = true,
    link     = true,
    call     = true,
    return_  = true,
    end_     = true,
    trans    = true,
    move     = true,
    quake    = true,
    fade     = true,
    playvoice = true,
    wait     = true,
}

-- ═══════════════════════════════════════════════════════════════════════
--  sandbox.create(env_table) → sandboxed _ENV proxy
--  Returns a table with __index → env_table + whitelist
--  and __newindex → env_table only (blocks writing to whitelist)
-- ═══════════════════════════════════════════════════════════════════════

function Sandbox.create(env_table)
    env_table = env_table or {}

    local proxy = {}
    local mt = {
        __index = function(t, k)
            -- 1. Check env_table first (user-defined context)
            if env_table[k] ~= nil then
                return env_table[k]
            end
            -- 2. Whitelist
            if WHITELIST[k] ~= nil then
                return WHITELIST[k]
            end
            -- 3. Block blacklisted globals
            if BLACKLIST[k] then
                error(string.format(
                    "[sandbox] Access denied: '%s' is blacklisted", tostring(k)), 2)
            end
            -- 4. Nil for disallowed
            return nil
        end,

        __newindex = function(t, k, v)
            -- Allow writing to env scope only
            if WHITELIST[k] ~= nil then
                error(string.format(
                    "[sandbox] Cannot override built-in: '%s'", tostring(k)), 2)
            end
            if BLACKLIST[k] then
                error(string.format(
                    "[sandbox] Access denied: '%s' is blacklisted", tostring(k)), 2)
            end
            env_table[k] = v
        end,
    }

    setmetatable(proxy, mt)
    return proxy
end

-- ═══════════════════════════════════════════════════════════════════════
--  sandbox.execute(code, env, cancelToken) → ok, result, env
--  Compiles with sandboxed _ENV, runs with pcall, supports cancel.
-- ═══════════════════════════════════════════════════════════════════════

function Sandbox.execute(code, env, cancelToken)
    if not code or #code == 0 then
        return true, nil, env
    end

    env = env or {}

    -- Create sandboxed _ENV proxy
    local sandbox_env = Sandbox.create(env)

    -- Compile with sandboxed environment
    local fn, compileErr = load(code, "=sandbox", "t", sandbox_env)
    if not fn then
        print("[sandbox] Compile error: " .. tostring(compileErr))
        return false, compileErr, env
    end

    -- Inject cancel awareness if token provided
    if cancelToken then
        env._CANCEL_TOKEN = cancelToken
        env._SANDBOX_CHECK_CANCEL = function()
            if cancelToken.cancelled then
                error("[sandbox] Execution cancelled")
            end
        end
    end

    -- Set print redirect in env for sandboxed code
    env.print = Sandbox.print_redirect

    local ok, result = pcall(fn)

    if ok then
        return true, result, env
    else
        print("[sandbox] Runtime error: " .. tostring(result))
        return false, result, env
    end
end

-- ═══════════════════════════════════════════════════════════════════════
--  sandbox.is_strict() → bool
--  Returns true when NOT in dev_mode (Release builds enforce sandbox).
--  In dev_mode, eval/emb use the lax (old) path.
-- ═══════════════════════════════════════════════════════════════════════

function Sandbox.is_strict()
    -- Check _CAESURA_CONFIG global set by engine config
    local cfg = rawget(_G, "_CAESURA_CONFIG") or {}
    if cfg.dev_mode ~= nil then
        return not cfg.dev_mode
    end
    -- If _CAESURA_DEBUG is set, dev mode → lax sandbox
    if rawget(_G, "_CAESURA_DEBUG") then
        return false
    end
    -- Default: strict in all other cases
    return true
end

-- ═══════════════════════════════════════════════════════════════════════
--  sandbox.whitelist / blacklist — expose for introspection
-- ═══════════════════════════════════════════════════════════════════════

Sandbox.whitelist = WHITELIST
Sandbox.blacklist = BLACKLIST
Sandbox.forbidden_kag = FORBIDDEN_KAG

-- ═══════════════════════════════════════════════════════════════════════
--  sandbox.print_redirect(...)
--  Redirects sandboxed print() calls to the engine log.
-- ═══════════════════════════════════════════════════════════════════════

function Sandbox.print_redirect(...)
    local args = { ... }
    local parts = {}
    for i = 1, select("#", ...) do
        parts[i] = tostring(args[i])
    end
    print("[sandbox:print] " .. table.concat(parts, "\t"))
end

return Sandbox
