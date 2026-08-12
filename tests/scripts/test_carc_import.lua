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
local EXE = "bin" .. SEP .. "Debug" .. SEP .. "carc_pack.exe"
local TMP_IN = "tmp" .. SEP .. "carc_test_in"
local TMP_CARC = "tmp" .. SEP .. "carc_test.carc"
local TMP_OUT = "tmp" .. SEP .. "carc_test_out"

local function shell(cmd)
    return pcall(os.execute, cmd)
end

-- ---------------------------------------------------------------------------
-- 1. pack a KAG3-style scene into a CARC archive
-- ---------------------------------------------------------------------------
shell('rmdir /s /q "' .. TMP_IN .. '" 2>nul')
shell('mkdir "' .. TMP_IN .. '" 2>nul')
local f = io.open(TMP_IN .. SEP .. "legacy.ks", "w")
f:write('[ch text="HP: &f.hp && MP: &mp.mp"]\n[waitse]\n[chara_show name="hero"]\n*start\n[end]\n')
f:close()
shell(EXE .. ' "' .. TMP_IN .. '" "' .. TMP_CARC .. '" >nul 2>nul')
local carcExists = io.open(TMP_CARC, "r") ~= nil
check("carc_pack creates archive", carcExists)

-- ---------------------------------------------------------------------------
-- 2. list subcommand returns hash entries
-- ---------------------------------------------------------------------------
shell(EXE .. ' list "' .. TMP_CARC .. '" > "tmp' .. SEP .. 'carc_list.txt" 2>nul')
local lf = io.open("tmp" .. SEP .. "carc_list.txt", "r")
local listContent = lf and lf:read("*a") or ""
if lf then lf:close() end
check("list prints entries", listContent:find("%x%x%x%x", 1) ~= nil)

-- ---------------------------------------------------------------------------
-- 3. extract --path restores the scene
-- ---------------------------------------------------------------------------
shell('rmdir /s /q "' .. TMP_OUT .. '" 2>nul')
shell(EXE .. ' extract "' .. TMP_CARC .. '" "' .. TMP_OUT
    .. '" --path "legacy.ks" >nul 2>nul')
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
shell('rmdir /s /q "tmp' .. SEP .. 'carc_test_full" 2>nul')
shell(EXE .. ' extract "' .. TMP_CARC .. '" "tmp' .. SEP
    .. 'carc_test_full" >nul 2>nul')
local fullCount = 0
local pf = io.popen('dir /b "tmp' .. SEP .. 'carc_test_full" 2>nul')
if pf then
    for _ in pf:lines() do fullCount = fullCount + 1 end
    pf:close()
end
check("full extract yields files", fullCount >= 1)

-- cleanup
shell('rmdir /s /q "' .. TMP_IN .. '" 2>nul')
shell('rmdir /s /q "' .. TMP_OUT .. '" 2>nul')
shell('rmdir /s /q "tmp' .. SEP .. 'carc_test_full" 2>nul')
os.remove(TMP_CARC)
os.remove("tmp" .. SEP .. "carc_list.txt")

-- Exit gate.
if failed > 0 then
    print(string.format("CARC IMPORT TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("CARC IMPORT TESTS DONE (%d passed)", passed))
