-- test_lsp.lua — KAG Neo-Genesis language service tests (Battle 2).
-- completion / hover / diagnostics driven by the 78 command contracts.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
        failed = failed + 1 end
end

local lsp = require("kag.lsp")
local schema = require("kag.schema")

-- ---------------------------------------------------------------------------
-- 1. completion: command names after "["
-- ---------------------------------------------------------------------------
local c1 = lsp.completion("[ch")
check("completion [ch returns items", #c1 > 0)
local hasCh = false
for _, it in ipairs(c1) do
    if it.label == "ch" then hasCh = true break end
end
check("completion includes ch", hasCh)
check("completion items have kind+insertText",
      c1[1] and c1[1].kind and c1[1].insertText)

-- empty prefix completes everything
local c0 = lsp.completion("[")
check("completion [ returns many", #c0 > 30)

-- ---------------------------------------------------------------------------
-- 2. completion: params after "[cmd "
-- ---------------------------------------------------------------------------
local cp = lsp.completion("[ch ")
check("completion [ch space returns params", #cp > 0)
local hasText = false
for _, it in ipairs(cp) do
    if it.label == "text" then
        hasText = true
        check("ch text param has type detail", it.detail:find("string", 1, true) ~= nil)
    end
end
check("completion includes text param", hasText)

-- ---------------------------------------------------------------------------
-- 3. hover: command contract + param details
-- ---------------------------------------------------------------------------
local h = lsp.hover("ch")
check("hover ch has title", h and h.title == "[ch]")
check("hover ch has desc", h and h.text:find("KAG3%-compatible ch", 1) ~= nil
      or (h and #h.text > 0))
local h2 = lsp.hover("pt", "speed")
check("hover pt.speed has type+default+range",
      h2 and h2.text:find("type=number", 1, true)
      and h2.text:find("default=50", 1, true)
      and h2.text:find("min=8", 1, true) and h2.text:find("max=5000", 1, true))
check("hover unknown cmd nil", lsp.hover("definitely_not_a_cmd") == nil)

-- ---------------------------------------------------------------------------
-- 4. diagnostics: unknown commands, required params, require-any
-- ---------------------------------------------------------------------------
local d1 = lsp.diagnostics('[ch text="a"]\n[unknown_cmd_xyz]\n')
check("diagnostics flags unknown command", #d1 == 1
      and d1[1].message:find("unknown_cmd_xyz", 1, true) ~= nil
      and d1[1].severity == 2)

local d2 = lsp.diagnostics('[playbgm]\n[playbgm file="x.ogg"]\n')
check("diagnostics require-any on bare playbgm", #d2 == 1
      and d2[1].message:find("file", 1, true) ~= nil
      and d2[1].severity == 1)
check("diagnostics ok with file present", d2[1].line == 1)

local d3 = lsp.diagnostics("[ch]\n")  -- ch has no required params
check("diagnostics clean for [ch]", #d3 == 0)

-- line numbers accurate
local d4 = lsp.diagnostics("line1\nline2\n[bad_cmd_here]\n")
check("diagnostics line number", #d4 == 1 and d4[1].line == 3)

-- ---------------------------------------------------------------------------
-- 5. json output (what the IDE receives via /api/eval)
-- ---------------------------------------------------------------------------
local j1 = lsp.json("hover", "ch")
check("hover json valid", j1:sub(1, 1) == "[" and j1:find('"title"', 1, true) ~= nil)
local j2 = lsp.json("diagnostics", '[playbgm]\n')
check("diagnostics json has severity+line",
      j2:find('"severity":1', 1, true) ~= nil and j2:find('"line":1', 1, true) ~= nil)

-- ---------------------------------------------------------------------------
-- 6. ks_check parity: expression compile + truncation (Battle 2c)
-- ---------------------------------------------------------------------------
local de = lsp.diagnostics('[if exp="f.x > &&"]\n')
check("expr compile error flagged", #de == 1
      and de[1].message:find("does not compile", 1, true) ~= nil
      and de[1].severity == 1)
local dg = lsp.diagnostics('[if exp="f.x > 5 && f.y != 0"]\n')
check("valid expr clean", #dg == 0)
local dt = lsp.diagnostics('[ch text="a"]\n[bg storage="x"\n')
check("truncation flagged", #dt == 1
      and dt[1].message:find("stopped before end", 1, true) ~= nil)
local dc = lsp.diagnostics('[ch text="a"]\n; done\n')
check("comment tail no false positive", #dc == 0)
local dboth = lsp.diagnostics('[if exp="bad &&"]\n[bg storage="x"\n')
check("expr+truncation both flagged", #dboth == 2)

-- ---------------------------------------------------------------------------
-- Navigation: definition / references
-- ---------------------------------------------------------------------------
local navText = "*start\n[ch text=\"hi\"]\n[jump target=*start]\n"
    .. "*ending\n[call *ending]\n[link *start]back[/link]\n"
-- [jump target=*start] is line 3, starts at col 8 (target value col 18)
local d1 = lsp.definition(navText, 3, 18)
check("definition: jump target -> label line", d1 ~= nil and d1.name == "start"
      and d1.line == 1)
local d2 = lsp.definition(navText, 1, 1)
check("definition: on label itself", d2 ~= nil and d2.name == "start"
      and d2.line == 1)
local d3 = lsp.definition(navText, 5, 11)
check("definition: call bare target", d3 ~= nil and d3.name == "ending"
      and d3.line == 4)
local d4 = lsp.definition(navText, 2, 5)
check("definition: non-nav token -> nil", d4 == nil)
local d5 = lsp.definition("[jump target=*missing]\n", 1, 18)
check("definition: missing label -> name only", d5 ~= nil
      and d5.name == "missing" and d5.line == nil)
local d6 = lsp.definition("", 1, 1)
check("definition: empty scene -> nil", d6 == nil)

local r1 = lsp.references(navText, "start")
check("references: label + 2 nav sites", #r1 == 3)
local kinds = {}
for _, r in ipairs(r1) do kinds[r.kind] = (kinds[r.kind] or 0) + 1 end
check("references: one definition two references",
      kinds.definition == 1 and kinds.reference == 2)
local r2 = lsp.references(navText, "nope")
check("references: unknown label -> empty", #r2 == 0)
local r3 = lsp.references(navText, "")
check("references: empty name -> empty", #r3 == 0)

-- json bridge shapes
local jd = lsp.json("definition", navText, 3, 18)
check("json definition shape", jd:find('"name":"start"', 1, true) ~= nil
      and jd:find('"line":1', 1, true) ~= nil)
local jr = lsp.json("references", navText, "start")
check("json references shape", jr:find('"kind":"reference"', 1, true) ~= nil)
local jm = lsp.json("definition", "[jump target=*missing]\n", 1, 18)
check("json missing label omits line", jm:find('"name":"missing"', 1, true) ~= nil
      and jm:find('"line"', 1, true) == nil)


-- ---------------------------------------------------------------------------
-- 7. ${expr} interpolation diagnostics (Battle 2e): interpolatable text
--    params are run through Schema.checkInterp (the SAME compile path as
--    runtime) so a bad interpolation fails in the editor.
-- ---------------------------------------------------------------------------
local di1 = lsp.diagnostics('[ch text="hp ${bad &&}"]\n')
check("interp compile error flagged", #di1 == 1
      and di1[1].message:find("interpolation", 1, true) ~= nil
      and di1[1].severity == 1)
local di2 = lsp.diagnostics('[ch text="hp ${f.hp}"]\n')
check("interp valid clean", #di2 == 0)
local di3 = lsp.diagnostics('[ch text="${ {a=1}.a }"]\n')
check("interp nested constructor clean", #di3 == 0)
local di4 = lsp.diagnostics('[ch text="hp ${oops"]\n')
check("interp unterminated warning", #di4 == 1
      and di4[1].message:find("unterminated", 1, true) ~= nil
      and di4[1].severity == 2)
local di5 = lsp.diagnostics('[ch text="${ [[a]] } ok"]\n')
check("interp long bracket clean", #di5 == 0)
local di6 = lsp.diagnostics('[ch text="${f.x} ok ${bad +} end"]\n')
check("interp multi-span flags bad one", #di6 == 1
      and di6[1].message:find("bad +", 1, true) ~= nil)

-- ---------------------------------------------------------------------------
-- 8. expression context completion: variable table prefixes
-- ---------------------------------------------------------------------------
local vp1 = lsp.completion('[if exp="f', 11)
local hasDot = false
for _, it in ipairs(vp1) do if it.label == "f." then hasDot = true break end end
check("completion exp value offers f.", hasDot)
local vp2 = lsp.completion('[ch text="hp ${', 16)
local hasTf = false
for _, it in ipairs(vp2) do if it.label == "tf." then hasTf = true break end end
check("completion ${ offers tf.", hasTf)
local vp3 = lsp.completion('[ch ', 4)
local hasF = false
for _, it in ipairs(vp3) do if it.label == "f." then hasF = true break end end
check("completion non-expr omits variable prefixes", not hasF)

-- ---------------------------------------------------------------------------
-- 9. hover: expression-language cheat-sheet
-- ---------------------------------------------------------------------------
local hs1 = lsp.hover("if", "exp")
check("hover if.exp has cheat-sheet", hs1 ~= nil
      and hs1.text:find("expression language", 1, true) ~= nil)
local hs2 = lsp.hover("eval")
check("hover eval has cheat-sheet", hs2 ~= nil
      and hs2.text:find("expression language", 1, true) ~= nil)
local hs3 = lsp.hover("ch")
check("hover ch no cheat-sheet", hs3 ~= nil
      and hs3.text:find("expression language", 1, true) == nil)

-- ---------------------------------------------------------------------------
-- 10. unknown-param diagnostics (round-73): misspelled/undeclared named
--     params on a known command are flagged as warnings.
-- ---------------------------------------------------------------------------
local du1 = lsp.diagnostics('[ch texte="x"]\n')
check("unknown param flagged for [ch texte]", #du1 == 1
      and du1[1].message:find("unknown param 'texte'", 1, true) ~= nil
      and du1[1].message:find(" for [ch]", 1, true) ~= nil
      and du1[1].severity == 2
      and du1[1].line == 1)
local du2 = lsp.diagnostics('[ch text="x"]\n')
check("known param text clean", #du2 == 0)
local du3 = lsp.diagnostics('[if exp="f.x"]\n')
check("flow command [if] no false positive", #du3 == 0)
-- Positional args (numeric slots) must NOT be flagged as unknown
-- params. (The pre-existing required-param check still reports missing
-- required params here because it does not map positional slots to named
-- params -- that is out of scope for this round; only assert no
-- "unknown param" warning.)
local du4 = lsp.diagnostics('[set f.hp 30]\n')
local du4unknown = false
for _, it in ipairs(du4) do
    if it.message:find("unknown param", 1, true) then du4unknown = true break end
end
check("positional params [set f.hp 30] no unknown-param", not du4unknown)
local du5 = lsp.diagnostics('[notify "saved"]\n')
local du5unknown = false
for _, it in ipairs(du5) do
    if it.message:find("unknown param", 1, true) then du5unknown = true break end
end
check("positional param [notify] no unknown-param", not du5unknown)
local du6 = lsp.diagnostics('[playbgm file="x.ogg"]\n')
check("storage/file alias no false positive", #du6 == 0)
local du7 = lsp.diagnostics('[ch text="a" typos=1 tele="b"]\n')
check("multiple unknown params each flagged", #du7 == 2
      and du7[1].message:find("tele", 1, true) ~= nil
      and du7[2].message:find("typos", 1, true) ~= nil)
local du8 = lsp.diagnostics('[ch text="a"]\n[ch text="b"]\n')
check("valid params across lines clean", #du8 == 0)

-- Exit gate.
if failed > 0 then
    print(string.format("LSP TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("LSP TESTS DONE (%d passed)", passed))
