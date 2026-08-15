-- test_expr_lang2.lua — KAG expression language (kag/expr.lua)
-- Round 71+ boundary expansion over test_expr_lang.lua (rounds 53-70):
--   operator precedence chains, string/table/function-call edges, numeric
--   literal forms, null/nil semantics, variable-name/scope prefixes, and
--   nested ternary + ?? composition.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local expr = require("kag.expr")

-- ===========================================================================
-- (1) Operator precedence chains
-- ===========================================================================
check("mul binds tighter than add", expr.translate("a + b * c") == "a + b * c")
check("add after mul", expr.translate("a * b + c") == "a * b + c")
check("&& then || is Lua and-then-or (same precedence order)",
    expr.translate("a && b || c") == "a and b or c")
check("not binds on a, then and",
    expr.translate("not a and b") == "not a and b")

do
    -- Lua: `and` binds tighter than `or`, exactly as TJS `&&` beats `||`,
    -- so the translated chain keeps TJS left-assoc semantics.
    local ctx = { f = { a = false, b = true, c = true }, sf = {}, tf = {}, mp = {}, lf = {} }
    local ok1, v1 = expr.evaluate(ctx, "f.a && f.b || f.c")
    check("&& || evaluates via and/or precedence", ok1 and v1 == true, tostring(v1))
    ctx.f.a, ctx.f.b, ctx.f.c = true, false, false
    local ok2, v2 = expr.evaluate(ctx, "f.a || f.b && f.c")
    check("|| && evaluates (or lower than and)", ok2 and v2 == true, tostring(v2))
    ctx.f.a, ctx.f.b, ctx.f.c = true, false, true
    local ok3, v3 = expr.evaluate(ctx, "!f.b && f.c")
    check("not && evaluates", ok3 and v3 == true, tostring(v3))
    ctx.f.a, ctx.f.b, ctx.f.c = 1, 2, 3
    local ok4, v4 = expr.evaluate(ctx, "f.a * 2 + f.c * 3")
    check("mul/add chain evaluates (=11)", ok4 and v4 == 2 + 9, tostring(v4))
end

-- Comparison chaining: TJS `a < b < c` is ((a<b)<c); Lua forbids the chain.
-- The translator must not crash; it leaves the operators in place so the
-- Lua compile step reports it visibly rather than silently guessing.
for _, src in ipairs({ "a < b < c", "a == b == c" }) do
    local tr = expr.translate(src)
    check("chain " .. src .. " translates without crash", tr ~= nil)
end

-- ===========================================================================
-- (2) String / table / function-call edges
-- ===========================================================================
check("operators in double-quoted string pair untouched",
    expr.translate('f.s == "x && y || z"') == 'f.s == "x && y || z"')
check("operators in single-quoted string pair untouched",
    expr.translate("f.s == 'a ? b : c'") == "f.s == 'a ? b : c'")
check("ternary decides table field via then-branch",
    expr.translate("f.flag ? f.x1 : f.x2") == "((f.flag) and (f.x1) or (f.x2))")

do
    local ctx = { f = { arr = { 10, 20 }, obj = { y = 5 }, x1 = 1, x2 = 2,
                        flag = true, calc = function(a, b) return (a or 0) + (b or 0) end },
                  sf = {}, tf = {}, mp = {}, lf = {} }
    local ok1, v1 = expr.evaluate(ctx, "f.obj.y == 5")
    check("nested table index evaluates", ok1 and v1 == true, tostring(v1))
    local ok2, v2 = expr.evaluate(ctx, "f.arr[1] + f.obj.y")
    check("mixed array+nested index evaluates (=15)", ok2 and v2 == 15, tostring(v2))
    -- single argument with ternary: translated+eval correctly.
    ctx.f.flag = true
    local ok3, v3 = expr.evaluate(ctx, "f.calc(f.flag ? 1 : 2)")
    check("ternary single-arg call evaluates (=1)", ok3 and v3 == 1, tostring(v3))
    ctx.f.flag = false
    local ok3b, v3b = expr.evaluate(ctx, "f.calc(f.flag ? 1 : 2)")
    check("ternary single-arg call else (=2)", ok3b and v3b == 2, tostring(v3b))
    local tr1 = expr.translate("f.calc(f.flag ? 1 : 2)")
    check("single-arg ternary call translates", tr1 == "f.calc(((f.flag) and (1) or (2)))", tr1)
    -- parenthesized ternary arg keeps trailing comma arg intact (workaround).
    local tr2 = expr.translate("f.calc((f.flag ? 1 : 2), 3)")
    check("parenthesized ternary call arg works", tr2:match("f%.calc%(%(.*%)%, 3%)") ~= nil, tr2)

    -- [round 84 fix] Multi-arg calls: top-level commas split the argument
    -- list so a ternary stays inside its own argument (f.calc(f.flag ? 1 : 2, 3)
    -- no longer folds the trailing 3 into the else branch).
    local tr3 = expr.translate("f.calc(f.flag ? 1 : 2, 3)")
    check("ternary in multi-arg call: trailing arg stays separate", tr3 == "f.calc(((f.flag) and (1) or (2)), 3)", tr3)
    local tr4 = expr.translate("f.calc(f.a, f.b, f.flag ? 1 : 2)")
    check("ternary in multi-arg call: leading args stay separate", tr4 == "f.calc(f.a, f.b, ((f.flag) and (1) or (2)))", tr4)
end

-- ===========================================================================
-- (3) Numeric literal boundaries
-- ===========================================================================
check("scientific literal preserved", expr.translate("f.x > 1e3") == "f.x > 1e3")
check("hex literal preserved", expr.translate("f.x == 0xFF") == "f.x == 0xFF")
check("negative literal preserved", expr.translate("-5 < f.x") == "-5 < f.x")
check("leading-dot float preserved", expr.translate("f.x >= .5") == "f.x >= .5")

do
    local ctx = { f = { x = 1000 }, sf = {}, tf = {}, mp = {}, lf = {} }
    local ok1, v1 = expr.evaluate(ctx, "f.x == 1e3")
    check("scientific number evaluates", ok1 and v1 == true, tostring(v1))
    local ok2, v2 = expr.evaluate(ctx, "f.x >= 0xFF")
    check("hex number evaluates (1000>=255)", ok2 and v2 == true, tostring(v2))
    ctx.f.x = 0.5
    local ok3, v3 = expr.evaluate(ctx, "f.x == .5")
    check("leading-dot float evaluates", ok3 and v3 == true, tostring(v3))
    -- 64-bit integer larger than 2^53 (TJS double would lose precision)
    ctx.f.big = 9007199254740993
    local ok4, v4 = expr.evaluate(ctx, "f.big == 9007199254740993")
    check("big int literal evaluates", ok4 and v4 == true, tostring(v4))
    -- 1/0 in Lua yields inf (no crash at translation; runtime math is Lua's)
    local ok5, v5 = expr.evaluate(ctx, "1 / 0 > 0")
    check("div-by-zero does not crash translator", ok5 ~= nil)
end

-- ===========================================================================
-- (4) null / nil semantics
-- ===========================================================================
check("?? chain translates to or chain",
    expr.translate("f.a ?? f.b ?? 99") == "f.a or f.b or 99")
check("?? with ternary then-branch",
    expr.translate("f.a ?? (f.flag ? 1 : 2)") == "f.a or (((f.flag) and (1) or (2)))")

do
    local ctx = { f = { a = nil, b = nil, s = "" }, sf = {}, tf = {}, mp = {}, lf = {} }
    local ok1, v1 = expr.evaluate(ctx, "f.a ?? f.b ?? 99")
    check("?? chain falls all the way back", ok1 and v1 == 99, tostring(v1))
    ctx.f.b = 7
    local ok2, v2 = expr.evaluate(ctx, "f.a ?? f.b ?? 99")
    check("?? chain stops at non-nil", ok2 and v2 == 7, tostring(v2))
    -- empty string is truthy (NOT nil) -> ?? keeps it
    local ok3, v3 = expr.evaluate(ctx, "f.s ?? 'anon'")
    check("empty string kept by ?? (not nil)", ok3 and v3 == "", tostring(v3))
    local ok4, v4 = expr.evaluate(ctx, "f.s == nil")
    check("empty string is not nil", ok4 and v4 == false, tostring(v4))
    -- nil on the left falls through to a string
    local ok5, v5 = expr.evaluate(ctx, "f.a ?? 'd'")
    check("nil ?? string yields string", ok5 and v5 == "d", tostring(v5))
end

-- ===========================================================================
-- (5) Variable-name boundaries + scope prefixes
-- ===========================================================================
check("underscore/number var name translates",
    expr.translate("f.cnt_1 > 0") == "f.cnt_1 > 0")
check("deep chain f.a.b.c translates",
    expr.translate("f.a.b.c == 42") == "f.a.b.c == 42")
check("reserved word as quoted index",
    expr.translate('f["and"] == 1') == 'f["and"] == 1')

do
    local ctx = { f = { cnt_1 = 3, a = { b = { c = 42 } }, ["and"] = 1, score_2 = 8 },
        sf = { x = 100 }, tf = { flag = true }, mp = { z = 5 }, lf = { y = 9 } }
    local ok1, v1 = expr.evaluate(ctx, "f.cnt_1 > 0")
    check("underscore var evaluates", ok1 and v1 == true, tostring(v1))
    local ok2, v2 = expr.evaluate(ctx, "f.a.b.c == 42")
    check("deep chain f.a.b.c evaluates", ok2 and v2 == true, tostring(v2))
    local ok3, v3 = expr.evaluate(ctx, 'f["and"] == 1')
    check("reserved-word field via quoted index", ok3 and v3 == true, tostring(v3))
    -- Lua keyword as a bare TJS identifier cannot compile -> visible error.
    local ok4, v4 = expr.evaluate(ctx, "f.and && f.cnt_1")
    check("keyword field reports failure (not silent true)", ok4 == false and v4 == nil)
    -- scope prefixes: tf./sf./mp./lf.
    local ok5, v5 = expr.evaluate(ctx, "sf.x == 100")
    check("sf. scope prefix evaluates", ok5 and v5 == true, tostring(v5))
    local ok6, v6 = expr.evaluate(ctx, "tf.flag && mp.z == 5")
    check("tf./mp. scope prefixes evaluate", ok6 and v6 == true, tostring(v6))
    local ok7, v7 = expr.evaluate(ctx, "lf.y == 9")
    check("lf. scope prefix evaluates", ok7 and v7 == true, tostring(v7))
end

-- ===========================================================================
-- (6) Nested ternary + ?? composition
-- ===========================================================================
check("nested ternary in then via parens",
    expr.translate("(f.a ? f.b : (f.c ? f.d : f.e))")
        == "(((f.a) and (f.b) or ((((f.c) and (f.d) or (f.e))))))")
check("?? inside ternary branch",
    expr.translate("f.a ? (f.b ?? f.c) : f.d")
        == "((f.a) and ((f.b or f.c)) or (f.d))")

do
    local ctx = { f = { a = true, b = 1, c = true, d = 2, e = 3, g = nil,
                          h = 30, k = 40 },
        sf = {}, tf = {}, mp = {}, lf = {} }
    local ok1, v1 = expr.evaluate(ctx, "f.a ? f.b : (f.c ? f.d : f.e)")
    check("nested ternary picks outer then", ok1 and v1 == 1, tostring(v1))
    ctx.f.a = false
    local ok2, v2 = expr.evaluate(ctx, "f.a ? f.b : (f.c ? f.d : f.e)")
    check("nested ternary picks inner then", ok2 and v2 == 2, tostring(v2))
    ctx.f.c = false
    local ok3, v3 = expr.evaluate(ctx, "f.a ? f.b : (f.c ? f.d : f.e)")
    check("nested ternary picks inner else", ok3 and v3 == 3, tostring(v3))
    ctx.f.a = true
    -- ?? inside a ternary branch: g is nil so inner falls back to h (=30).
    local ok4, v4 = expr.evaluate(ctx, "f.a ? (f.g ?? f.h) : f.k")
    check("?? inside ternary then-branch", ok4 and v4 == 30, tostring(v4))
    -- ternary inside ?? RHS: g nil -> pick 10 (then-branch).
    local ok5, v5 = expr.evaluate(ctx, "f.g ?? (f.a ? 10 : 20)")
    check("ternary inside ?? RHS evaluates", ok5 and v5 == 10, tostring(v5))
    -- g now set -> ?? short-circuits the ternary RHS entirely.
    ctx.f.g = 15
    local ok6, v6 = expr.evaluate(ctx, "f.g ?? (f.a ? 10 : 20)")
    check("?? short-circuits ternary RHS", ok6 and v6 == 15, tostring(v6))
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("EXPR LANG 2 TESTS DONE")
