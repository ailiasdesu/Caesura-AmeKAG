-- =============================================================================
--  test_tokenizer.lua — Tokenizer unit tests
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local tokenizer = require("tokenizer")

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
        print(string.format("  [PASS] %s", name))
    else
        failed = failed + 1
        print(string.format("  [FAIL] %s  -- %s", name, detail or ""))
    end
end

print("\n=== Tokenizer Tests ===\n")

-- 1. Basic tag parsing
do
    local tokens = tokenizer.parse("[bg]")
    check("bg tag count", #tokens == 1, "got " .. #tokens)
    if #tokens > 0 then
        check("bg type", tokens[1].type == "command")
        check("bg cmd name", tokens[1].cmd == "bg")
    end
end

-- 2. Tag with params
do
    local tokens = tokenizer.parse("[bg storage=test]")
    check("bg with param", #tokens == 1)
    if #tokens > 0 then
        check("param present", tokens[1].params ~= nil)
    end
end

-- 3. Text chunk
do
    local tokens = tokenizer.parse("Hello World")
    check("text parsed", #tokens >= 1)
end

-- 4. Multiple tags
do
    local tokens = tokenizer.parse("[bg][fg]")
    check("multi-tag count", #tokens == 2)
end

-- 5. Label
do
    local tokens = tokenizer.parse("*label_name")
    check("label parsed", #tokens == 1)
end

-- 6. If/endif
do
    local tokens = tokenizer.parse("[if exp='true'][endif]")
    check("if-endif token count", #tokens >= 2)
end

-- 7. Macro
do
    local tokens = tokenizer.parse("[macro name=test][endmacro]")
    check("macro token count", #tokens == 2)
end

-- Batch A token tests
local tokens_se = tokenizer.parse("[se file=\"click.wav\" loop=0]")
local tokens_stopse = tokenizer.parse("[stopse]")
local tokens_wait = tokenizer.parse("[wait time=500]")

-- REGRESSION (C++ demo e2e): the explicit P_se prefix must NOT shadow
-- longer command names once bare-value params exist
local t_setbgm = tokenizer.parse("[setbgmvolume volume=0.6]")
check("setbgmvolume distinct", t_setbgm[1].cmd == "setbgmvolume")
local t_waitbgm = tokenizer.parse("[waitbgm]")
check("waitbgm distinct", t_waitbgm[1].cmd == "waitbgm")
local t_setse = tokenizer.parse("[setsevolume v=1]")
check("setsevolume distinct", t_setse[1].cmd == "setsevolume")

check("se tag parsed", tokens_se[1].cmd == "se")
check("se file param present",
      type(tokens_se[1].params) == "table" and tokens_se[1].params[1][2] == "click.wav")
check("stopse tag parsed", tokens_stopse[1].cmd == "stopse")
check("fadebgm tag parsed", tokenizer.parse("[fadebgm time=1000]")[1].cmd == "fadebgm")
check("fadese tag parsed", tokenizer.parse("[fadese 200]")[1].cmd == "fadese")
check("wait tag parsed", tokens_wait[1].cmd == "wait")
check("wait time param present", tokens_wait[1].params[1][2] == "500")
check("delay tag parsed", tokenizer.parse("[delay 500]")[1].cmd == "delay")
check("skip tag parsed", tokenizer.parse("[skip]")[1].cmd == "skip")

-- Error reporting: parse failure carries a line number (binary search)
do
    local ok, err = pcall(function() tokenizer.parse("[") end)
    if ok then failed = failed + 1 else passed = passed + 1 print("  [PASS] parse failure raises") end
    if not ok and tostring(err):find("line") then passed = passed + 1 print("  [PASS] parse failure has line") else failed = failed + 1 end
end

print(string.format("\nResults: %d passed, %d failed", passed, failed))
-- REGRESSION (lpeg ^1 batch zero-width guard, review should-fix):
-- [cmd k=v ] must NOT gain an empty bare param; [cmd x=] falls to the
-- bare-value shape, never an empty named value
local t_trail = tokenizer.parse("[cmd k=v ]")
check("trailing-space param count", #t_trail[1].params == 1
      and t_trail[1].params[1][1] == "k" and t_trail[1].params[1][2] == "v")
local t_empty = tokenizer.parse("[cmd x=]")
check("empty-value falls to bare", t_empty[1].params[1][1] == "1"
      and t_empty[1].params[1][2] == "x=")
local t_q = tokenizer.parse('[cmd k=""]')
check("quoted empty kept", t_q[1].params[1][1] == "k"
      and t_q[1].params[1][2] == "")

if failed > 0 then os.exit(1) end
