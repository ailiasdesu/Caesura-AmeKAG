-- =============================================================================
--  run_lua_tests.lua — Standalone Lua test runner
--  Usage: lua tests/scripts/run_lua_tests.lua
-- =============================================================================

-- Add scripts/ to package.path
local test_dir = arg and arg[0] and arg[0]:match("(.*[/\\])") or ""
package.path = test_dir .. "../../scripts/?.lua;" .. test_dir .. "?.lua;" .. package.path

-- Save globals that sandbox may replace
local _real_os_exit = os.exit
local _real_dofile  = dofile

-- NOTE: test_kag_commands must run BEFORE test_scheduler because
-- scheduler internally loads kag module which caches a partial table.
local tests = {
    "test_tokenizer",
    "test_kag_commands",
    "test_scheduler",
    "test_sandbox",
}

local passed, failed = 0, 0
print("\n=== Caesura Lua Test Suite ===\n")

for _, name in ipairs(tests) do
    print(string.format("Running %s.lua...", name))
    local ok, err = pcall(function() _real_dofile(test_dir .. name .. ".lua") end)
    if ok then
        passed = passed + 1
        print(string.format("  [OK] %s\n", name))
    else
        failed = failed + 1
        print(string.format("  [FAIL] %s: %s\n", name, tostring(err)))
    end
end

print(string.format("Results: %d passed, %d failed, %d total",
    passed, failed, passed + failed))
if failed > 0 then _real_os_exit(1) end
