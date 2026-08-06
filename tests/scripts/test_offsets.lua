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

-- BLOCKING regression (review): mid-line command with NO whitespace --
-- the old init = endpos + 1 skipped the '[' and re-tokenized it as text
local compact = tokenizer.parse_with_offsets('hello[ch text="x"]')
check("compact 2 tokens", #compact == 2)
check("compact ch offset", compact[2].type == "command"
      and compact[2].offset == 6)
check("compact text kept", compact[1].type == "text"
      and compact[1].content == "hello")
-- adjacent commands ][
local adj = tokenizer.parse_with_offsets('[cl][ch text="y"]')
check("adjacent 2 commands", #adj == 2
      and adj[1].cmd == "cl" and adj[2].cmd == "ch" and adj[2].offset == 5)

if failed > 0 then os.exit(1) end
print("OFFSET TESTS DONE")
