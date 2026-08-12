-- test_lua_bracket.lua — Lua long-bracket escaping contract tests.
-- The editor's luaString (editor/src/lib/luaString.ts) picks level
-- N = (longest run of '=' in the content) + 1. These tests verify the
-- SEMANTIC contract on the real Lua 5.4 parser: for a set of malicious
-- payloads, a chunk built with that level either round-trips the string
-- or fails to compile — it must NEVER execute the payload's code.
-- (The TS implementation is typecheck/build-verified separately; this
-- file pins the language-level invariant it relies on.)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
        failed = failed + 1 end
end

-- Same algorithm as editor/src/lib/luaString.ts
local function luaString(s)
    local maxRun = 0
    for run in tostring(s):gmatch("=+") do
        if #run > maxRun then maxRun = #run end
    end
    local eq = string.rep("=", maxRun + 1)
    return "[" .. eq .. "[" .. tostring(s) .. "]" .. eq .. "]"
end

-- Payloads may embed __MARK__ as their "code": if the bracket escaping
-- lets the payload break out, the payload's statement executes and sets
-- env[__MARK__] — the oracle detects it. Payloads without __MARK__ are
-- checked by the round-trip assertion (a broken escape makes s ~= p and
-- the chunk errors).
local PAYLOADS = {
    "]=] __MARK__ = 'PWNED' --",        -- level-1 terminator + comment swallow
    "plain text",                       -- no equals at all
    "a]=]b",                            -- single equals, no code
    "a]==]b",                           -- two equals, no code
    "x]=]=] __MARK__ = 'PWNED'",        -- nested terminators
    "y[==[z]=]",                        -- looks like an opener
    "]=] __MARK__ = 'PWNED' -- ]=]",    -- double terminator
    "",                                 -- empty
    "= = =",                            -- equals runs separated
    "]]]=]]] __MARK__ = 'PWNED' --",    -- deep bracket level
}

for i, payload in ipairs(PAYLOADS) do
    local marker = "PWN_" .. i
    local p = payload:gsub("__MARK__", marker)
    local chunk = "local s = " .. luaString(p) .. "\n"
        .. "if s ~= " .. string.format("%q", p) .. " then error('mismatch') end"
    -- run in a per-payload env; marker lands on the env (not _G) because
    -- load() binds the chunk to `env`
    local env = { print = function() end, os = { exit = function() end },
                  error = error, string = string }
    local fn, lerr = load(chunk, "=bracket_test", "t", env)
    if not fn then
        -- compile failure is acceptable (DoS only, not code execution)
        check("payload " .. i .. " does not execute (" .. payload .. ")",
              true)
    else
        local ok, rerr = pcall(fn)
        check("payload " .. i .. " does not execute (" .. payload .. ")",
              ok == true and env[marker] == nil,
              tostring(rerr) .. " marker=" .. tostring(env[marker]))
    end
end

-- The escaped string must round-trip exactly (no data corruption).
local round = "a]=]b]==]c"
local rchunk = "return " .. luaString(round)
local rfn = assert(load(rchunk, "=roundtrip", "t", {}))
check("round-trip exact", rfn() == round)

if failed > 0 then
    print(string.format("LUA_BRACKET TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("LUA_BRACKET TESTS DONE (%d passed)", passed))
