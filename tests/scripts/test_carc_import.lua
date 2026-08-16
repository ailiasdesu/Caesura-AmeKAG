-- test_carc_import.lua — Battle 5d: CARC archive scene import tests.
-- Packs a KAG3-style scene into a .carc (carc_pack), extracts it, and
-- asserts kag3_import converts the archived scene correctly.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
        failed = failed + 1 end
end

local import = require("kag3_import")
local tokenizer = require("tokenizer")

local SEP = package.config:sub(1, 1)  -- "\" on Windows, "/" elsewhere
local IS_WIN = SEP == "\\"
-- Multi-config (MSVC) puts the tool at bin/Debug/carc_pack.exe; single-
-- config (Linux/macOS) at bin/carc_pack. Pick whichever exists so the
-- suite runs on every CI platform (round 60).
local EXE = nil
for _, c in ipairs(IS_WIN and {
    "bin" .. SEP .. "Debug" .. SEP .. "carc_pack.exe",
    "bin" .. SEP .. "carc_pack.exe",
} or {
    "bin" .. SEP .. "Debug" .. SEP .. "carc_pack",
    "bin" .. SEP .. "carc_pack",
}) do
    if io.open(c, "r") then EXE = c break end
end
if not EXE then
    print("SKIP: carc_pack binary not found (checked bin/Debug and bin)")
    return  -- round 71: os.exit(0) here silently KILLED the whole main suite
end
local TMP_IN = "tmp" .. SEP .. "carc_test_in"
local TMP_CARC = "tmp" .. SEP .. "carc_test.carc"
local TMP_OUT = "tmp" .. SEP .. "carc_test_out"

local function shell(cmd)
    return pcall(os.execute, cmd)
end

-- Portable directory reset (round 60: the Lua suites now run on Linux and
-- macOS CI where rmdir/mkdir 2>nul are not valid).
local function reset_dir(path)
    if IS_WIN then
        shell('rmdir /s /q "' .. path .. '" 2>nul')
        shell('mkdir "' .. path .. '" 2>nul')
    else
        shell('rm -rf "' .. path .. '"')
        shell('mkdir -p "' .. path .. '"')
    end
end
local REDIR = IS_WIN and ">nul 2>nul" or ">/dev/null 2>&1"

-- ---------------------------------------------------------------------------
-- 1. pack a KAG3-style scene into a CARC archive
-- ---------------------------------------------------------------------------
reset_dir(TMP_IN)
local f = io.open(TMP_IN .. SEP .. "legacy.ks", "w")
f:write('[ch text="HP: &f.hp && MP: &mp.mp"]\n[waitse]\n[chara_show name="hero"]\n*start\n[end]\n')
f:close()
-- Nested relative path (doc: extract --path restores the original relative
-- path into a subdirectory). Pack catches both files in one archive.
if IS_WIN then shell('mkdir "' .. TMP_IN .. SEP .. 'sub' .. '" 2>nul') else shell('mkdir -p "' .. TMP_IN .. '/sub"') end
local nf = io.open(TMP_IN .. SEP .. "sub" .. SEP .. "nested.ks", "w")
nf:write('[ch text="nested"]\n[end]\n')
nf:close()
shell(EXE .. ' "' .. TMP_IN .. '" "' .. TMP_CARC .. '" ' .. REDIR)
local carcExists = io.open(TMP_CARC, "r") ~= nil
check("carc_pack creates archive", carcExists)

-- ---------------------------------------------------------------------------
-- 2. list subcommand returns hash entries
-- ---------------------------------------------------------------------------
shell(EXE .. ' list "' .. TMP_CARC .. '" > "tmp' .. SEP .. 'carc_list.txt" '
    .. (IS_WIN and "2>nul" or "2>/dev/null"))
local lf = io.open("tmp" .. SEP .. "carc_list.txt", "r")
local listContent = lf and lf:read("*a") or ""
if lf then lf:close() end
check("list prints entries", listContent:find("%x%x%x%x", 1) ~= nil)
-- The tool prints the 32-byte path hash as 64 hex chars, one per line
-- (doc: "list 打印归档内文件的路径哈希（每行一个，machine-readable）").
local hashOk = true
for line in (listContent .. "\n"):gmatch("([^\r\n]*)\n") do
    if line ~= "" and not (#line == 64 and line:match("^%x+$")) then
        hashOk = false
    end
end
check("list lines are 64-hex path hashes", hashOk)

-- ---------------------------------------------------------------------------
-- 3. extract --path restores the scene
-- ---------------------------------------------------------------------------
reset_dir(TMP_OUT)
shell(EXE .. ' extract "' .. TMP_CARC .. '" "' .. TMP_OUT
    .. '" --path "legacy.ks" ' .. REDIR)
local ef = io.open(TMP_OUT .. SEP .. "legacy.ks", "r")
local extracted = ef and ef:read("*a") or ""
if ef then ef:close() end
check("extract --path restores content",
      extracted:find("HP: &f.hp", 1, true) ~= nil)

-- ---------------------------------------------------------------------------
-- 4. kag3_import converts the archived scene
-- ---------------------------------------------------------------------------
local rep = import.processScene(TMP_OUT .. SEP .. "legacy.ks")
check("archived scene imports", rep ~= nil)
if rep then
    check("&var embeds converted", rep.converted_embeds >= 1)
    check("waitse renamed", rep.renames >= 1)
    check("chara_show reported", #rep.unsupported == 1
          and rep.unsupported[1].cmd == "chara_show")
    local out = rep.output
    check("output has %f.hp% interpolation",
          out:find("%f.hp%", 1, true) ~= nil)
    check("output has waitsound", out:find("[waitsound]", 1, true) ~= nil)
    check("output re-tokenizes", #tokenizer.parse(out) > 0)
end

-- ---------------------------------------------------------------------------
-- 5. full extract (hash-named) does not crash
-- ---------------------------------------------------------------------------
reset_dir("tmp" .. SEP .. "carc_test_full")
shell(EXE .. ' extract "' .. TMP_CARC .. '" "tmp' .. SEP
    .. 'carc_test_full" ' .. REDIR)
local fullCount = 0
local listCmd = IS_WIN
    and ('dir /b "tmp' .. SEP .. 'carc_test_full" 2>nul')
    or ('ls -1 "tmp' .. SEP .. 'carc_test_full" 2>/dev/null')
local pf = io.popen(listCmd)
if pf then
    for _ in pf:lines() do fullCount = fullCount + 1 end
    pf:close()
end
check("full extract yields files", fullCount >= 1)

-- ---------------------------------------------------------------------------
-- 6. extract --path restores a nested relative path into a subdirectory
--    (doc: "提取单个文件到原始相对路径（需 --path 指定已知路径）")
-- ---------------------------------------------------------------------------
reset_dir("tmp" .. SEP .. "carc_test_nested")
shell(EXE .. ' extract "' .. TMP_CARC .. '" "tmp' .. SEP
    .. 'carc_test_nested" --path "sub/nested.ks" ' .. REDIR)
local nef = io.open("tmp" .. SEP .. "carc_test_nested" .. SEP .. "sub" .. SEP .. "nested.ks", "r")
local nestedContent = nef and nef:read("*a") or ""
if nef then nef:close() end
check("extract --path restores nested relative path",
      nestedContent:find("nested", 1, true) ~= nil)

-- ---------------------------------------------------------------------------
-- 7. extract --path with an unknown relative path is rejected (no file written)
-- ---------------------------------------------------------------------------
local before = ""
local pf2 = io.popen((IS_WIN and 'dir /b "tmp' .. SEP .. 'carc_test_nested" 2>nul'
    or 'ls -1 "tmp' .. SEP .. 'carc_test_nested" 2>/dev/null'))
if pf2 then
    before = pf2:read("*a") or ""
    pf2:close()
end
shell(EXE .. ' extract "' .. TMP_CARC .. '" "tmp' .. SEP
    .. 'carc_test_nested" --path "does_not_exist.ks" ' .. REDIR)
local after = ""
local pf3 = io.popen((IS_WIN and 'dir /b "tmp' .. SEP .. 'carc_test_nested" 2>nul'
    or 'ls -1 "tmp' .. SEP .. 'carc_test_nested" 2>/dev/null'))
if pf3 then
    after = pf3:read("*a") or ""
    pf3:close()
end
check("extract --path unknown path writes nothing", before == after)
os.remove("tmp" .. SEP .. "carc_test_nested" .. SEP .. "sub" .. SEP .. "nested.ks")

-- cleanup
if IS_WIN then
    shell('rmdir /s /q "' .. TMP_IN .. '" 2>nul')
    shell('rmdir /s /q "' .. TMP_OUT .. '" 2>nul')
    shell('rmdir /s /q "tmp' .. SEP .. 'carc_test_full" 2>nul')
    shell('rmdir /s /q "tmp' .. SEP .. 'carc_test_nested" 2>nul')
else
    shell('rm -rf "' .. TMP_IN .. '"')
    shell('rm -rf "' .. TMP_OUT .. '"')
    shell('rm -rf "tmp' .. SEP .. 'carc_test_full"')
    shell('rm -rf "tmp' .. SEP .. 'carc_test_nested"')
end
os.remove(TMP_CARC)
os.remove("tmp" .. SEP .. "carc_list.txt")

-- Exit gate.
if failed > 0 then
    print(string.format("CARC IMPORT TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("CARC IMPORT TESTS DONE (%d passed)", passed))
