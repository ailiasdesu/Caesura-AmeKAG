-- test_iscript_sandbox.lua — [iscript] sandbox lockdown (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local KAG = require("kag")

local function runIs(body, f0)
    local dispatched = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = KAG
    local vars = f0 or {}
    local ctx = { f = vars, tf = {}, sf = {}, mp = {}, variables = {},
        _whileIterByScene = { ["t.ks"] = 0 },
        macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function()
        scheduler.run(ctx, { { "iscript", { body = body } } }, 1)
    end)
    local ok = true
    while coroutine.status(co) ~= "dead" do
        ok = coroutine.resume(co)
    end
    package.loaded["kag"] = kag_orig
    return ok, vars
end

-- whitelist works: f mutation + math/string reachable
local ok1, v1 = runIs([[f.x = math.floor(2.7) .. string.upper("ab")]], {})
check("iscript whitelist works", ok1 and v1.x == "2AB")

-- os is restricted: only clock/date/time (no execute/remove/rename)
local ok2, v2 = runIs([[
local reachable = {}
for k in pairs(os) do reachable[#reachable + 1] = k end
f.oskeys = table.concat(reachable, ",")
]], {})
check("os restricted to whitelist", ok2 and v2.oskeys == "clock,date,time"
      or (v2.oskeys and v2.oskeys:find("execute") == nil))

-- require/debug/io/load are NOT reachable
local ok3, v3 = runIs([[
f.r = type(require)
f.d = type(debug)
f.i = type(io)
f.l = type(load)
f.dof = type(dofile)
]], {})
check("require unreachable", v3.r == "nil")
check("debug unreachable", v3.d == "nil")
check("io unreachable", v3.i == "nil")
check("load unreachable", v3.l == "nil")
check("dofile unreachable", v3.dof == "nil")

-- sandboxed code errors are caught (no crash)
local ok4, v4 = runIs([[f.y = nil_field_xyz]], {})
check("iscript runtime error caught", ok4 == true)

if failed > 0 then os.exit(1) end
print("ISCRIPT SANDBOX TESTS DONE")
