-- test_expr_lang.lua — KAG expression language (kag/expr.lua)
-- TJS-compatible operator translation + ternary + error visibility.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local expr = require("kag.expr")

-- ---- translate: operators ------------------------------------------------
check("&& translates to and", expr.translate("a && b") == "a and b")
check("|| translates to or", expr.translate("a || b") == "a or b")
check("!= translates to ~=", expr.translate("a != b") == "a ~= b")
check("! translates to not", expr.translate("!a") == "not a")
check("! before paren", expr.translate("!(a && b)") == "not (a and b)")
check("mixed chain", expr.translate("a && b || c && !d")
    == "a and b or c and not d")
check("comparison preserved", expr.translate("x == 1 && y < 2")
    == "x == 1 and y < 2")

-- ---- translate: string literals are untouched ----------------------------
check("operators inside double quotes kept",
    expr.translate('s == "a && b"') == 's == "a && b"')
check("operators inside single quotes kept",
    expr.translate("s == 'a || b'") == "s == 'a || b'")
check("escaped quote inside string",
    expr.translate([[s == "a \" && b"]]) == [[s == "a \" && b"]])

-- ---- translate: ternary --------------------------------------------------
check("simple ternary", expr.translate("a ? 1 : 2") == "((a) and (1) or (2))")
check("ternary with operators",
    expr.translate("a && b ? x : y") == "((a and b) and (x) or (y))")
check("nested ternary",
    expr.translate("a ? b ? 1 : 2 : 3")
        == "((a) and (((b) and (1) or (2))) or (3))")
check("ternary with parens",
    expr.translate("(a || b) ? (c && d) : e")
        == "(((a or b)) and ((c and d)) or (e))")
check("ternary inside string not split",
    expr.translate('a ? "x:y" : "z"') == '((a) and ("x:y") or ("z"))')

-- ---- evaluate: TJS expressions against ctx tables ------------------------
do
    local ctx = { f = { hp = 30, mp = 5 }, tf = { flag = true },
                  sf = {}, current_scene = "t.ks", token_index = 3 }
    local ok, v = expr.evaluate(ctx, "f.hp > 20 && f.mp >= 5")
    check("TJS && evaluates true", ok and v == true)
    local ok2, v2 = expr.evaluate(ctx, "tf.flag && f.hp != 0")
    check("TJS != evaluates true", ok2 and v2 == true)
    local ok3, v3 = expr.evaluate(ctx, "!tf.flag || f.hp == 30")
    check("TJS ! evaluates true", ok3 and v3 == true)
    local ok4, v4 = expr.evaluate(ctx, "f.hp > 20 ? 100 : 0")
    check("ternary picks then", ok4 and v4 == 100)
    local ok5, v5 = expr.evaluate(ctx, "f.hp < 20 ? 100 : 0")
    check("ternary picks else", ok5 and v5 == 0)
    local ok6, v6 = expr.evaluate(ctx, "f.hp == 30 ? 1 : 2 ? 3 : 4")
    check("nested ternary picks outer then", ok6 and v6 == 1)
end

-- ---- evaluate: visible errors (no silent false) --------------------------
do
    local ctx = { f = {}, tf = {}, sf = {}, mp = {},
                  current_scene = "err.ks", token_index = 7 }
    local printed = {}
    local realPrint = print
    print = function(...) printed[#printed + 1] = table.concat({...}, " ") end
    local ok, v = expr.evaluate(ctx, "tf.missing ==")
    print = realPrint
    check("syntax error returns false", ok == false and v == nil)
    check("syntax error printed with location",
        #printed > 0 and printed[1]:find("err.ks:7", 1, true) ~= nil)
    check("syntax error message mentions expression",
        #printed > 0 and printed[1]:find("expression error", 1, true) ~= nil)

    local printed2 = {}
    print = function(...) printed2[#printed2 + 1] = table.concat({...}, " ") end
    local ok2, v2 = expr.evaluate(ctx, "tf.nil_table.field == 1")
    print = realPrint
    check("runtime error returns false", ok2 == false and v2 == nil)
    check("runtime error printed",
        #printed2 > 0 and printed2[1]:find("runtime error", 1, true) ~= nil)
end

-- ---- evaluate: cache hit does not recompile -------------------------------
do
    local ctx = { f = { n = 1 }, tf = {}, sf = {}, mp = {},
                  current_scene = "c.ks", token_index = 1 }
    local realLoad = load
    local compiles = 0
    load = function(...) compiles = compiles + 1 return realLoad(...) end
    expr.evaluate(ctx, "f.n > 0")
    expr.evaluate(ctx, "f.n > 0")
    expr.evaluate(ctx, "f.n > 0")
    load = realLoad
    check("cached expression compiled once", compiles == 1)
end

-- ---- translate: stable across calls (pure function) ----------------------
check("translate idempotent", expr.translate(expr.translate("a && b"))
    == "a and b")

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("EXPR LANG TESTS DONE")
