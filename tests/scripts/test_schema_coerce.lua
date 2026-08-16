-- test_schema_coerce.lua — Deep boundary tests for Schema.coerce (the
-- editor LSP / runtime authority for command-param enforcement).
--
-- Coverage areas (task spec): type coercion, defaults, enum/clamp limits,
-- positional, special values, and coerce idempotency. Behaviors here LOCK
-- the current implementation; deviations that look like bugs are recorded
-- in the [FINDINGS] report (this file is the executable witness for each).
--
-- Suite context: runs in the shared test VM (see run_lua_tests.lua), so the
-- schema singleton carries contracts registered by earlier tests. Every
-- contract this file defines uses the unique _cb_ prefix to avoid collisions.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local check = function(name, cond, detail)
    if cond then print("  [PASS] " .. name) passed = (passed or 0) + 1
    else print("  [FAIL] " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
        failed = (failed or 0) + 1 end
end
local Schema = require("kag.schema")
pcall(require, "kag.commands.math")      -- [add]/[dec]/... positional contracts
pcall(require, "kag.commands.video")     -- [video] file positional
pcall(require, "kag.commands.system")    -- [set]/[inc]/[clamp] positional

-- ---------------------------------------------------------------------------
-- 1. TYPE COERCION — number / boolean / string / file deep boundaries
-- ---------------------------------------------------------------------------
Schema.define("_cb_num", {
    v   = { type = "number" },
    min0 = { type = "number", min = 0, max = 10 },
})
-- number accepted forms
local ok, e
ok, e = pcall(function() return Schema.coerce("_cb_num", { v = "5" }, {}) end)
check("number '5' accepted", ok and e.v == 5)
ok, e = pcall(function() return Schema.coerce("_cb_num", { v = "5.5" }, {}) end)
check("number '5.5' accepted", ok and e.v == 5.5)
ok, e = pcall(function() return Schema.coerce("_cb_num", { v = "-0.5" }, {}) end)
check("number '-0.5' accepted", ok and e.v == -0.5)
ok, e = pcall(function() return Schema.coerce("_cb_num", { v = " 3 " }, {}) end)
check("number surrounding spaces trimmed", ok and e.v == 3)
ok, e = pcall(function() return Schema.coerce("_cb_num", { v = "5." }, {}) end)
check("number trailing-dot '5.' -> 5.0", ok and e.v == 5.0)
ok, e = pcall(function() return Schema.coerce("_cb_num", { v = "03" }, {}) end)
check("number leading-zero '03' -> 3", ok and e.v == 3)
ok, e = pcall(function() return Schema.coerce("_cb_num", { v = 42 }, {}) end)
check("number raw number passes", ok and e.v == 42)
-- number rejected forms
check("number 'abc' rejected",
      not pcall(Schema.coerce, "_cb_num", { v = "abc" }, {}))
check("number '1e3' (scientific) rejected",
      not pcall(Schema.coerce, "_cb_num", { v = "1e3" }, {}))
check("number '0x10' (hex) rejected",
      not pcall(Schema.coerce, "_cb_num", { v = "0x10" }, {}))
check("number '+5' rejected (no plus sign) [FINDING]",
      not pcall(Schema.coerce, "_cb_num", { v = "+5" }, {}))
check("number raw boolean rejected",
      not pcall(Schema.coerce, "_cb_num", { v = true }, {}))

-- boolean
Schema.define("_cb_bool", { b = { type = "boolean" } })
check("boolean 'true' -> true",
      (Schema.coerce("_cb_bool", { b = "true" }, {})).b == true)
check("boolean '1' -> true",
      (Schema.coerce("_cb_bool", { b = "1" }, {})).b == true)
check("boolean 'yes' -> true",
      (Schema.coerce("_cb_bool", { b = "yes" }, {})).b == true)
check("boolean 'false' -> false",
      (Schema.coerce("_cb_bool", { b = "false" }, {})).b == false)
check("boolean '0' -> false",
      (Schema.coerce("_cb_bool", { b = "0" }, {})).b == false)
check("boolean 'no' -> false",
      (Schema.coerce("_cb_bool", { b = "no" }, {})).b == false)
check("boolean 'YES' -> true (case-insensitive)",
      (Schema.coerce("_cb_bool", { b = "YES" }, {})).b == true)
check("boolean 'No' -> false (case-insensitive)",
      (Schema.coerce("_cb_bool", { b = "No" }, {})).b == false)
check("boolean raw true passes",
      (Schema.coerce("_cb_bool", { b = true }, {})).b == true)
check("boolean raw number 1 rejected [FINDING]",
      not pcall(Schema.coerce, "_cb_bool", { b = 1 }, {}))
check("boolean ' 1 ' (spaced) rejected [FINDING]",
      not pcall(Schema.coerce, "_cb_bool", { b = " 1 " }, {}))
check("boolean 'bogus' rejected",
      not pcall(Schema.coerce, "_cb_bool", { b = "bogus" }, {}))

-- string: tostring of number/boolean/table? (table -> error-free tostring? tostring(table)=addr; only scalars here)
Schema.define("_cb_str", { s = { type = "string" } })
check("string number 123 -> '123'",
      (Schema.coerce("_cb_str", { s = 123 }, {})).s == "123")
check("string boolean true -> 'true'",
      (Schema.coerce("_cb_str", { s = true }, {})).s == "true")
check("string float 5.5 -> '5.5'",
      (Schema.coerce("_cb_str", { s = 5.5 }, {})).s == "5.5")
check("string plain passthrough",
      (Schema.coerce("_cb_str", { s = "hello" }, {})).s == "hello")

-- file type
Schema.define("_cb_file", { f = { type = "file" } })
check("file valid relative ok",
      (function() local p = Schema.coerce("_cb_file", { f = "assets/bg/x.png" }, {}); return p.f == "assets/bg/x.png" end)())
check("file empty dropped as absent [FINDING: dead empty-check]",
      (Schema.coerce("_cb_file", { f = "" }, {})).f == nil)
check("file traversal '..' rejected",
      not pcall(Schema.coerce, "_cb_file", { f = "../evil.png" }, {}))
check("file 'a../..x' rejected (contains ..)",
      not pcall(Schema.coerce, "_cb_file", { f = "a../..x" }, {}))
check("file backslash rejected",
      not pcall(Schema.coerce, "_cb_file", { f = "dir\\evil" }, {}))
check("file absolute '/' rejected",
      not pcall(Schema.coerce, "_cb_file", { f = "/etc/passwd" }, {}))
check("file './a' accepted (dot-slash is not traversal)",
      (function() local p = Schema.coerce("_cb_file", { f = "./a" }, {}); return p.f == "./a" end)())
check("file 'a./b' accepted (dotted basename ok)",
      (function() local p = Schema.coerce("_cb_file", { f = "a./b" }, {}); return p.f == "a./b" end)())
-- resolver presence check
check("file missing via resolver rejected",
      not pcall(Schema.coerce, "_cb_file", { f = "x.png" },
          { resolve_file = function() return nil end }))
check("file found via resolver ok",
      (function() local p = Schema.coerce("_cb_file", { f = "x.png" },
          { resolve_file = function() return "found" end }); return p.f == "x.png" end)())

-- ---------------------------------------------------------------------------
-- 2. DEFAULTS — missing / typing / required / empty-string / positional
-- ---------------------------------------------------------------------------
Schema.define("_cb_def", {
    n  = { type = "number", default = 50 },
    s  = { type = "string", default = "dflt" },
    b  = { type = "boolean", default = false },
    opt = { type = "string" },            -- no default, not required
})
local pd = Schema.coerce("_cb_def", {}, {})
check("default number applied", pd.n == 50)
check("default string applied", pd.s == "dflt")
check("default boolean applied", pd.b == false)
check("no-default non-required absent (nil)", pd.opt == nil)
-- required missing throws
Schema.define("_cb_req", { req = { type = "number", required = true } })
check("required missing throws",
      not pcall(Schema.coerce, "_cb_req", {}, {}))
check("required present ok",
      (pcall(Schema.coerce, "_cb_req", { req = "3" }, {})))
-- default type: emitted as-specified (not re-coerced) [FINDING]
Schema.define("_cb_deftype", { n = { type = "number", default = "oops" } })
local pdt = Schema.coerce("_cb_deftype", {}, {})
check("default wrong type emitted verbatim [FINDING]", pdt.n == "oops")
-- empty string counts as absent: default applied, required throws
local pe = Schema.coerce("_cb_def", { s = "" }, {})
check("empty string -> default applied", pe.s == "dflt")
check("required + empty string throws",
      not pcall(Schema.coerce, "_cb_req", { req = "" }, {}))
-- required + default both present: missing still throws (required wins)
Schema.define("_cb_reqdef", { x = { type = "number", default = 7, required = true } })
check("required beats default on missing",
      not pcall(Schema.coerce, "_cb_reqdef", {}, {}))

-- ---------------------------------------------------------------------------
-- 3. ENUM / LIMITS — values hit/reject, clamp boundaries, choices forms
-- ---------------------------------------------------------------------------
Schema.define("_cb_enum", { m = { type = "enum", values = { "on", "off", "toggle" } } })
check("enum value hit", (Schema.coerce("_cb_enum", { m = "on" }, {})).m == "on")
check("enum value miss rejected",
      not pcall(Schema.coerce, "_cb_enum", { m = "bogus" }, {}))
check("enum number not in values rejected",
      not pcall(Schema.coerce, "_cb_enum", { m = 123 }, {}))
-- enum stays a string
check("enum coerced value is string", type((Schema.coerce("_cb_enum", { m = "toggle" }, {})).m) == "string")
-- string-with-choices: MAP form validated, ARRAY form broken [FINDING]
Schema.define("_cb_cmap", { m = { type = "string", choices = { a = true, b = true } } })
check("string choices MAP valid", (Schema.coerce("_cb_cmap", { m = "a" }, {})).m == "a")
check("string choices MAP invalid rejected",
      not pcall(Schema.coerce, "_cb_cmap", { m = "z" }, {}))
Schema.define("_cb_carr", { m = { type = "string", choices = { "a", "b" } } })
check("string choices ARRAY rejects all [FINDING]",
      not pcall(Schema.coerce, "_cb_carr", { m = "a" }, {}))
-- clamp boundaries
check("clamp ==min unclamped", (Schema.coerce("_cb_num", { min0 = "0" }, {})).min0 == 0)
check("clamp ==max unclamped", (Schema.coerce("_cb_num", { min0 = "10" }, {})).min0 == 10)
check("clamp below->min", (Schema.coerce("_cb_num", { min0 = "-1" }, {})).min0 == 0)
check("clamp above->max", (Schema.coerce("_cb_num", { min0 = "11" }, {})).min0 == 10)
-- string length limits: coerce has NO maxlen field for string-typed params.
-- Verbatim long strings pass (documented absence, nothing to enforce).
Schema.define("_cb_long", { s = { type = "string" } })
local long = string.rep("x", 10000)
check("string length unlimited (no maxlen in coerce)", (Schema.coerce("_cb_long", { s = long }, {})).s == long)

-- ---------------------------------------------------------------------------
-- 4. POSITIONAL — positional_index interaction with named / default / required
-- ---------------------------------------------------------------------------
Schema.define("_cb_pos", {
    name = { type = "string", required = true, positional_index = 1 },
    vol  = { type = "number", positional_index = 2 },  -- no default
    opt  = { type = "string", default = "D", positional_index = 3 },
})
local pp = Schema.coerce("_cb_pos", { [1] = "Alice" }, {})
check("positional fills required (no error)", pp[1] == "Alice")
check("positional name not written to out.name", pp.name == nil)
local pp2 = Schema.coerce("_cb_pos", { [1] = "A", [3] = "slot3" }, {})
check("positional skips default for opt", pp2.opt == nil and pp2[3] == "slot3")
-- positional vs named conflict: named wins for out.name, positional still copied
local pp3 = Schema.coerce("_cb_pos", { name = "Named", [1] = "Alice" }, {})
check("named beats positional for out.name", pp3.name == "Named")
check("positional slot still present", pp3[1] == "Alice")
-- positional slot out of range: required slot unfilled -> error; non-required -> absent
check("positional out-of-range + required throws",
      not pcall(Schema.coerce, "_cb_pos", { [9] = "x" }, {}))
local pp4 = Schema.coerce("_cb_pos", { [1] = "A", [2] = "30", [3] = "c", [4] = "extra" }, {})
check("optional positional slots absent in out", pp4.opt == nil and pp4.vol == nil)
-- positional numbers are NOT type-coerced (stay raw string at out[N]) [FINDING]
check("positional numeric value stays uncoerced string [FINDING]",
      tostring(Schema.coerce("_cb_pos", { [1] = "A", [2] = "30" }, {})[2]) == "30")
-- production: [add f.x 5] positional value not schema-coerced (value.type=number advisory)
local pm = Schema.coerce("add", { [1] = "f.x", [2] = "5" }, {})
check("math/add positional number stays string [FINDING]",
      type(pm[2]) == "string" and pm[2] == "5")

-- ---------------------------------------------------------------------------
-- 5. SPECIAL VALUES — empty vs nil vs "0", quoted equals, escapes, dotted keys
-- ---------------------------------------------------------------------------
Schema.define("_cb_sp", { n = { type = "number", default = 5 }, b = { type = "boolean" } })
check("nil -> default", (Schema.coerce("_cb_sp", {}, {})).n == 5)
check("'' -> default (number)", (Schema.coerce("_cb_sp", { n = "" }, {})).n == 5)
check("'0' -> numeric 0 (not absent)", (Schema.coerce("_cb_sp", { n = "0" }, {})).n == 0)
check("'0' boolean -> false", (Schema.coerce("_cb_sp", { b = "0" }, {})).b == false)
-- literal '0' string passthrough for string-typed
local pz = Schema.coerce("_cb_str", { s = "0" }, {})
check("string '0' stays '0'", pz.s == "0")
-- quoted equals preserved in string value
local pq = Schema.coerce("_cb_str", { s = 'a="b" c' }, {})
check("quoted equals preserved", pq.s == 'a="b" c')
-- backslash escape preserved in string
local pb = Schema.coerce("_cb_str", { s = "a\nb" }, {})
check("backslash escape preserved", pb.s == "a\nb")
-- dotted declared key is a literal key (round 50 parser splits dotted keys
-- upstream; coerce treats a dotted spec name literally)
Schema.define("_cb_dot", { ["f.hp"] = { type = "number" } })
local pdt2 = Schema.coerce("_cb_dot", { ["f.hp"] = "30" }, {})
check("dotted declared key coerced literally", pdt2["f.hp"] == 30)
local pdu = Schema.coerce("_cb_str", { ["f.hp"] = "abc" }, {})
check("dotted undeclared key passes through", pdu["f.hp"] == "abc")

-- ---------------------------------------------------------------------------
-- 6. COERCE IDEMPOTENCY — coerce(coerce(x)) == coerce(x)
-- ---------------------------------------------------------------------------
Schema.define("_cb_idem", {
    n  = { type = "number", min = 0, max = 10 },
    s  = { type = "string" },
    b  = { type = "boolean" },
    l  = { type = "list", item_type = "number" },
    en = { type = "enum", values = { "on", "off" } },
    d  = { type = "number", default = 9 },
})
local x1 = Schema.coerce("_cb_idem", { n = "7", s = 123, b = "1", l = "1,2,3", en = "on" }, {})
local x2 = Schema.coerce("_cb_idem", x1, {})
check("idempotent number clamped", x1.n == x2.n and x2.n == 7)
check("idempotent string", x1.s == x2.s and x2.s == "123")
check("idempotent boolean", x1.b == x2.b and x2.b == true)
check("idempotent list (array equal)", x1.l[1] == x2.l[1] and x1.l[2] == x2.l[2] and x1.l[3] == x2.l[3])
check("idempotent enum", x1.en == x2.en and x2.en == "on")
check("idempotent default-filled passes", (Schema.coerce("_cb_idem", x2, {})).d == 9)
check("idempotent clamp preserved (no drift)", x2.n == 7 and x2.n == (Schema.coerce("_cb_idem", x2, {})).n)

if failed and failed > 0 then
    print(string.format("SCHEMA COERCE BOUNDARY: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("SCHEMA COERCE BOUNDARY TESTS DONE (%d passed)", passed))
