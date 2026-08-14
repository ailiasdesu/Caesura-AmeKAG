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
}

local passed, failed = 0, 0
print("=== Caesura Orphan Test Suite (isolated) ===\n")
for _, name in ipairs(tests) do
    print(string.format("Running %s.lua...", name))
    local ok, err = pcall(dofile, test_dir .. name .. ".lua")
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
