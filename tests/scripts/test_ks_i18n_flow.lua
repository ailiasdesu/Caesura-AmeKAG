-- test_ks_i18n_flow.lua — i18n translation-workflow close-the-loop:
--   extract {key} refs + [ch]/[text] literals -> backfill (find_key_missing /
--   find_missing) -> engine loads the filled dictionary (i18n.load / set_language
--   / localize) and resolves both per-line and {key} translations, including
--   plural variant tables. Also CLI-level --keys / --missing (subprocess).
local results = {}
local function check(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    results[#results + 1] = cond
end

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local i18n = require("i18n")
local ks = require("ks_i18n")

-- Save i18n state (suite shares globals) and restore at the end.
local saved = { current = i18n.current, strings = i18n.strings,
                lines = i18n.lines, fallback = i18n.fallback,
                default_language = i18n.default_language }

local IS_WIN = package.config:sub(1, 1) == "\\"
local function mkdirs(path)
    if IS_WIN then
        os.execute('mkdir "' .. path:gsub("/", "\\") .. '" 2>nul')
    else
        os.execute('mkdir -p "' .. path .. '"')
    end
end
local function rmdirs(path)
    if IS_WIN then
        os.execute('rmdir /s /q "' .. path:gsub("/", "\\") .. '" 2>nul')
    else
        os.execute('rm -rf "' .. path .. '"')
    end
end

-- Repo-local tmp dir (forward slashes — fileutil scan_dir C6 whitelist).
local tmpdir = "tmp/test_ks_i18n_flow"
local function reset_tmp()
    rmdirs("tmp/test_ks_i18n_flow")
    mkdirs("tmp/test_ks_i18n_flow")
end
reset_tmp()

-- ---------------------------------------------------------------------------
-- 1. extract_key_refs — {key} string-table reference extraction (unit-level)
-- ---------------------------------------------------------------------------
local ks_text = (
    "[ch name=\"N\" text=\"Hello {greeting} {missing_one}\"]\n"
    .. "[p]\n"
    .. "plain {present_key} text\n"
    .. "[p]\n"
    .. "[button text=\"Go {go_key}\" target=\"*a\"]\n[endbutton]\n"
    .. "[ch name=\"N\" text=\"markup {b}bold{/b}\"]\n"
    .. "[ch name=\"N\" text=\"value [${tv.x}] expr\"]\n"
)
local refs = ks.extract_key_refs(ks_text)
local function countKey(list, key)
    local n = 0
    for _, r in ipairs(list) do if r.key == key then n = n + 1 end end
    return n
end
check("flow extract: greeting ref found", countKey(refs, "greeting") == 1)
check("flow extract: missing_one ref found", countKey(refs, "missing_one") == 1)
check("flow extract: present_key ref from bare text", countKey(refs, "present_key") == 1)
check("flow extract: go_key ref from button", countKey(refs, "go_key") == 1)
check("flow extract: markup {b} never a ref", countKey(refs, "b") == 0)
-- the ${...} region is stripped, so its contents are not false refs
check("flow extract: ${} body not a ref", countKey(refs, "tv") == 0
      and countKey(refs, "x") == 0)

-- ---------------------------------------------------------------------------
-- 2. collect_key_refs — file-level inventory with reuse + scene context
-- ---------------------------------------------------------------------------
local fk = io.open(tmpdir .. "/k.ks", "w")
fk:write(ks_text)
fk:close()
local coll = ks.collect_key_refs(tmpdir)
local function findColl(key)
    for _, c in ipairs(coll) do if c.key == key then return c end end
    return nil
end
check("flow collect: sorted by key", coll[1].key < coll[2].key)
local g = findColl("greeting")
check("flow collect: first dialogue context kept", g ~= nil
      and g.first:find("Hello {greeting}", 1, true) ~= nil)
check("flow collect: scene in files set", g ~= nil and g.files["k.ks"] ~= nil)
check("flow collect: count is occurrence count", g ~= nil and g.count == 1)

-- ---------------------------------------------------------------------------
-- 3. backfill gate — find_key_missing flags missing string-table keys
-- ---------------------------------------------------------------------------
local partial = { greeting = "你好", go_key = "去" }
local miss = ks.find_key_missing(tmpdir, partial)
check("flow backfill: 2 of 4 unique key refs missing",
      miss.total == 4 and miss.missing == 2)
local function hasEntry(list, key)
    for _, e in ipairs(list) do if e.key == key then return true end end
    return false
end
check("flow backfill: missing_one flagged", hasEntry(miss.entries, "missing_one"))
check("flow backfill: present_key flagged (not in dict)", hasEntry(miss.entries, "present_key"))
check("flow backfill: resolved keys excluded",
      not hasEntry(miss.entries, "greeting") and not hasEntry(miss.entries, "go_key"))
check("flow backfill: missing carries counts", (function()
    for _, e in ipairs(miss.entries) do
        if e.key == "missing_one" and e.count == 1 then return true end
    end
    return false
end)())
-- full coverage -> zero missing
local full = { greeting = "x", go_key = "x", present_key = "x", missing_one = "x" }
check("flow backfill: full dict -> zero missing",
      ks.find_key_missing(tmpdir, full).missing == 0)
-- lang table shape (top-level keys = string table; lines excluded)
local langShape = { lines = { ["k.ks:abc"] = "tx" }, greeting = "hi", go_key = "g" }
local miss2 = ks.find_key_missing(tmpdir, langShape)
check("flow backfill: lang-table top-level used, lines excluded",
      miss2.missing == 2 and miss2.total == 4)

-- ---------------------------------------------------------------------------
-- 4. end-to-end close-the-loop: extract -> generate template -> fill dict
--    -> engine loads + set_language + localize resolves both the
--    per-line content key and the {key} string-table token.
-- ---------------------------------------------------------------------------
local e2e_dir = tmpdir .. "/e2e"
mkdirs(e2e_dir)
local fe = io.open(e2e_dir .. "/scene.ks", "w")
fe:write("[ch name=\"Mio\" text=\"Hello {greeting}\"]\n[p]\nthe captain rowed away\n[p]\n")
fe:close()

-- (a) generate a fresh template (content-addressed per-line keys)
local body, tkeys = ks.build_template(e2e_dir, nil)
check("flow e2e: 2 per-line messages extracted", tkeys == 2)
local lineKey = "scene.ks:" .. i18n.fnv1a("Hello {greeting}")
local lineKey2 = "scene.ks:" .. i18n.fnv1a("the captain rowed away")
check("flow e2e: template carries content key", body:find(lineKey, 1, true) ~= nil)
check("flow e2e: template carries bare-text key", body:find(lineKey2, 1, true) ~= nil)

-- (b) "translator" fills the dictionary: string keys (greeting) + per-line
local filled = {
    greeting = "こんにちは",
    lines = {
        [lineKey] = "こんにちは！ {greeting}",
        [lineKey2] = "船頭は漕ぎ去った。",
    },
}
-- (c) engine loads it and resolves
i18n.current = "en"
i18n.strings = { greeting = "hello" }
i18n.lines = {}
i18n.fallback = {}
i18n.set_language("ja")  -- no ja file -> builtin; then override below
check("flow e2e: current set to ja", i18n.current == "ja")
i18n.strings = filled
i18n.lines = filled.lines
check("flow e2e: per-line translation resolves",
      i18n.localize("the captain rowed away", "scene.ks") == "船頭は漕ぎ去った。")
-- A per-line translation is returned verbatim (the runtime does NOT
-- re-expand {key} tokens inside an already-localized line — that matches
-- i18n.localize precedence: translated line wins over expand).
local resolved = i18n.localize("Hello {greeting}", "scene.ks")
check("flow e2e: translated line returned verbatim",
      resolved == "こんにちは！ {greeting}")
check("flow e2e: bare {key} translates via string table",
      i18n.localize("{greeting}", "scene.ks") == "こんにちは")

-- backfill confirms the filled dict is complete for BOTH directions
local e2eMiss = ks.find_missing(e2e_dir, filled)
check("flow e2e: per-line fully translated", e2eMiss.missing == 0)
local e2eKeyMiss = ks.find_key_missing(e2e_dir, filled)
check("flow e2e: {key} refs fully resolved", e2eKeyMiss.missing == 0)

-- ---------------------------------------------------------------------------
-- 5. plural-variant {key}: a referenced plural table resolves both forms
-- ---------------------------------------------------------------------------
local fp = io.open(tmpdir .. "/pl.ks", "w")
fp:write("[ch name=\"N\" text=\"box holds {items}\"]\n[p]\n")
fp:close()
local plDict = { items = { one = "{n} item", other = "{n} items" } }
check("flow plural: key ref extracted", (function()
    local rr = ks.extract_key_refs("box {items}")
    return #rr == 1 and rr[1].key == "items"
end)())
-- backfill gate over a dir containing ONLY pl.ks: the plural-variant dict
-- is complete (all {key} refs resolve), so zero missing.
local plOnly = tmpdir .. "/plonly"
mkdirs(plOnly)
local fpp = io.open(plOnly .. "/pl.ks", "w")
fpp:write("[ch name=\"N\" text=\"box holds {items}\"]\n[p]\n")
fpp:close()
check("flow plural: not missing when plural table present",
      ks.find_key_missing(plOnly, plDict).missing == 0)
i18n.current = "en"
i18n.strings = plDict
i18n.lines = {}
check("flow plural: en one form", i18n.translate("items", { n = 1 }) == "1 item")
check("flow plural: en other form", i18n.translate("items", { n = 3 }) == "3 items")
i18n.current = "ja"
i18n.strings.items = { other = "{n} 個" }
check("flow plural: ja single form", i18n.translate("items", { n = 5 }) == "5 個")
-- and a {key} reference in dialogue resolves through the plural table
i18n.current = "ja"
check("flow plural: localize of {items} string emits generic form",
      i18n.localize("box holds {items}", "scene.ks") == "box holds {n} 個")

-- ---------------------------------------------------------------------------
-- 6. CLI-level --keys / --missing via REAL subprocess (os.exit would kill
--    the whole suite in-process; same contract as test_i18n.lua round 74).
-- ---------------------------------------------------------------------------
local SEP = package.config:sub(1, 1)
local CLI_DIR = tmpdir .. "/cli"
mkdirs(CLI_DIR)
local fcli = io.open(CLI_DIR .. "/s.ks", "w")
fcli:write("[ch name=\"N\" text=\"Greet {greeting}\"]\n[p]\nplain line\n[p]\n[ch text=\"{extra}\"]\n[p]\n")
fcli:close()
local cliLang = CLI_DIR .. "/lang.lua"
local fl = io.open(cliLang, "w")
fl:write("{ greeting = \"こんにちは\", }\n")
fl:close()

-- (a) module-level backfill check on the loaded lang: greeting resolves,
-- extra missing
local km = ks.find_key_missing(CLI_DIR, ks.load_lang(cliLang))
check("flow cli: greeting resolves, extra missing",
      km.total == 2 and km.missing == 1)

-- locate the interpreter (vendored lua first), wrap Windows paths
local SAVE_ARG = _G.arg
local cands = {}
if type(SAVE_ARG) == "table" and type(SAVE_ARG[-1]) == "string"
    and #SAVE_ARG[-1] > 0 then cands[#cands+1] = SAVE_ARG[-1] end
for _, c in ipairs({ "external" .. SEP .. "lua" .. SEP .. "lua.exe",
                     "external" .. SEP .. "lua" .. SEP .. "lua" }) do
    cands[#cands+1] = c
end
cands[#cands+1] = "lua5.4"
cands[#cands+1] = IS_WIN and "lua.exe" or "lua"
local LUA = nil
for _, c in ipairs(cands) do
    local pathLike = c:find(SEP, 1, true) or c:find("/", 1, true)
    if not pathLike or io.open(c, "r") then LUA = c break end
end
local function wrapCmd(cmd)
    if IS_WIN and LUA and (LUA:find("[ %(%)]") or LUA:find(SEP, 1, true)) then
        return 'call "' .. LUA .. '" ' .. cmd
    end
    return LUA .. " " .. cmd
end
local base = ' scripts/ks_i18n.lua --dir "' .. CLI_DIR .. '" --out "' .. cliLang .. '"'

-- (b) --keys subprocess: exit 1 (extra unresolved), report lists both refs
local rep = nil
local fk2 = io.popen(wrapCmd(base .. " --keys"))
if fk2 then rep = fk2:read("*a"); fk2:close() end
if rep then
    check("flow cli --keys: lists greeting", rep:find("greeting(", 1, true) ~= nil)
    check("flow cli --keys: flags extra as MISSING",
          rep:find("extra(1 ref, in s.ks) <<MISSING>>", 1, true) ~= nil)
    check("flow cli --keys: summary counts 2 refs 1 unresolved",
          rep:find("2 unique {key} refs (1 unresolved)", 1, true) ~= nil)
else
    print("SKIP flow cli --keys: no LUA interpreter located")
end

-- (c) --missing subprocess: per-line backlog (3/3) + unresolved {extra}
local rep2 = nil
local fk3 = io.popen(wrapCmd(base .. " --missing"))
if fk3 then rep2 = fk3:read("*a"); fk3:close() end
if rep2 then
    check("flow cli --missing: reports per-line backlog",
          rep2:find("3/3 keys untranslated", 1, true) ~= nil)
    check("flow cli --missing: reports unresolved {extra} key ref",
          rep2:find("{extra} (1 ref, in s.ks)", 1, true) ~= nil)
    -- {greeting} DOES appear in the per-line -- original: comment; what we
    -- assert is it is NOT reported as an unresolved key ref (the key-ref
    -- section records each missing key in the "N ref," format).
    check("flow cli --missing: resolved greeting not flagged as key miss",
          rep2:find("{greeting} (", 1, true) == nil)
else
    print("SKIP flow cli --missing: no LUA interpreter located")
end
_G.arg = SAVE_ARG

-- ---------------------------------------------------------------------------
-- cleanup
-- ---------------------------------------------------------------------------
i18n.current, i18n.strings = saved.current, saved.strings
i18n.lines, i18n.fallback = saved.lines, saved.fallback
i18n.default_language = saved.default_language
rmdirs("tmp/test_ks_i18n_flow")

local failed = 0
for _, ok in ipairs(results) do
    if not ok then failed = failed + 1 end
end
if failed > 0 then
    print(string.format("KS_I18N FLOW: %d passed, %d FAILED",
        #results - failed, failed))
    os.exit(1)
end
print(string.format("KS_I18N FLOW TESTS DONE (%d passed)", #results))
