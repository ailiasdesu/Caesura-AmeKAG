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


-- ---------------------------------------------------------------------------
-- 11. definition/references across nav command forms (round 74 / stage D):
--     named quoted target="*label" for [jump]/[call]/[link], cursor placed
--     INSIDE the quoted value (not just on the command), plus aggregation
--     of multiple nav sites to one label.
-- ---------------------------------------------------------------------------
-- Quoted named forms: each line targets *scene with the cursor inside the
-- quoted value (col 17 = the 'e' inside "*scene"). [link]...[/link] kept
-- terminal (tokenizer truncates content after a mid-stream [/link]; the
-- link command token itself always parses).
local navQuoted = "*scene\n"
    .. "[jump target=\"*scene\"]\n"
    .. "[call target=\"*scene\"]\n"
    .. "[jump target=\"*scene\"]\n"
    .. "[link target=\"*scene\"]go[/link]\n"
local dqm = lsp.definition(navQuoted, 2, 17)
check("def: jump target=\"quoted\" value cursor -> scene", dqm ~= nil
      and dqm.name == "scene" and dqm.line == 1)
local dqc = lsp.definition(navQuoted, 3, 17)
check("def: call target=\"quoted\" value cursor -> scene", dqc ~= nil
      and dqc.name == "scene" and dqc.line == 1)
local dql = lsp.definition(navQuoted, 5, 17)
check("def: link target=\"quoted\" value cursor -> scene", dql ~= nil
      and dql.name == "scene" and dql.line == 1)
-- Cursor strictly inside the value of a SECOND jump on the same label.
local dq2 = lsp.definition(navQuoted, 4, 19)
check("def: 2nd jump quoted value cursor -> scene", dq2 ~= nil
      and dq2.name == "scene" and dq2.line == 1)
-- Aggregation: 4 nav sites + 1 definition for *scene.
local rq = lsp.references(navQuoted, "scene")
check("refs: quoted named 4 sites + def", #rq == 5)
local qk = { definition = 0, reference = 0 }
for _, x in ipairs(rq) do qk[x.kind] = (qk[x.kind] or 0) + 1 end
check("refs: 1 def + 4 references (jump/call/link)", qk.definition == 1
      and qk.reference == 4)
-- Positions: definition line 1, refs span lines 2..5.
local qlines = {}
for _, x in ipairs(rq) do qlines[#qlines + 1] = x.line end
table.sort(qlines)
check("refs: aggregated at every nav line", qlines[1] == 1
      and qlines[#qlines] == 5 and #qlines == 5)
-- Definition granularity is the whole command token, not param-value
-- precise: a cursor anywhere inside a nav command (including on the
-- param NAME, col 9 in "target=...") still resolves to its target. The
-- editor passes the true cursor column; token-span resolution is the
-- existing, intended behaviour (see original d1).
local dqX = lsp.definition(navQuoted, 2, 9)
check("def: cursor on param name still resolves via token span", dqX ~= nil
      and dqX.name == "scene" and dqX.line == 1)
-- Bare forms (no quotes) also aggregate for call/link (terminal link).
local navBare = "*end\n[jump *end]\n[call *end]\n[link *end]go[/link]\n"
local rb = lsp.references(navBare, "end")
check("refs: bare call+link aggregate", #rb == 4)
local dbj = lsp.definition(navBare, 2, 8)
check("def: bare jump value cursor -> end", dbj ~= nil and dbj.name == "end"
      and dbj.line == 1)
local dbc = lsp.definition(navBare, 3, 8)
check("def: bare call value cursor -> end", dbc ~= nil and dbc.name == "end"
      and dbc.line == 1)
local dbl = lsp.definition(navBare, 4, 8)
check("def: bare link value cursor -> end", dbl ~= nil and dbl.name == "end"
      and dbl.line == 1)
-- json bridge for quoted named + aggregation shapes.
local jqn = lsp.json("definition", navQuoted, 3, 17)
check("json: quoted call definition shape", jqn:find('"name":"scene"', 1, true) ~= nil
      and jqn:find('"line":1', 1, true) ~= nil)
local jqa = lsp.json("references", navQuoted, "scene")
check("json: aggregation serializes definition+references",
      jqa:find('"kind":"definition"', 1, true) ~= nil
      and jqa:find('"kind":"reference"', 1, true) ~= nil)

-- ---------------------------------------------------------------------------
-- 12. KAG3 alias param completion (round 75): [sel] shares the [button]
--     handler (TextCommands.sel = button) but registers no contract of its
--     own; the LSP must complete its params from the aliased command.
-- ---------------------------------------------------------------------------
do
    local cSel = lsp.completion('[sel ', nil)
    local params = {}
    for _, it in ipairs(cSel) do
        if it.kind == 5 then params[it.label] = true end
    end
    check("alias: [sel ] completes text", params.text == true)
    check("alias: [sel ] completes target", params.target == true)
    check("alias: [sel ] completes cond", params.cond == true)
    check("alias: [sel ] completes caption", params.caption == true)
    check("alias: [sel ] completes x (round-74 result capture)", params.x == true)
    -- prefix filtering still applies on the alias params
    local cSelX = lsp.completion('[sel x', nil)
    local hasX = false
    for _, it in ipairs(cSelX) do
        if it.label == 'x' then hasX = true end
    end
    check("alias: [sel x filters to x", hasX)
    -- non-alias commands are unaffected
    local cCh = lsp.completion('[ch ', nil)
    local hasSelOnly = false
    for _, it in ipairs(cCh) do
        if it.label == 'x' then hasSelOnly = true end
    end
    check("alias: [ch ] does not gain x", not hasSelOnly)
end

-- ---------------------------------------------------------------------------
-- 13. label rename (round 76): rename a *label across the scene -- update
--     the definition AND every nav reference ([jump]/[call]/[link]/[goto]/
--     [sel]/[select] via a named target="*name" or bare *name). Edits are
--     {kind, line, col, length, newText} with col/length on the NAME byte
--     (after the leading '*') so an editor replaces it in place. nil is
--     returned for a rejected (invalid) newName; {} when no defined label
--     resolves under the cursor.
-- ---------------------------------------------------------------------------
-- [link]...[/link] must stay LAST: the tokenizer truncates parsing
-- mid-stream after a [/link], so its tail [goto]/[sel] lines would be lost.
local renTxt = "*start\n"
    .. "[jump target=\"*start\"]\n"
    .. "[call *start]\n"
    .. "[goto target=*start]\n"
    .. "[sel target=\"*start\"]\n"
    .. "[link *start]go[/link]\n"
local ren = lsp.rename(renTxt, 1, 1, "scene")  -- cursor on the *start def
check("rename: def + 5 nav sites", ren ~= nil and #ren == 6)
local rnDef, rnRef = 0, 0
local defEdit = nil
for _, ed in ipairs(ren) do
    if ed.kind == "definition" then rnDef = rnDef + 1; defEdit = ed end
    if ed.kind == "reference" then rnRef = rnRef + 1 end
    check("rename: edit has newText+length", type(ed.newText) == "string"
          and type(ed.length) == "number")
end
check("rename: 1 definition + 5 references", rnDef == 1 and rnRef == 5)
check("rename: def edit at line 1 col 2 (after *)", defEdit ~= nil
      and defEdit.line == 1 and defEdit.col == 2
      and defEdit.newText == "scene" and defEdit.length == 5)
-- Applying the edits must rewrite every site in place.
local function apply_edits(text, edits)
    local ls = { 1 }
    for i = 1, #text do if text:byte(i) == 10 then ls[#ls + 1] = i + 1 end end
    table.sort(edits, function(a, b)
        if a.line == b.line then return a.col < b.col end
        return a.line < b.line end)
    for j = #edits, 1, -1 do
        local e = edits[j]
        local bs = (ls[e.line] or 1) + e.col - 1
        text = text:sub(1, bs - 1) .. e.newText .. text:sub(bs + e.length)
    end
    return text
end
local renamed = apply_edits(renTxt, ren)
local sceneCount = select(2, renamed:gsub("%*scene", ""))
check("rename: applied rewrites every site to *scene", sceneCount == 6
      and renamed:find("*start", 1, true) == nil)
check("rename: quoted jump rewritten", renamed:find('[jump target="*scene"]', 1, true) ~= nil)
check("rename: bare call rewritten", renamed:find("[call *scene]", 1, true) ~= nil)
check("rename: goto rewritten", renamed:find("[goto target=*scene]", 1, true) ~= nil)
check("rename: sel rewritten", renamed:find('[sel target="*scene"]', 1, true) ~= nil)
check("rename: bare link rewritten", renamed:find("[link *scene]go", 1, true) ~= nil)
-- Leading '*' on newName is stripped and accepted.
local renStar = lsp.rename(renTxt, 1, 1, "*scene")
check("rename: leading * stripped+accepted", renStar ~= nil and #renStar == 6)

-- Invalid newName is rejected (nil): spaces, empty, non-string.
check("rename: rejects spaced newName", lsp.rename(renTxt, 1, 1, "bad name") == nil)
check("rename: rejects empty newName", lsp.rename(renTxt, 1, 1, "") == nil)
check("rename: rejects non-string newName", lsp.rename(renTxt, 1, 1, 42) == nil)

-- Unknown / absent label -> no edits ({}, not rejected).
check("rename: missing label no edits",
      #lsp.rename("[jump target=*missing]\n", 1, 18, "x") == 0)
check("rename: cursor off any token no edits",
      #lsp.rename("[ch text=\"hi\"]\n", 1, 5, "x") == 0)

-- JSON shape roundtrip via lsp.json.
local jr = lsp.json("rename", renTxt, 1, 1, "scene")
check("json rename renamed:true + edits", jr:find('"renamed":true', 1, true) ~= nil
      and jr:find('"edits":', 1, true) ~= nil
      and jr:find('"kind":"definition"', 1, true) ~= nil
      and jr:find('"newText":"scene"', 1, true) ~= nil
      and jr:find('"length":5', 1, true) ~= nil)
local jrBad = lsp.json("rename", renTxt, 1, 1, "bad name")
check("json rename rejected -> renamed:false", jrBad ~= nil
      and jrBad:find('"renamed":false', 1, true) ~= nil)
local jrMissing = lsp.json("rename", "[jump target=*missing]\n", 1, 18, "x")
check("json rename no-op -> renamed:true empty edits", jrMissing ~= nil
      and jrMissing:find('"renamed":true', 1, true) ~= nil
      and jrMissing:find('"edits":[]', 1, true) ~= nil)

-- Exit gate.
if failed > 0 then



    print(string.format("LSP TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("LSP TESTS DONE (%d passed)", passed))
