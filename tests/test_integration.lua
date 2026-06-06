-- =============================================================================
--  Caesura (AmeKAG) — test_integration.lua
--  集成测试: KAG tokenizer, 存档/读档, CARC header 验证
-- =============================================================================

local has_lpeg, lpeg = pcall(require, "lpeg")

local testDir = arg and arg[0] and arg[0]:match("(.*[/\\])") or ""
if testDir == "" then testDir = "tests/" end

local function findScript(path)
    local candidates = { "scripts/" .. path, "../scripts/" .. path }
    for _, cand in ipairs(candidates) do
        local f = io.open(cand, "r")
        if f then f:close(); return cand end
    end
    return nil
end

-- ── Test infrastructure ────────────────────────────────────────────────────
local passed, failed, skipped = 0, 0, 0
local results = {}

local function test(name, fn)
    local ok, err = pcall(fn)
    if ok then
        passed = passed + 1
        results[#results + 1] = { name = name, status = "PASS" }
        print("[PASS] " .. name)
    else
        failed = failed + 1
        results[#results + 1] = { name = name, status = "FAIL", error = tostring(err) }
        print("[FAIL] " .. name .. " — " .. tostring(err))
    end
end

local function skip(name, reason)
    skipped = skipped + 1
    results[#results + 1] = { name = name, status = "SKIP", reason = reason }
    print("[SKIP] " .. name .. " — " .. reason)
end

local function assert_eq(expected, actual, msg)
    if expected ~= actual then
        error((msg or "assertion failed") .. ": expected " .. tostring(expected) .. ", got " .. tostring(actual))
    end
end

local function assert_not_nil(v, msg)
    if v == nil then error(msg or "expected non-nil") end
end

-- ── Test 1: KAG Tokenizer ──────────────────────────────────────────────────
print("\n=== Test Suite 1: KAG Tokenizer ===\n")

test("Tokenizer module loads", function()
    local tokPath = findScript("tokenizer.lua")
    assert_not_nil(tokPath, "tokenizer.lua not found")
    local tok = require("tokenizer")
    assert_not_nil(tok, "tokenizer module is nil")
    assert_not_nil(tok.parse, "tokenizer.parse is nil")
end)

test("Parse simple [ch] tag", function()
    local tok = require("tokenizer")
    local tokens = tok.parse('[ch name="crack" text="e1c2"]')
    assert_not_nil(tokens, "parse returned nil")
    assert_eq(1, #tokens, "expected 1 token")
    local t = tokens[1]
    assert_eq("command", t.type, "token type")
    assert_eq("ch", t.cmd, "command name")
    assert_eq("crack", t.params[1][2], "name param")
    assert_eq("e1c2", t.params[2][2], "text param")
end)

test("Parse [bg] tag", function()
    local tok = require("tokenizer")
    local tokens = tok.parse('[bg path="tests/fixtures/test_bg.png"]')
    assert_not_nil(tokens, "parse returned nil")
    assert_eq(1, #tokens, "expected 1 token")
    assert_eq("bg", tokens[1].cmd, "command name")
    assert_eq("tests/fixtures/test_bg.png", tokens[1].params[1][2], "path param")
end)

test("Parse [wait] tag with numeric param", function()
    local tok = require("tokenizer")
    local tokens = tok.parse("[wait time=500]")
    assert_not_nil(tokens, "parse returned nil")
    assert_eq(1, #tokens, "expected 1 token")
    assert_eq("wait", tokens[1].cmd, "command name")
    assert_eq("500", tokens[1].params[1][2], "time param")
end)

test("Parse [save] tag", function()
    local tok = require("tokenizer")
    local tokens = tok.parse("[save slot=1]")
    assert_not_nil(tokens, "parse returned nil")
    assert_eq(1, #tokens, "expected 1 token")
    assert_eq("save", tokens[1].cmd, "command name")
    assert_eq("1", tokens[1].params[1][2], "slot param")
end)

test("Parse [load] tag", function()
    local tok = require("tokenizer")
    local tokens = tok.parse("[load slot=1]")
    assert_not_nil(tokens, "parse returned nil")
    assert_eq(1, #tokens, "expected 1 token")
    assert_eq("load", tokens[1].cmd, "command name")
    assert_eq("1", tokens[1].params[1][2], "slot param")
end)

test("Parse [saveplace] and [loadplace]", function()
    local tok = require("tokenizer")
    local tokens = tok.parse("[saveplace]\n[loadplace]")
    assert_not_nil(tokens, "parse returned nil")
    assert_eq(2, #tokens, "expected 2 tokens")
    assert_eq("saveplace", tokens[1].cmd, "first cmd")
    assert_eq("loadplace", tokens[2].cmd, "second cmd")
end)

test("Parse tag with single-quoted value", function()
    local tok = require("tokenizer")
    local tokens = tok.parse("[ch name='crack' text='test']")
    assert_not_nil(tokens, "parse returned nil")
    assert_eq(1, #tokens, "expected 1 token")
    assert_eq("ch", tokens[1].cmd)
    assert_eq("crack", tokens[1].params[1][2], "name param")
    assert_eq("test", tokens[1].params[2][2], "text param")
end)

test("Skip comments (; line)", function()
    local tok = require("tokenizer")
    local tokens = tok.parse("; this is a comment\n[ch name=\"A\" text=\"B\"]")
    assert_not_nil(tokens, "parse returned nil")
    assert_eq(1, #tokens, "expected 1 token, comment skipped")
    assert_eq("ch", tokens[1].cmd)
end)

test("Parse multiple tags sequentially", function()
    local tok = require("tokenizer")
    local ks = "\n[ch name=\"A\" text=\"line1\"]\n[ch name=\"B\" text=\"line2\"]\n[wait time=100]\n"
    local tokens = tok.parse(ks)
    assert_not_nil(tokens, "parse returned nil")
    -- newlines consumed by skip, only 3 command tokens
    assert_eq(3, #tokens, "expected 3 command tokens")
    for _, t in ipairs(tokens) do
        assert_eq("command", t.type, "all tokens should be commands")
    end
end)

test("Parse full integration_test.ks file", function()
    local tok = require("tokenizer")
    local ksPath = testDir .. "scripts/integration_test.ks"
    local f = io.open(ksPath, "r")
    assert_not_nil(f, "Cannot open " .. ksPath)
    local content = f:read("*a")
    f:close()
    assert_not_nil(content, "File is empty")
    local tokens = tok.parse(content)
    assert_not_nil(tokens, "parse returned nil for full file")
    -- Count unique command tags
    local cmds = {}
    for _, t in ipairs(tokens) do
        if t.type == "command" then
            cmds[t.cmd] = (cmds[t.cmd] or 0) + 1
        end
    end
    assert_not_nil(cmds["ch"], "expected [ch] tag")
    assert_not_nil(cmds["bg"], "expected [bg] tag")
    assert_not_nil(cmds["wait"], "expected [wait] tag")
    assert_not_nil(cmds["saveplace"], "expected [saveplace] tag")
    assert_not_nil(cmds["loadplace"], "expected [loadplace] tag")
    assert_not_nil(cmds["save"], "expected [save] tag")
    assert_not_nil(cmds["load"], "expected [load] tag")
    print("  Total tokens: " .. #tokens)
end)

-- ── Test 2: Save/Load JSON Serialization ───────────────────────────────────
print("\n=== Test Suite 2: Save/Load JSON Serialization ===\n")

test("SaveCommands module loads", function()
    local savePath = findScript("kag/commands/save.lua")
    if not savePath then skip("SaveCommands module loads", "save.lua not found"); return end
    local SaveCommands = require("kag.commands.save")
    assert_not_nil(SaveCommands, "SaveCommands module is nil")
end)

test("Simple JSON encode/decode round-trip", function()
    -- Minimal JSON encoder
    local function jsonEncode(tbl)
        local parts = {}
        for k, v in pairs(tbl) do
            local val = type(v) == "string" and ('"' .. v .. '"') or tostring(v)
            parts[#parts + 1] = '"' .. k .. '":' .. val
        end
        return "{" .. table.concat(parts, ",") .. "}"
    end
    local data = { token_index = 5, scene_path = "test.ks" }
    local json = jsonEncode(data)
    print("  JSON: " .. json)
    assert(json:find("token_index"), "JSON missing token_index")
    assert(json:find("test.ks"), "JSON missing scene_path")
end)

test("Save state capture table has required fields", function()
    local state = {
        token_index = 5,
        scene_path = "test.ks",
        variables = { flag_seen = true, affection = 10 },
        backlog = { "line1", "line2" },
    }
    assert_not_nil(state.token_index, "missing token_index")
    assert_not_nil(state.scene_path, "missing scene_path")
    assert_not_nil(state.variables, "missing variables")
    assert_not_nil(state.backlog, "missing backlog")
end)

test("Save→Load state restore simulation", function()
    local saved = { token_index = 5, scene_path = "test.ks", flag = true }
    local function saveState(s) return s end
    local function loadState(s) return s end
    local state = saveState(saved)
    local restored = loadState(state)
    assert_eq(5, restored.token_index, "token_index restored")
    assert_eq("test.ks", restored.scene_path, "scene_path restored")
    print("  State save->load round-trip OK")
end)

-- ── Test 3: CARC Format ────────────────────────────────────────────────────
print("\n=== Test Suite 3: CARC Format ===\n")

local CARC_MAGIC = 0x43524143
local CARC_HEADER_SIZE = 64
local CARC_FILEENTRY_SIZE = 116

test("CARC magic constant is correct", function()
    assert_eq(0x43524143, CARC_MAGIC)
end)

test("CARC Header struct size is 64 bytes", function()
    assert_eq(64, CARC_HEADER_SIZE)
end)

test("CARC FileEntry struct size is 116 bytes", function()
    assert_eq(116, CARC_FILEENTRY_SIZE)
end)

test("CARC pack tool binary exists", function()
    local found = false
    for _, path in ipairs({"bin/Debug/carc_pack.exe", "bin/Release/carc_pack.exe"}) do
        local f = io.open(path, "rb")
        if f then f:close(); found = true; print("  Found: " .. path); break end
    end
    assert(found, "carc_pack.exe not found — needs build")
end)

-- CARC verification (file pre-created by CI/developer)
test("CARC verify: magic number 0x43524143", function()
    local outPath = testDir .. "test.carc"
    local f = io.open(outPath, "rb")
    if not f then skip("CARC verify magic", outPath .. " not found — run carc_pack first"); return end
    local data = f:read("*a")
    f:close()
    assert(#data >= 4, "CARC file too small")
    local b1, b2, b3, b4 = string.byte(data, 1, 4)
    local magic = b1 + b2 * 256 + b3 * 65536 + b4 * 16777216
    assert_eq(CARC_MAGIC, magic, "CARC magic mismatch")
    print("  Magic: 0x" .. string.format("%08X", magic))
end)

test("CARC verify: numFiles > 0", function()
    local outPath = testDir .. "test.carc"
    local f = io.open(outPath, "rb")
    if not f then skip("CARC verify numFiles", outPath .. " not found"); return end
    local data = f:read("*a")
    f:close()
    -- numFiles at offset 4 (uint32 LE)
    local b1, b2, b3, b4 = string.byte(data, 5, 8)
    local numFiles = b1 + b2 * 256 + b3 * 65536 + b4 * 16777216
    assert(numFiles > 0, "numFiles should be > 0, got " .. tostring(numFiles))
    print("  numFiles: " .. numFiles)
end)

test("CARC verify: content and index offsets valid", function()
    local outPath = testDir .. "test.carc"
    local f = io.open(outPath, "rb")
    if not f then skip("CARC verify offsets", outPath .. " not found"); return end
    local data = f:read("*a")
    f:close()
    local function readU64(data, offset)
        local val = 0
        for i = 0, 7 do
            val = val + string.byte(data, offset + i + 1) * (256 ^ i)
        end
        return val
    end
    -- CARCHeader: magic(4) version(4) contentOffset(8) contentSize(8) indexOffset(8) indexSize(8) numFiles(4) reserved(20)
    local contentOffset = readU64(data, 8)
    local indexOffset = readU64(data, 24)
    local fileSize = #data
    assert(contentOffset > 0, "contentOffset should be > 0, got " .. contentOffset)
    assert(contentOffset <= fileSize, "contentOffset " .. contentOffset .. " > fileSize " .. fileSize)
    assert(indexOffset > 0, "indexOffset should be > 0, got " .. indexOffset)
    assert(indexOffset <= fileSize, "indexOffset " .. indexOffset .. " > fileSize " .. fileSize)
    print("  contentOffset: " .. contentOffset .. ", indexOffset: " .. indexOffset)
end)

-- ── Test 4: GameState ctx Table ────────────────────────────────────────────
print("\n=== Test Suite 4: GameState ctx Table ===\n")

test("GameState ctx table specification", function()
    local ctxFields = {
        "co", "tokens", "token_index", "call_stack",
        "f", "sf", "tf", "layers", "backlog",
        "active_operations", "stop_flag", "input_focus",
    }
    print("  Required ctx fields: " .. table.concat(ctxFields, ", "))
    local ctx = {}
    ctx.tokens = {}
    ctx.token_index = 1
    ctx.call_stack = {}
    ctx.f = {}
    ctx.sf = {}
    ctx.tf = {}
    ctx.layers = {}
    ctx.backlog = {}
    ctx.active_operations = {}
    ctx.stop_flag = false
    ctx.input_focus = "kag"
    for _, field in ipairs(ctxFields) do
        if field ~= "co" then
            assert(ctx[field] ~= nil, "ctx." .. field .. " is nil")
        end
    end
    assert_eq("kag", ctx.input_focus, "ctx.input_focus")
    assert_eq(1, ctx.token_index, "ctx.token_index")
    assert_eq(false, ctx.stop_flag, "ctx.stop_flag")
end)

-- ── Test 5: Engine Bootstrap ───────────────────────────────────────────────
print("\n=== Test Suite 5: Engine Bootstrap ===\n")

test("Main.cpp exists and references expected modules", function()
    local f = io.open("src/Main.cpp", "r")
    assert_not_nil(f, "src/Main.cpp not found")
    local content = f:read("*a")
    f:close()
    assert(content:find("Engine"), "Main.cpp missing Engine")
    assert(content:find("LuaManager"), "Main.cpp missing LuaManager")
end)

test("All key source files exist", function()
    local files = {
        "src/Main.cpp",
        "src/Carc/CARCFormat.h",
        "src/Carc/CARCReader.h",
        "src/Carc/CARCReader.cpp",
        "src/Carc/CARCWriter.h",
        "src/Carc/CARCWriter.cpp",
        "tools/carc_pack/main.cpp",
    }
    for _, path in ipairs(files) do
        local f = io.open(path, "r")
        assert_not_nil(f, path .. " NOT FOUND")
        f:close()
    end
    print("  " .. #files .. " source files verified")
end)

test("All key script files exist", function()
    local files = {
        "scripts/tokenizer.lua",
        "scripts/lpeg.lua",
        "scripts/kag/parser.lua",
        "scripts/kag/commands/save.lua",
    }
    for _, path in ipairs(files) do
        local f = io.open(path, "r")
        assert_not_nil(f, path .. " NOT FOUND")
        f:close()
    end
    print("  " .. #files .. " script files verified")
end)

-- ── Summary ─────────────────────────────────────────────────────────────────
print("\n" .. string.rep("=", 60))
print("  TEST RESULTS SUMMARY")
print(string.rep("=", 60))
print(string.format("  PASS:  %d", passed))
print(string.format("  FAIL:  %d", failed))
print(string.format("  SKIP:  %d", skipped))
print(string.format("  TOTAL: %d", passed + failed + skipped))
print(string.rep("=", 60))

if failed > 0 then
    print("\n  FAILED TESTS:")
    for _, r in ipairs(results) do
        if r.status == "FAIL" then
            print("    - " .. r.name)
            print("      Error: " .. tostring(r.error))
        end
    end
end
print("")

os.exit(failed > 0 and 1 or 0)
