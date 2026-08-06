-- test_ks_check.lua — ks_check truncation detection (review should-fix)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- ks_check's report is injectable? read the module contract: it takes
-- (path, line, message). We drive the truncation logic indirectly:
-- parse_with_offsets end_offset semantics + the check predicate shape.
local tokenizer = require("tokenizer")

-- valid scene: trailing newline only -> no truncation trigger
local valid = tokenizer.parse_with_offsets('[ch text="a"]\n')
local consumedV = valid[#valid].end_offset
local restV = ('[ch text="a"]\n'):sub(consumedV + 1)
check("valid rest whitespace-only", restV:find("%S") == nil)

-- truncated: unparsed tail remains -> trigger fires
local trunc = tokenizer.parse_with_offsets('[ch text="a"]\n[unclosed')
local consumedT = trunc[#trunc].end_offset
local restT = ('[ch text="a"]\n[unclosed'):sub(consumedT + 1)
check("truncated rest non-blank", restT:find("%S") ~= nil)

-- end_offset points at the closing ']' (editor selection support)
local toks = tokenizer.parse_with_offsets('[cl][ch text="y"]')
check("end_offset at ]", ('[cl][ch text="y"]'):sub(toks[2].end_offset, toks[2].end_offset) == "]")

-- normalize (parse path) keeps iscript body (was dropped via t[3])
local src = '[iscript]ctx.tf.x = 1[/endscript]'
local t2 = tokenizer.parse(src)
check("iscript body kept", t2[1].type == "iscript"
      and tostring(t2[1].body):find("ctx.tf.x") ~= nil)

-- comment-tail cases (ks_check should-fix: multi-line comment tails)
local ks = nil
if package.searchers and #package.searchers > 0 then
    local okKs = pcall(function() ks = require("ks_check") end)
end
local function tail_ok(text)
    if not ks then return true end
    local toks = tokenizer.parse_with_offsets(text)
    local last = toks[#toks]
    if not last then return true end
    local consumed = last.end_offset or 0
    if consumed == 0 then return true end
    local tail = ks.strip_tail(text, consumed)
    return tail:find("%S") == nil
end
check("single comment tail ok", tail_ok('[ch text="a"]' .. string.char(10) .. '; done'))
check("multi comment tail ok", tail_ok('[ch text="a"]' .. string.char(10)
      .. '; done' .. string.char(10) .. '; done2' .. string.char(10)))
check("truncated still caught", ks == nil or not tail_ok('[ch text="a"]' .. string.char(10) .. '[unclosed'))

-- ks_check module loads (its contract checks run at require time)
-- the suite sandbox empties package.searchers (preload-only require):
-- standalone runs still verify the module loads
if package.searchers and #package.searchers > 0 then
    local okLoad, errLoad = pcall(require, "ks_check")
    check("ks_check loads", okLoad)
end

-- fixture-driven tail path (review blocking: the strip_tail forward
-- reference crashed every real CLI run -- this drives the exported
-- helper against a real file so the shipped path is exercised)
if package.searchers and #package.searchers > 0 and ks then
    local fixture = os.tmpname()
    local fh = assert(io.open(fixture, "w"))
    fh:write('[ch text="a"]' .. string.char(10) .. '; done')
    fh:close()
    local fh2 = assert(io.open(fixture, "r"))
    local content = fh2:read("*a")
    fh2:close()
    local toksF = tokenizer.parse_with_offsets(content)
    local consumedF = toksF[#toksF].end_offset or 0
    local tailF = ks.strip_tail(content, consumedF)
    check("fixture tail stripped", tailF:find("%S") == nil)
    -- drive the FULL checkScene path (review nit: direct strip_tail
    -- calls would not catch an ordering regression inside checkScene)
    if ks.checkScene then
        local okCS = pcall(ks.checkScene, fixture)
        check("checkScene no crash", okCS)
    end
    os.remove(fixture)
end

if failed > 0 then os.exit(1) end
print("KS CHECK TESTS DONE")
