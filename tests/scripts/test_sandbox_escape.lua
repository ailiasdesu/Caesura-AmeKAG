-- =============================================================================
--  test_sandbox_escape.lua — sandbox ESCAPE attempts must be REFUSED
-- =============================================================================
--  Companion to test_sandbox.lua (which proves the ALLOWED paths still work).
--  Every assertion here is "the escape is denied": it fails loudly if a future
--  refactor re-opens rawset(_G), debug.getmetatable(_G), load() inside a
--  sandbox, or the _G write protection.
--
--  Run standalone:  external/lua/lua.exe tests/scripts/test_sandbox_escape.lua
--  In the suite:    it must run AFTER test_sandbox (both need the lockdown to
--                   have happened; sandbox.lua is idempotent, so requiring it
--                   here is safe either way).
-- =============================================================================
package.path = "scripts/?.lua;" .. package.path

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
        print("PASS " .. name)
    else
        failed = failed + 1
        print("FAIL " .. name .. (detail and ("  -- " .. tostring(detail)) or ""))
    end
end

-- Apply the lockdown (idempotent: the suite may already have run it).
local Sandbox = require("sandbox")

-- ---------------------------------------------------------------------------
-- 1. rawset(_G, ...) must NOT bypass the __newindex write protection
-- ---------------------------------------------------------------------------
--  This was the headline escape: rawset ignores __newindex by definition, so
--  one line created any global. The refusal must use the SAME message as a
--  plain assignment, otherwise the two paths are distinguishable.
do
    local ok, err = pcall(function() rawset(_G, "evil", 1) end)
    check("rawset(_G, 'evil') refused", not ok, err)
    check("rawset(_G, 'evil') left no global", rawget(_G, "evil") == nil)
    check("rawset refusal matches assignment wording",
          (not ok) and type(err) == "string"
          and err:find("cannot create global", 1, true) ~= nil, err)

    -- A whitelisted key must still be raw-writable: scripts/system.lua does
    -- exactly this for quicksave/quickload/autosave.
    local restore = rawget(_G, "autosave")
    local ok2, err2 = pcall(function() rawset(_G, "autosave", function() end) end)
    check("rawset(_G, 'autosave') still allowed", ok2, err2)
    rawset(_G, "autosave", restore)

    -- rawset on a NON-_G table is untouched (the ctx.tf "no __newindex trap"
    -- invariant used by [emb]/[eval] must keep working).
    local t = setmetatable({}, { __newindex = function() error("trapped") end })
    local ok3 = pcall(function() rawset(t, "x", 1) end)
    check("rawset on a plain table still works", ok3 and t.x == 1)
end

-- ---------------------------------------------------------------------------
-- 2. debug.getmetatable must not hand out the REAL _G metatable
-- ---------------------------------------------------------------------------
--  __metatable = "protected" only blocks the Lua-level getmetatable;
--  debug.getmetatable returned the live table, and rewriting its __newindex
--  disabled the protection entirely.
do
    check("debug.getmetatable removed", debug.getmetatable == nil)
    check("debug.getupvalue removed (leaks captured rawset/load)",
          debug.getupvalue == nil)
    check("debug.getuservalue removed", debug.getuservalue == nil)
    check("debug.setmetatable still absent", debug.setmetatable == nil)
    -- The Lua-level route only ever yields the marker, never a table.
    local mt = getmetatable(_G)
    check("getmetatable(_G) yields the marker, not a table", mt == "protected", tostring(mt))
    -- debug.getinfo / traceback stay (used by demo entry.lua + test_integration)
    check("debug.getinfo retained", type(debug.getinfo) == "function")
    check("debug.traceback retained", type(debug.traceback) == "function")
end

-- ---------------------------------------------------------------------------
-- 3. A sandbox ENVIRONMENT must not expose load / rawset / raw metatable ops
-- ---------------------------------------------------------------------------
do
    local env = Sandbox.create({ mode = "release" })
    local ok, probe = Sandbox.execute([[
        return table.concat({ type(load), type(loadstring), type(rawset),
                              type(rawget), type(dofile), type(loadfile),
                              type(require), type(debug) }, ",")
    ]], env)
    check("sandbox env probe runs", ok, probe)
    check("load/loadstring/rawset/rawget/dofile/loadfile/require/debug all nil in sandbox env",
          probe == "nil,nil,nil,nil,nil,nil,nil,nil", tostring(probe))

    -- getmetatable inside a sandbox never returns a live metatable table.
    local ok2, probe2 = Sandbox.execute([[
        local t = setmetatable({}, { __index = function() return 7 end })
        return type(getmetatable(t)) .. "/" .. tostring(t.anything)
    ]], Sandbox.create({ mode = "release" }))
    check("sandbox getmetatable returns no table", ok2 and probe2 == "nil/7",
          tostring(probe2))

    -- setmetatable cannot be aimed at _G, nor replace an existing metatable.
    local ok3, probe3 = Sandbox.execute([[
        local a = pcall(setmetatable, _G, {})
        local t = setmetatable({}, {})
        local b = pcall(setmetatable, t, { __index = function() return 1 end })
        return tostring(a) .. "/" .. tostring(b)
    ]], Sandbox.create({ mode = "release" }))
    check("sandbox setmetatable refuses _G and metatable replacement",
          ok3 and probe3 == "false/false", tostring(probe3))
end

-- ---------------------------------------------------------------------------
-- 4. load() is a TRUSTED-HOST channel: a dynamic chunk cannot compile
-- ---------------------------------------------------------------------------
--  Host modules under scripts/ and tests/ keep compiling (config.lua, i18n.lua,
--  kag/expr.lua, scheduler.lua [iscript]/[eval] all depend on it), but a chunk
--  that was itself compiled at runtime is refused.
do
    -- This test file lives under tests/ -> trusted, load works here.
    local fn = load("return 1 + 1", "=trusted_probe", "t", {})
    check("trusted host module can still compile", type(fn) == "function"
          and fn() == 2)

    -- A dynamically compiled chunk trying to compile again is refused.
    local outer = load([[
        local f, err = load("return 1", "=inner", "t", {})
        return tostring(f) .. "|" .. tostring(err)
    ]], "=dynamic_probe", "t", { load = load, tostring = tostring })
    check("dynamic probe compiles (host frame)", type(outer) == "function")
    local okOuter, res = pcall(outer)
    check("nested load from a dynamic chunk refused",
          okOuter and type(res) == "string" and res:find("^nil|") ~= nil
          and res:find("Sandbox", 1, true) ~= nil, tostring(res))

    -- Binary chunks from an untrusted caller are refused with a distinct
    -- message (Lua does not validate bytecode -- memory safety, not just
    -- sandboxing).
    local dumped = string.dump(function() return 1 end)
    local outer2 = load([[
        local f, err = load(BC, "=inner_bc", "b", {})
        return tostring(f) .. "|" .. tostring(err)
    ]], "=dynamic_bc", "t", { load = load, tostring = tostring, BC = dumped })
    local okOuter2, res2 = pcall(outer2)
    check("nested binary load from a dynamic chunk refused",
          okOuter2 and type(res2) == "string"
          and res2:find("binary chunks are host%-only") ~= nil, tostring(res2))

    check("loadstring global removed", rawget(_G, "loadstring") == nil)
    check("loadfile global removed", rawget(_G, "loadfile") == nil)
    check("dofile global removed", rawget(_G, "dofile") == nil)

    -- ARITY REGRESSION (the guard must not change load's calling contract).
    -- Lua distinguishes an ABSENT 4th argument (chunk inherits the caller's
    -- _ENV) from an explicitly passed nil (chunk gets _ENV = nil). A guard
    -- that declares four parameters and forwards them unconditionally turns
    -- every 2-arg / 3-arg call into the second case, and the chunk dies on its
    -- first global access with "attempt to index a nil value (upvalue '_ENV')".
    -- Real 2-3 arg callers: tests/scripts/test_label_jump.lua:17,50,
    -- test_ks_bake.lua:124, test_example_game.lua:87, scripts/music_room.lua:117,
    -- and the pcall(load, ...) sites in scripts/kag/expr.lua +
    -- scripts/kag/compiler.lua (the [if]/[eval]/[emb] expression path).
    do
        -- 2 args: env ABSENT -> globals must be reachable.
        local f2, e2 = load("return type(tostring)", "=arity2")
        check("2-arg load compiles", type(f2) == "function", tostring(e2))
        local ok2, r2 = pcall(f2)
        check("2-arg load chunk keeps caller _ENV (globals reachable)",
              ok2 and r2 == "function", tostring(r2))

        -- 3 args: mode given, env ABSENT -> same requirement.
        local f3, e3 = load("return type(tostring)", "=arity3", "t")
        check("3-arg load compiles", type(f3) == "function", tostring(e3))
        local ok3, r3 = pcall(f3)
        check("3-arg load chunk keeps caller _ENV", ok3 and r3 == "function",
              tostring(r3))

        -- 4 args with an EXPLICIT nil env: Lua semantics say _ENV = nil, so
        -- this MUST still fail. Collapsing it into the absent case would mean
        -- the sandbox silently rewrites the language for its callers.
        local f4 = load("return type(tostring)", "=arity4", "t", nil)
        check("4-arg explicit-nil env still compiles", type(f4) == "function")
        local ok4, r4 = pcall(f4)
        check("4-arg explicit-nil env keeps Lua semantics (_ENV is nil)",
              not ok4 and type(r4) == "string"
              and r4:find("_ENV", 1, true) ~= nil, tostring(r4))

        -- 4 args with a real env table: the sandbox env path itself.
        local f5 = load("return X", "=arity5", "t", { X = 42 })
        local ok5, r5 = pcall(f5)
        check("4-arg table env honoured", ok5 and r5 == 42, tostring(r5))

        -- pcall(load, ...) — the exact shape kag/expr.lua:567,585 and
        -- kag/compiler.lua:610,632 use, including the binary AOT path.
        local okp, cp = pcall(load, "return 1 + 1", "=kag_expr_probe", "t", {})
        check("pcall(load, 4 args) works (expr.lua shape)",
              okp and type(cp) == "function" and cp() == 2)
        local okp2, cp2 = pcall(load, "return type(tostring)", "=kag_expr_probe2")
        check("pcall(load, 2 args) keeps caller _ENV",
              okp2 and type(cp2) == "function" and cp2() == "function")
        local okb, cb = pcall(load, string.dump(function() return 7 end),
                              "=kag_expr_bc", "b", {})
        check("pcall(load, binary) works for a trusted caller (AOT path)",
              okb and type(cb) == "function" and cb() == 7)
    end
end

-- ---------------------------------------------------------------------------
-- 5. The ORIGINAL _G write protection still behaves exactly as before
-- ---------------------------------------------------------------------------
do
    local ok, err = pcall(function() _G._ESCAPE_TEST_NOT_WHITELISTED = 1 end)
    check("non-whitelisted global assignment still refused", not ok, err)
    check("refusal message unchanged",
          (not ok) and type(err) == "string"
          and err:find("Sandbox: cannot create global", 1, true) ~= nil, err)
    check("no global leaked", rawget(_G, "_ESCAPE_TEST_NOT_WHITELISTED") == nil)

    -- Whitelisted globals still assignable (quicksave/autosave and friends).
    local restoreQ, restoreA = rawget(_G, "quicksave"), rawget(_G, "autosave")
    local okQ = pcall(function() _G.quicksave = function() end end)
    local okA = pcall(function() _G.autosave = function() end end)
    check("whitelisted quicksave still assignable", okQ)
    check("whitelisted autosave still assignable", okA)
    rawset(_G, "quicksave", restoreQ)
    rawset(_G, "autosave", restoreA)

    -- _SANDBOX_MODE is NOT whitelisted: a script must not downgrade the mode.
    local okMode = pcall(function() _G._SANDBOX_MODE = "dev" end)
    check("_SANDBOX_MODE cannot be reassigned", not okMode)
    local okModeRaw = pcall(function() rawset(_G, "_SANDBOX_MODE", "dev") end)
    check("_SANDBOX_MODE cannot be raw-written either", not okModeRaw)
    check("_SANDBOX_MODE intact", _G._SANDBOX_MODE == "strict"
          or _G._SANDBOX_MODE == "dev")
end

print(string.format("\nResults: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
print("SANDBOX ESCAPE TESTS DONE")
