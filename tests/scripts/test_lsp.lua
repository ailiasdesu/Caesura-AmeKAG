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

-- Exit gate.
if failed > 0 then
    print(string.format("LSP TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("LSP TESTS DONE (%d passed)", passed))
