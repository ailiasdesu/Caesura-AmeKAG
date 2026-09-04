-- test_errorui_wiring.lua — t212: runtime command error -> ErrorUI chain (G1/G2/G4)
-- Harness style: run_lua_tests.lua convention (check()/PASS/FAIL counters).
-- Suggested registration: tests/scripts/run_lua_tests.lua main suite (captain
-- wiring per task; this file is the new deliverable, registration deferred).
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local kag_runner = require("kag_runner")

-- T1: install_error_handler installs a default on a fresh ctx
do
    local ctx = {}
    local h = kag_runner.install_error_handler(ctx)
    check("T1 default handler installed", type(h) == "function" and ctx.handle_error == h)
end

-- T2: first-definition-wins — a pre-set handler is kept untouched
do
    local custom = function() end
    local ctx = { handle_error = custom }
    local h = kag_runner.install_error_handler(ctx)
    check("T2 first-definition-wins", h == custom and ctx.handle_error == custom)
end

-- T3: default handler routes Engine.report_command_error(cmd, msg, scene, line)
--     with G4 traceback attached and Debug.log visibility (both stubs set)
do
    local engCalls, dbgCalls = {}, {}
    _G.Engine = { report_command_error = function(cmd, err, scene, line)
        engCalls[#engCalls + 1] = { cmd, err, scene, line } end }
    _G.Debug = { log = function(level, msg) dbgCalls[#dbgCalls + 1] = { level, msg } end }
    local ctx = {}
    kag_runner.install_error_handler(ctx)
    ctx.handle_error("ch", "boom: nil pointer", "assets/script/main.ks", 42)

    check("T3a Engine.report_command_error called once", #engCalls == 1)
    local c = engCalls[1] or {}
    check("T3b args: cmd/scene/line correct", c[1] == "ch" and c[3] == "assets/script/main.ks" and c[4] == 42)
    check("T3c G4 traceback attached to err", type(c[2]) == "string" and c[2]:find("stack traceback", 1, true) ~= nil)
    check("T3d Debug.log error entry present", #dbgCalls == 1 and dbgCalls[1][1] == "error"
          and dbgCalls[1][2]:find("[ErrorUI]", 1, true) ~= nil)
    _G.Engine, _G.Debug = nil, nil
end

-- T4: fallback — no Engine/Debug bindings: handler must not throw (print path)
do
    _G.Engine, _G.Debug = nil, nil
    local ctx = {}
    kag_runner.install_error_handler(ctx)
    local ok, err = pcall(ctx.handle_error, "assert", "assertion failed", "?", 7)
    check("T4 fallback without bindings does not throw", ok == true)
    _G.Engine, _G.Debug = nil, nil
end

-- T5: scheduler contract — the error hook arguments include scene + line and
--     the ctx stashes error_command/error_token_line (source contract lock;
--     the full dispatch path is exercised by the engine smoke run).
--     Static check: the installed default is what runtime errors reach, and
--     the scheduler block for handle_error passes 4 args (cmd, err, scene, line).
do
    local src = io.open("scripts/scheduler.lua", "rb")
    local body = src and src:read("*a") or ""
    if src then src:close() end
    check("T5 scheduler passes scene+line to handle_error",
          body:find("ctx.error_command = actual_cmd", 1, true) ~= nil
          and body:find("ctx.error_token_line", 1, true) ~= nil
          and body:find("pcall(ctx.handle_error, actual_cmd, tostring(err),", 1, true) ~= nil)
end

print(string.format("test_errorui_wiring: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
