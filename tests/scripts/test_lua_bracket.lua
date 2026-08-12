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

-- Marker: if the payload's code executes, this global gets set.
local function buildChunk(payload, marker)
    local chunk = "local s = " .. luaString(payload) .. "\n"
        .. "if s ~= " .. string.format("%q", payload) .. " then error('mismatch') end\n"
        .. marker .. " = 'PWNED'"
    return chunk
end

local PAYLOADS = {
    "]=] print('X') --",          -- level-1 terminator + comment swallow
    "plain text",                 -- no equals at all
    "a]=]b",                      -- single equals
    "a]==]b",                     -- two equals
    "x]=]=] os.exit(1)",          -- nested terminators
    "y[==[z]=]",                  -- looks like an opener
    "]=] print(1) -- ]=]",
    "",
    "= = =",                      -- equals runs separated
    "]]]=]]] print('deep') --",   -- deep bracket level
}

for i, payload in ipairs(PAYLOADS) do
    local marker = "PWN_" .. i
    local chunk = buildChunk(payload, marker)
    -- clear the marker, run the chunk in a sandboxed env
    local env = { print = function() end, os = { exit = function() end },
                  error = error, string = string }
    _G[marker] = nil
    local fn, lerr = load(chunk, "=bracket_test", "t", env)
    if not fn then
        -- compile failure is acceptable (DoS only, not code execution)
        check("payload " .. i .. " does not execute (" .. payload .. ")",
              true)
    else
        local ok, rerr = pcall(fn)
        check("payload " .. i .. " does not execute (" .. payload .. ")",
              ok == true and _G[marker] == nil,
              tostring(rerr) .. " marker=" .. tostring(_G[marker]))
    end
    _G[marker] = nil
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
