-- test_offsets.lua — parse_with_offsets byte-accuracy (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local tokenizer = require("tokenizer")
local src = '[ch name="Hero" text="\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c"]\n[bg storage="bg.png"]\n; comment\n[ch text="line2"]'
local toks = tokenizer.parse_with_offsets(src)
check("three tokens", #toks == 3)
check("ch offset 1", toks[1].offset == 1)
-- each offset points at the '[' of its command
check("bg offset points at [", src:sub(toks[2].offset, toks[2].offset) == "[")
check("ch2 offset points at [", src:sub(toks[3].offset, toks[3].offset) == "[")
-- offsets strictly increase (byte positions, UTF-8 multi-byte aware)
check("offsets increase", toks[1].offset < toks[2].offset
      and toks[2].offset < toks[3].offset)
-- empty / BOM handled
check("empty returns {}", #tokenizer.parse_with_offsets("") == 0)
local bom = "\xef\xbb\xbf[ch text=\"x\"]"
check("BOM stripped", tokenizer.parse_with_offsets(bom)[1].offset == 1)

if failed > 0 then os.exit(1) end
print("OFFSET TESTS DONE")
