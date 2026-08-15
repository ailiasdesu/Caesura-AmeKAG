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

-- ---- translate: parentheses scope operators --------------------------------
check("parens preserved around translated ops",
    expr.translate("(a && b) == 1") == "(a and b) == 1")
check("parens group mixed chain",
    expr.translate("a && (b || c) && d") == "a and (b or c) and d")

-- ---- translate: table keys and string literals stay untouched --------------
check("operators inside table-key quotes kept",
    expr.translate('f["a&&b"] == 1') == 'f["a&&b"] == 1')
check("single-quote key with || kept",
    expr.translate("f['x||y'] != 2") == "f['x||y'] ~= 2")
check("escaped quote + && inside string",
    expr.translate([[s == "a \" && b"]]) == [[s == "a \" && b"]])

-- ---- translate: numeric and unary edges ------------------------------------
check("negative literal preserved",
    expr.translate("-5 < f.hp") == "-5 < f.hp")
check("float and scientific literals",
    expr.translate("f.hp >= 3.14 && f.mp <= 1e3")
        == "f.hp >= 3.14 and f.mp <= 1e3")
check("double negation !!, translated",
    expr.translate("!a && !!b") == "not a and not not b")
check("chained != compiles (Lua semantics)",
    expr.translate("a != b != c") == "a ~= b ~= c")
check("range idiom preserved",
    expr.translate("1 < f.hp && f.hp < 100")
        == "1 < f.hp and f.hp < 100")

-- ---- translate: ternary with parenthesized branches ------------------------
check("ternary paren branches",
    expr.translate("a ? (b && c) : (d || e)")
        == "((a) and ((b and c)) or ((d or e)))")

-- ---- evaluate: nil and short-circuit edges --------------------------------
do
    local ctx = { f = { hp = 30, mp = 5 }, tf = { flag = true },
                  sf = {}, mp = {}, lf = {} }
    local ok1, v1 = expr.evaluate(ctx, "f.missing == nil")
    check("missing field compares to nil", ok1 and v1 == true)

    -- Short-circuit must prevent the runtime error on the right side.
    local ok2, v2 = expr.evaluate(ctx, "false && tf.nil_table.field == 1")
    check("&& short-circuits nil deref", ok2 and v2 == false)
    local ok3, v3 = expr.evaluate(ctx, "true || tf.nil_table.field == 1")
    check("|| short-circuits nil deref", ok3 and v3 == true)

    -- Strict typing: string vs number does NOT coerce (Lua semantics).
    local ok4, v4 = expr.evaluate(ctx, "'30' == f.hp")
    check("string != number (strict Lua compare)", ok4 and v4 == false)
end

-- ---- evaluate: arithmetic and comparison edges -----------------------------
do
    local ctx = { f = { hp = 30, mp = 5 }, sf = {}, tf = {}, mp = {}, lf = {} }
    local ok1, v1 = expr.evaluate(ctx, "f.hp % 7 == 2")
    check("modulo evaluates", ok1 and v1 == true)
    local ok2, v2 = expr.evaluate(ctx, "f.hp / 4 > 7")
    check("division evaluates", ok2 and v2 == true)
    local ok3, v3 = expr.evaluate(ctx, "f.hp >= 3.14")
    check("float comparison evaluates", ok3 and v3 == true)
    local ok4, v4 = expr.evaluate(ctx, "f.hp != 0 && f.hp != 31")
    check("range exclusion evaluates", ok4 and v4 == true)
    local ok5, v5 = expr.evaluate(ctx, "f.hp > 20 and f.hp < 40")
    check("Lua-native and works too", ok5 and v5 == true)
end

-- ---- null-coalescing ?? (round 53: doc mentioned it, never translated) ---
do
    local ctx = { f = { hp = 0, name = "Aoi" }, sf = {}, tf = {}, mp = {}, lf = {} }
    local ok1, v1 = expr.evaluate(ctx, "f.missing ?? 42")
    check("?? falls back on missing var", ok1 and v1 == 42)
    local ok2, v2 = expr.evaluate(ctx, "f.hp ?? 99")
    check("?? keeps 0 (truthy in Lua)", ok2 and v2 == 0)
    local ok3, v3 = expr.evaluate(ctx, "f.name ?? 'anon'")
    check("?? keeps present value", ok3 and v3 == "Aoi")
    local ok4, v4 = expr.evaluate(ctx, "(f.a ?? 1) + (f.b ?? 2)")
    check("?? nests in arithmetic", ok4 and v4 == 3)
    local tr = expr.translate("'x ?? y'")
    check("?? inside string literal untouched", tr == "'x ?? y'")
end

-- ---- ternary inside [...] index brackets (round 61) ----------------------
do
    local ctx = { f = { arr = { 10, 20 }, flag = true, b = true, t = 0 },
        sf = {}, tf = {}, mp = {}, lf = {} }
    local ok1, v1 = expr.evaluate(ctx, "f.arr[ f.flag ? 1 : 2 ]")
    check("ternary in index picks then", ok1 and v1 == 10)
    ctx.f.flag = false
    local ok2, v2 = expr.evaluate(ctx, "f.arr[ f.flag ? 1 : 2 ]")
    check("ternary in index picks else", ok2 and v2 == 20)
    local ok3, v3 = expr.evaluate(ctx, "f.arr[ f.flag ? 1 : 2 ] + 5")
    check("ternary in index composes with arithmetic", ok3 and v3 == 25)
    ctx.f.flag = true
    local ok4, v4 = expr.evaluate(ctx, "f.arr[ f.b ? 1 : 2 ] ? 10 : 20")
    check("outer ternary around indexed ternary", ok4 and v4 == 10)
    ctx.f.flag = false
    local ok5, v5 = expr.evaluate(ctx, "f.arr[ f.flag ? 1 : 2 ]")
    check("ternary in index keeps indexing", ok5 and v5 == 20)
    local tr = expr.translate("f.arr[ f.flag ? 1 : 2 ]")
    check("translate leaves no '?' for index ternary",
        tr:find("?", 1, true) == nil, tr)
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("EXPR LANG TESTS DONE")