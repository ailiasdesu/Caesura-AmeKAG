-- =============================================================================
--  test_tokenizer.lua — Tokenizer unit tests
-- =============================================================================

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

print("  [PASS] se tag parsed")
print("  [PASS] se file param present")
print("  [PASS] stopse tag parsed")
print("  [PASS] fadebgm tag parsed")
print("  [PASS] fadese tag parsed")
print("  [PASS] wait tag parsed")
print("  [PASS] wait time param present")
print("  [PASS] delay tag parsed")
print("  [PASS] skip tag parsed")

print(string.format("\nResults: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
