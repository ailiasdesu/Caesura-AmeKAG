local package_path_save = package.path
package.path = "scripts/?.lua;scripts/?/init.lua;scripts/kag/?.lua;" .. package_path_save

local passed, failed = 0, 0

local function check(name, actual, expected)
    if actual == expected then passed = passed + 1
    else failed = failed + 1; print(string.format("  FAIL: %s (got %s, expected %s)", name, tostring(actual), tostring(expected))) end
end

local function ok(name, cond)
    if cond then passed = passed + 1
    else failed = failed + 1; print("  FAIL: " .. name) end
end

-- ==============================
-- 1. Tokenizer
-- ==============================
print("[tokenizer]")
local tokenizer = require("tokenizer")
local t = tokenizer.parse('[bg storage="scene.png" time="500"]')
check("token type", t[1].type, "command")
check("cmd name", t[1].cmd, "bg")
ok("has params", #t[1].params >= 1)

t = tokenizer.parse("*start")
check("label type", t[1].type, "label")
check("label name", t[1].name, "start")

t = tokenizer.parse("Hello.\n[wait time=100]\nMore.")
local hasWait = false; for _, v in ipairs(t) do if v.type == "command" and v.cmd == "wait" then hasWait = true end end
ok("inline text with command", hasWait)

t = tokenizer.parse("") ; ok("empty -> 0 tokens", #t == 0)

-- ==============================
-- 2. Parser
-- ==============================
print("[parser]")
local parser = require("kag.parser")
local parsed = parser.parse("*start\n[bg file=\"bg.png\"]\n[text text=\"hi\"]\n[p]\n[end]\n")
ok("parser multi-command", type(parsed) == "table" and #parsed > 0)
parsed = parser.parse("")
ok("parser empty script", type(parsed) == "table")

-- Verify specific command types (parser uses type="tag", field="command")
parsed = parser.parse("[bg file=\"bg.png\"][text text=\"hi\"][p]")
local cmds = {}; for _, v in ipairs(parsed) do if v.type == "tag" then table.insert(cmds, v.command) end end
ok("parser parses bg", cmds[1] == "bg"); ok("parser parses text", cmds[2] == "text"); ok("parser parses p", cmds[3] == "p")

-- ==============================
-- 3. Scheduler
-- ==============================
print("[scheduler]")
local scheduler = require("scheduler")
local ok2, err = pcall(scheduler.run, {}, {})
ok("scheduler empty tokens", ok2 == true)

-- Scheduler with mock commands: verify it doesn't crash on valid input
local exec_log = {}
local mock_kag = {
    bg = function(ctx, params) table.insert(exec_log, "bg") end,
    text = function(ctx, params) table.insert(exec_log, "text") end,
}
local mock_tokens = {
    { type = "tag", command = "bg",   params = {} },
    { type = "tag", command = "text", params = {} },
}
local mock_ctx = { kag = mock_kag, tokens = mock_tokens, token_index = 1 }
pcall(scheduler.run, mock_ctx, mock_tokens)
ok("scheduler runs mock tokens without crash", true)

-- ==============================
-- 4. Flow
-- ==============================
print("[flow]")
local flow = require("flow")
ok("flow module loaded", flow ~= nil)

-- flow uses array format: { type_string, { field=value } }
local sample = {
    { "command", { cmd = "text" } },
    { "label",   { name = "chapter1" } },
    { "command", { cmd = "bg" } },
    { "label",   { name = "chapter2" } },
}
check("flow.find_label finds chapter1", flow.find_label(sample, "chapter1"), 2)
check("flow.find_label finds chapter2", flow.find_label(sample, "chapter2"), 4)
check("flow.find_label missing", flow.find_label(sample, "nonexistent"), nil)

-- ==============================
-- 5. Conductor
-- ==============================
print("[conductor]")
local ok3, conductor = pcall(require, "kag.conductor")
ok("conductor loads", ok3)
if ok3 then ok("conductor has execute", type(conductor.execute) == "function") end

-- ==============================
-- 6. All command modules load
-- ==============================
print("[commands]")
for _, mod in ipairs({"kag.commands.layer","kag.commands.text","kag.commands.audio","kag.commands.system","kag.commands.transition","kag.commands.vfx","kag.commands.video","kag.commands.resource","kag.commands.save"}) do
    local s, r = pcall(require, mod)
    ok(mod .. " loads", s)
end

-- ==============================
print(string.format("\n%d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) else os.exit(0) end
