-- test_kag3_import.lua — KAG3 importer tests (scripts/kag3_import.lua)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local M = require("kag3_import")
local tokenizer = require("tokenizer")

-- ---------------------------------------------------------------------------
-- 1. &var embed conversion (text tokens and text-ish params)
-- ---------------------------------------------------------------------------
check("&f.x -> %f.x%", M.convertAmpVars("HP: &f.hp") == "HP: %f.hp%")
check("&tf.x -> %tf.x%", M.convertAmpVars("&tf.flag") == "%tf.flag%")
check("&sf.x -> %sf.x%", M.convertAmpVars("&sf.day") == "%sf.day%")
check("&mp.x -> %mp.x%", M.convertAmpVars("&mp.name") == "%mp.name%")
check("&lf.x -> %lf.x%", M.convertAmpVars("&lf.tmp") == "%lf.tmp%")
check("&& protected", M.convertAmpVars("a && b") == "a && b")
check("&& before &var", M.convertAmpVars("a && b &f.x") == "a && b %f.x%")
check("no & no-op", M.convertAmpVars("plain text") == "plain text")
check("& unknown ns untouched", M.convertAmpVars("&kag.status") == "&kag.status")
check("multi embeds", M.convertAmpVars("&f.a and &f.b") == "%f.a% and %f.b%")

-- ---------------------------------------------------------------------------
-- 2. Static TJS expression translation
-- ---------------------------------------------------------------------------
check("&& -> and", M.translateExpr("f.hp > 10 && f.mp > 5") == "f.hp > 10 and f.mp > 5")
check("|| -> or", M.translateExpr("f.a || f.b") == "f.a or f.b")
check("!= -> ~=", M.translateExpr("f.a != 3") == "f.a ~= 3")
check("! -> not", M.translateExpr("!f.a") == "not f.a")
check("string literal safe", M.translateExpr("f.s == \"a && b\"") == "f.s == \"a && b\"")
check("plain expr unchanged", M.translateExpr("f.hp == 10") == "f.hp == 10")

-- ---------------------------------------------------------------------------
-- 3. Command mapping table
-- ---------------------------------------------------------------------------
local notes1 = {}
local new1, n1 = M.convertCommand("waitse", {})
check("waitse renamed to waitsound", new1 == "waitsound" and #n1 == 1)
local new2, n2 = M.convertCommand("ch", {})
check("known command unchanged", new2 == "ch" and #n2 == 0)
local new3, n3 = M.convertCommand("chara_show", {})
check("chara_show reported unsupported", new3 == "chara_show"
      and #n3 == 1 and n3[1]:find("UNSUPPORTED", 1, true) ~= nil)

-- ---------------------------------------------------------------------------
-- 4. processScene: end-to-end check on a KAG3-style scene
-- ---------------------------------------------------------------------------
local scene = [[
; KAG3 legacy scene (typical)
*start
&f.hp 是生命值，&mp.mp 是魔力
[ch name="Hero" text="HP: &f.hp && MP: &mp.mp"]
[waitse]
[chara_show name="hero" storage="chara/hero.png"]
[if exp="f.hp > 10 && f.flag != 0"]
[ch text="ok"]
[else]
[ch text="no"]
[endif]
[jump target=*next]
*next
[iscript]
// TJS-ish comment inside Lua block
var x = 1;  // this is Lua, kept verbatim
[/endscript]
[ch text="done"]
]]

-- write temp scene
local tmp = os.tmpname() .. ".ks"
local f = io.open(tmp, "w")
f:write(scene)
f:close()

local rep, err = M.processScene(tmp)
check("processScene parses", rep ~= nil)
if rep then
    check("scene tokens counted", rep.tokens > 10)
    check("&var embeds converted (text + param)", rep.converted_embeds >= 2)
    check("TJS expression converted", rep.converted_exprs >= 1)
    check("waitse renamed", rep.renames >= 1)
    local foundChara = false
    for _, u in ipairs(rep.unsupported) do
        if u.cmd == "chara_show" and u.line == 6 then foundChara = true end
    end
    check("chara_show reported at line 6", foundChara)
    check("iscript block flagged", #rep.iscript_blocks == 1
          and rep.iscript_blocks[1] == 14)
    check("strict blocking detected", M.hasBlocking(rep))

    -- 5. convert rebuild: comments preserved + re-tokenizable
    local out = rep.output
    check("comment preserved", out:find("KAG3 legacy scene", 1, true) ~= nil)
    check("label preserved", out:find("*start", 1, true) ~= nil)
    check("&f.hp converted in output", out:find("text=\"HP: %f.hp%", 1, true) ~= nil)
    check("&& intact in output", out:find("MP: %mp.mp%\"", 1, true) ~= nil)
    check("TJS && translated to and in output",
          out:find("f.hp > 10 and f.flag ~= 0", 1, true) ~= nil)
    local retoks = tokenizer.parse(out)
    check("output re-tokenizes", type(retoks) == "table" and #retoks > 0)
    check("waitse renamed to waitsound in output",
          out:find("waitse", 1, true) == nil
          and out:find("[waitsound]", 1, true) ~= nil)
    check("unsupported kept verbatim (reported, not deleted)",
          out:find("chara_show", 1, true) ~= nil)
end

-- ---------------------------------------------------------------------------
-- 5b. clean scene: no unsupported, strict not blocking
-- ---------------------------------------------------------------------------
local clean = [[
*start
[ch text="hello &f.name"]
[jump target=*end]
*end
[stop]
]]
local tmp2 = os.tmpname() .. ".ks"
f = io.open(tmp2, "w")
f:write(clean)
f:close()
local rep2 = M.processScene(tmp2)
check("clean scene parses", rep2 ~= nil)
if rep2 then
    check("clean scene: no unsupported", #rep2.unsupported == 0)
    check("clean scene: no iscript", #rep2.iscript_blocks == 0)
    check("clean scene: not strict-blocking", not M.hasBlocking(rep2))
    check("clean scene: embed converted", rep2.converted_embeds >= 1)
end

-- ---------------------------------------------------------------------------
-- 6. error handling
-- ---------------------------------------------------------------------------
local rep3, err3 = M.processScene(os.tmpname() .. "-nonexistent.ks")
check("missing file returns error", rep3 == nil and err3 ~= nil)

-- cleanup
os.remove(tmp)
os.remove(tmp2)

-- Exit gate (runner convention).
if failed > 0 then
    print(string.format("KAG3 IMPORT TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("KAG3 IMPORT TESTS DONE (%d passed)", passed))
