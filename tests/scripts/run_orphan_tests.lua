-- =============================================================================
--  run_orphan_tests.lua — Standalone runner for tests that are order-
--  incompatible with the main suite (they create global mocks like
--  _G.layers/_G.backend and therefore must NOT run after sandbox.lua has
--  locked globals, nor pollute the main suite's module cache).
--
--  Usage: lua tests/scripts/run_orphan_tests.lua
--  Each test runs in this process sequentially; they were verified to pass
--  standalone (round 13: orphan-recovery audit). CI invokes this as a
--  separate step AFTER run_lua_tests.lua.
-- =============================================================================

local test_dir = arg and arg[0] and arg[0]:match("(.*[/\\])") or ""
package.path = test_dir .. "../../scripts/?.lua;" .. test_dir .. "?.lua;" .. package.path

local tests = {
    "test_tokenizer_adv",
    "test_full_story_parse",
    "test_integration",
    "test_kag_core",
    "test_kag_e2e",
    "test_kag_system_flow",
    "test_macro",
    "test_p2_features",
    "test_contracts_runtime",
}

local passed, failed = 0, 0
print("=== Caesura Orphan Test Suite (isolated) ===\n")

-- Round 52 fix: tests that call os.exit (test_kag_core exits 0 on
-- success) would terminate THIS process mid-loop, silently skipping every
-- later test. Run each test in its own lua subprocess so os.exit is
-- contained (exit 0 = pass, non-zero = fail).
local lua_bin = arg and arg[-1] or "lua"
local function run_isolated(name)
    -- Windows popen: cmd.exe fails when the executable is quoted
    -- ("external\lua\lua.exe ..." -> 'external' not recognized), but
    -- unquoted works as long as the path has no spaces. The test file
    -- path is quoted (it may contain spaces).
    local bin = lua_bin:gsub('/', '\\')
    local f = (test_dir .. name .. '.lua'):gsub('/', '\\')
    local cmd = bin .. ' "' .. f .. '"'
    local pf = io.popen(cmd)
    if not pf then return false, "cannot spawn" end
    local out = pf:read("*a")
    local ok = pf:close()
    if out and #out > 0 then
        print(out:sub(1, math.min(#out, 4000)))
    end
    -- io.popen close: true when exit code 0 (Windows/Lua convention)
    return ok, ok and "" or "subprocess exited non-zero"
end

for _, name in ipairs(tests) do
    print(string.format("Running %s.lua...", name))
    local ok, err = run_isolated(name)
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
if failed > 0 then os.exit(1) end
