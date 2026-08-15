-- test_eval_env.lua — [eval] whitelist env (audit hardening)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")
local KAG = require("kag")

local function runEval(exp, f0)
    local dispatched = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = KAG
    local vars = f0 or {}
    local ctx = { f = vars, tf = {}, sf = {}, mp = {}, variables = {},
        _whileIterByScene = { ["t.ks"] = 0 },
        macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function()
        scheduler.run(ctx, { { "eval", { exp = exp } } }, 1)
    end)
    local ok = true
    while coroutine.status(co) ~= "dead" do ok = coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    return ok, vars, ctx
end

-- whitelist works: f mutation + math/string
local ok1, v1 = runEval([[f.x = math.floor(2.7) .. string.upper("ab")]], {})
check("eval whitelist works", ok1 and v1.x == "2AB")

-- os restricted (execute unreachable)
local ok2, v2 = runEval([[f.osx = type(os.execute)]], {})
check("eval os.execute unreachable", v2.osx == "nil")

-- require/debug/io/load unreachable
local ok3, v3 = runEval([[
f.r = type(require)
f.d = type(debug)
f.i = type(io)
f.l = type(load)
f.dof = type(dofile)
]], {})
check("eval require unreachable", v3.r == "nil")
check("eval debug unreachable", v3.d == "nil")
check("eval io unreachable", v3.i == "nil")
check("eval load unreachable", v3.l == "nil")
check("eval dofile unreachable", v3.dof == "nil")

-- statement semantics: [eval] executes statements (f writes visible)
local ok4, v4 = runEval([[f.answer = 40 + 2]], {})
check("eval statement executes", ok4 and v4.answer == 42)

-- round 71: bare VALUE expressions compile via a "return " wrap (doc
-- contract: a bare [eval] stores its result in tf.eval_result).
local ok5, v5, c5 = runEval([[f.hp * 2]], { hp = 21 })
check("eval bare value expression stores result",
    ok5 and c5.tf.eval_result == 42, tostring(c5 and c5.tf.eval_result))

local ok6, v6, c6 = runEval([[f.a ? 10 : 20]], { a = true })
check("eval bare ternary stores result",
    ok6 and c6.tf.eval_result == 10, tostring(c6 and c6.tf.eval_result))

local ok7, v7, c7 = runEval([[f.a && f.b ? 1 : 0]], { a = true, b = false })
check("eval bare TJS ternary stores result",
    ok7 and c7.tf.eval_result == 0, tostring(c7 and c7.tf.eval_result))

-- invalid expressions still surface the compile error, no crash, no result
local ok8, v8, c8 = runEval([[f.x +]], {})
check("eval invalid expression safe", ok8 and c8.tf.eval_result == nil)

if failed > 0 then os.exit(1) end
print("EVAL ENV TESTS DONE")
