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

if failed > 0 then os.exit(1) end
print("KS CHECK TESTS DONE")
