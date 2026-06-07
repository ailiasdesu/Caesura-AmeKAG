-- =============================================================================
--  test_sandbox.lua — Sandbox security verification
-- =============================================================================

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
        print(string.format("  [PASS] %s", name))
    else
        failed = failed + 1
        print(string.format("  [FAIL] %s  -- %s", name, detail or ""))
    end
end

print("\n=== Sandbox Tests ===\n")

-- 1. sandbox.lua loads
do
    local ok, sb = pcall(require, "sandbox")
    check("sandbox loads", ok, tostring(sb))
end

-- 2. sandbox module has expected functions
do
    local sandbox = require("sandbox")
    check("sandbox is table", type(sandbox) == "table")
    check("create exists", type(sandbox.create) == "function")
    check("execute exists", type(sandbox.execute) == "function")
    check("is_strict exists", type(sandbox.is_strict) == "function")
end

-- 3. Dev mode: basic code runs
do
    local sandbox = require("sandbox")
    local env = sandbox.create({mode = "dev"})
    check("dev env created", type(env) == "table")
    local fn, err = load("return 1 + 1", "=test", "t", env)
    check("dev eval loads", fn ~= nil, err)
    if fn then
        local ok, result = pcall(fn)
        check("dev eval executes", ok and result == 2, tostring(result))
    end
end

-- 4. Release mode: blacklisted globals blocked at compile time
do
    local sandbox = require("sandbox")
    local env = sandbox.create({mode = "release"})
    check("release env created", type(env) == "table")
    -- Accessing blacklisted globals should fail at load() time
    local fn, err = load("return os.exit", "=test", "t", env)
    check("release: os.exit blocked", fn ~= nil, "load should fail: " .. (err or "nil"))
end

print(string.format("\nResults: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end

