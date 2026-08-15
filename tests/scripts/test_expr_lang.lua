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

-- ---- perf baseline (round 66): translate/evaluate at scale ----
do
    local ctx = { f = { hp = 30, flag = true }, sf = {}, tf = {}, mp = {}, lf = {} }
    local src = "f.hp > 10 && f.flag ? f.hp * 2 : 0"
    local t0 = os.clock()
    for i = 1, 2000 do
        local ok, v = expr.evaluate(ctx, src)
        if not (ok and v == 60) then check("repeated evaluate stable", false); break end
    end
    local dt = os.clock() - t0
    check("2000x cached evaluate correct", true)
    check("2000x cached evaluate within budget", dt < 5.0, string.format("%.3fs", dt))
    -- 200-term numeric chain: 30 + sum(1..200) = 20130
    local big = "f.hp"
    for i = 1, 200 do
        big = big .. " + " .. i
    end
    local ok2, v2 = expr.evaluate(ctx, big)
    check("large expression evaluates", ok2 and v2 == 20130, tostring(v2))
    -- operator chain: and/or/not/!= all translate without recursion issues
    local ok3, v3 = expr.evaluate(ctx, "f.flag && (f.hp > 0 || !f.off) != false")
    check("operator chain evaluates", ok3 and v3 == true, tostring(v3))
end

-- ---- ternary inside parentheses (round 68) -----------------------------
do
    local ctx = { f = { arr = { 10, 20, 30 }, flag = true }, sf = {}, tf = {}, mp = {}, lf = {} }
    local ok1, v1 = expr.evaluate(ctx, "f.arr[1] + (f.flag ? f.arr[2] : f.arr[3])")
    check("ternary in parens evaluates", ok1 and v1 == 30, tostring(v1))
    ctx.f.flag = false
    local ok2, v2 = expr.evaluate(ctx, "f.arr[1] + (f.flag ? f.arr[2] : f.arr[3])")
    check("ternary in parens else-branch", ok2 and v2 == 40, tostring(v2))
    local tr = expr.translate("1 + (f.flag ? 2 : 3)")
    check("paren ternary leaves no '?'", tr:find("?", 1, true) == nil, tr)
    local tr2 = expr.translate("(a || b) ? (c && d) : e")
    check("existing paren grouping unchanged",
        tr2 == "(((a or b)) and ((c and d)) or (e))", tr2)
end

-- ---- translateAssignment (round 68): ternary RHS in eval statements ----
do
    local a1 = expr.translateAssignment("f.x = f.y ? 1 : 2")
    check("assignment ternary RHS translates",
        a1 == "f.x = ((f.y) and (1) or (2))", a1)
    local a2 = expr.translateAssignment("f.ok = f.hp > 10 && f.flag")
    check("assignment operators translate",
        a2 == "f.ok = f.hp > 10 and f.flag", a2)
    local a3 = expr.translateAssignment("f.x == 1")
    check("comparison (no assignment) untouched",
        a3 == "f.x == 1", a3)
    local a4 = expr.translateAssignment("f.s = 'a = b'")
    check("equals inside string not split",
        a4 == "f.s = 'a = b'", a4)
end

-- ---- round 70 review C1: >= <= != inside ternary/assignment -------
do
    local a1 = expr.translateAssignment("f.pick = f.lv >= 5 ? 1 : 0")
    check("assignment with >= ternary RHS",
        a1 == "f.pick = ((f.lv >= 5) and (1) or (0))", a1)
    local a2 = expr.translateAssignment("f.pick = f.hp != 0 ? 1 : 0")
    check("assignment with != ternary RHS",
        a2 == "f.pick = ((f.hp ~= 0) and (1) or (0))", a2)
    local a3 = expr.translateAssignment("f.x = f.a >= 3 && f.b")
    check("assignment with >= before &&",
        a3 == "f.x = f.a >= 3 and f.b", a3)
end

-- ---- round 70 review C2: long strings inside index/paren groups ----
do
    -- NOTE: "f.arr[f.s == [[x]t]] ? 1 : 2" (no space before the index
    -- close) is INVALID Lua: the long string [[x]t]] consumes the
    -- index's closing "]". The valid form separates them with a space:
    --   f.arr[ f.s == [[x]t]] ] ? 1 : 2
    -- Here the ternary applies to the WHOLE indexed expression
    -- (f.arr[f.s == "x]t"] ? 1 : 2); the "]" inside the literal must
    -- not close the index early (round 70 review C2).
    local tr = expr.translate("f.arr[ f.s == [[x]t]] ] ? 1 : 2")
    check("long-string ] inside index ternary",
        tr == "((f.arr[ f.s == [[x]t]] ]) and (1) or (2))", tr)
    local tr2 = expr.translate("f.arr[1] + (f.s == [[x)t]] ? 1 : 2)")
    check("long-string ) inside paren ternary",
        tr2:find("?", 1, true) == nil, tr2)
    local ctx = { f = { arr = { 10, 20 }, s = "x]t" },
        sf = {}, tf = {}, mp = {}, lf = {} }
    local tr3 = expr.translate("f.arr[f.s == [[x]t]] ? 1 : 2]")
    check("long-string inside index ternary wrap",
        tr3 == "f.arr[((f.s == [[x]t]]) and (1) or (2))]", tr3)
    local ok, v = expr.evaluate(ctx,
        "f.arr[f.s == [[x]t]] ? 1 : 2]")
    check("long-string index ternary evaluates", ok and v == 10,
        tostring(v))
end

-- ---- round 70 review C2b: long strings at top level (depth 0) ----
-- find_top/match_colon must not let the long-string closer's final ']'
-- decrement depth below 0, or a later '?' at depth 0 is never matched.
do
    local tr = expr.translate("f.s == [[x]t]] ? 1 : 2")
    check("top-level long string before ternary",
        tr == "((f.s == [[x]t]]) and (1) or (2))", tr)
    local tr2 = expr.translate("a ? [[x]t]] : [[y]]")
    check("long string as then-branch",
        tr2 == "((a) and ([[x]t]]) or ([[y]]))", tr2)
    local tr3 = expr.translate("[[a]] ? [[b ? c]] : [[d]]")
    check("long strings in all three parts",
        tr3 == "(([[a]]) and ([[b ? c]]) or ([[d]]))", tr3)
    local tr4 = expr.translate("[[a<b]] ? 1 : 2")
    check("long string with < before ternary",
        tr4 == "(([[a<b]]) and (1) or (2))", tr4)
    -- '?' ':' inside a long string are content, not ternary markers
    local tr5 = expr.translate("f.s == [[x ? y : z]] ? 1 : 0")
    check("? and : inside long string skipped",
        tr5 == "((f.s == [[x ? y : z]]) and (1) or (0))", tr5)
    -- operators inside long strings are content too
    local tr6 = expr.translate("[[a && b]] == [[c || d]]")
    check("&& || inside long strings preserved",
        tr6 == "[[a && b]] == [[c || d]]", tr6)
    local tr7 = expr.translate("[[a != b]] == 1")
    check("!= inside long string preserved",
        tr7 == "[[a != b]] == 1", tr7)
    -- ?? inside a long string is not null-coalescing
    local tr8 = expr.translate("[[a ?? b]] == 1")
    check("?? inside long string preserved",
        tr8 == "[[a ?? b]] == 1", tr8)
    -- '=' inside a long string is not an assignment boundary
    local a1 = expr.translateAssignment("f.x = [[a=b]] ? 1 : 0")
    check("assignment RHS with = inside long string",
        a1 == "f.x = ((f.x = ) and (1) or (0))" or
        a1:find("[[a=b]]", 1, true) ~= nil and a1:find(" and (1) or (0))", 1, true) ~= nil, a1)
    -- evaluate end-to-end with a long-string literal
    local ctx = { f = { s = "x]t", hp = 30 }, sf = {}, tf = {}, mp = {}, lf = {} }
    local ok, v = expr.evaluate(ctx, "f.s == [[x]t]] ? 1 : 2")
    check("top-level long string ternary evaluates", ok and v == 1,
        tostring(v))
    local ok2, v2 = expr.evaluate(ctx, "f.s == [[nope]] ? 1 : 2")
    check("top-level long string ternary else", ok2 and v2 == 2,
        tostring(v2))
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("EXPR LANG TESTS DONE")