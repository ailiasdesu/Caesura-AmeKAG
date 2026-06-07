local t = require("tokenizer")
local passed, failed = 0, 0
local function check(name, ok)
    if ok then passed = passed + 1; print("  [OK] " .. name)
    else failed = failed + 1; print("  [FAIL] " .. name) end
end
print("\n=== Tokenizer Advanced Tests ===\n")

-- Label test
local r = t.parse("*start")
check("label token type", #r == 1 and r[1].type == "label" and r[1].name == "start")

-- iscript block
local r2 = t.parse("[iscript]\nlocal x = 1\nprint(x)\n[endscript]")
check("iscript block", #r2 == 1 and r2[1].type == "iscript")

-- Label in context
local r3 = t.parse("Hello *chapter1 more text")
check("label in text context (count)", #r3 == 3)
check("label in text context (type)", r3[2].type == "label")

-- Mixed commands + labels
local r4 = t.parse("[bg storage=bg1]\n*main_scene\nHello World")
check("mixed command + label + text", #r4 == 3)

-- Params
local r5 = t.parse("[move path=(100,200,300) time=500 accel=-2]")
check("move with complex params", #r5 == 1 and r5[1].type == "command" and r5[1].cmd == "move")

-- Comment skipping
local r6 = t.parse("[bgm storage=bgm1]; this is a comment\n[wait time=1000]")
check("comment ignored", #r6 == 2)

-- Full story smoke test
local r7 = t.parse([[
*start
[bg storage=classroom]
[fg storage=hero_normal pos=center]
[wait time=1000]
Hello, this is a test.
[bgm storage=bgm01 loop=1]
; this line is a comment
*chapter2
[se file=click.wav]
[end]
]])
check("full story token count >= 8", #r7 >= 8)

print(string.format("\nResults: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
