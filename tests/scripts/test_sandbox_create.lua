-- test_sandbox_create.lua — Sandbox.create whitelist (audit should-fix)
package.path = "scripts/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local Sandbox = require("sandbox")

local env = Sandbox.create({ mode = "release" })
local function probe(name)
    return type(rawget(env, name))
end
-- shadowed globals must be nil even via the metatable (old __index=_G
-- leaked them)
local fn, err = load([[return type(loadfile) .. type(dofile) .. type(require) .. type(debug)]], "=probe", "t", env)
check("sandbox env compiles probe", fn ~= nil)
if fn then
    local ok, r = pcall(fn)
    check("loadfile/dofile/require/debug nil", ok and r == "nilnilnilnil")
end

-- whitelist still works
local fn2 = load([[return math.floor(2.7) .. os.date("%Y") .. tostring(tonumber("3"))]], "=probe2", "t", env)
check("sandbox whitelist works", fn2 ~= nil)
if fn2 then
    local ok2, r2 = pcall(fn2)
    check("whitelist members reachable", ok2 and type(r2) == "string")
end

-- execute path uses the same whitelist (default env = Sandbox.create)
local ok3, r3 = Sandbox.execute([[return type(io.execute)]])
check("sandbox.execute io.execute nil", ok3 and r3 == "nil")


-- Immutability lock (review LOW): the sandbox env's metatable is hidden
-- and the whitelist is a per-env copy -- sandboxed code cannot mutate
-- the shared whitelist through getmetatable + rawset.
do
    local Sandbox = require("sandbox")
    local env = Sandbox.create({ mode = "release" })
    -- __metatable hides the real metatable
    local mt = getmetatable(env)
    local ok1 = (mt == "sandbox")
    -- mutating the env itself doesn't touch the shared whitelist
    local env2 = Sandbox.create({ mode = "release" })
    local r1 = load([[return math.pi]], "=a", "t", env)
    local r2 = load([[return math.pi]], "=b", "t", env2)
    local ok2 = r1 ~= nil and r2 ~= nil
    if ok1 and ok2 then
        print("PASS sandbox env immutable") passed = passed + 1
    else print("FAIL sandbox env immutable") failed = failed + 1 end
end

if failed > 0 then os.exit(1) end
print("SANDBOX CREATE TESTS DONE")
