-- test_i18n.lua — Localization pipeline:
--   i18n.fnv1a content-addressed keys, i18n.localize precedence
--   (per-line translation > {key} token expansion > original),
--   inline-markup whitelist in expand, [ch]/[text] pipeline integration,
--   ks_i18n template generator (fresh + merge).
local results = {}
local function check(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    results[#results + 1] = cond
end

-- Mock the engine-side APIs (no GPU in unit tests)
_G.backend = _G.backend or {}
_G.backend = {
    create_solid_texture = function() return { _mock = true } end,
    render_text = function() end,
    set_input_focus = function() end,
    audio_play = function() end,
    create_viewport = function() return { _mock = true } end,
    destroy_viewport = function() end,
    clear_text = function() end,
    load_texture = function() return { _mock = true } end,
    submit_batch = function() end,
    text_set_state = function() end,
}

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local i18n = require("i18n")
local TextCommands = require("kag.commands.text")
local layers = require("layers")

-- Save i18n state (suite shares globals) and restore at the end.
local saved_current = i18n.current
local saved_strings = i18n.strings
local saved_lines = i18n.lines
local saved_fallback = i18n.fallback

-- ---------------------------------------------------------------------------
-- 1. fnv1a content addressing
-- ---------------------------------------------------------------------------
check("fnv1a known vector (matches ks_i18n template)",
      i18n.fnv1a("Welcome to Caesura AmeKAG Engine Demo.") == "21c6c4b9")
check("fnv1a is 8-hex", i18n.fnv1a("x"):match("^%x%x%x%x%x%x%x%x$") ~= nil)
check("fnv1a differs per message",
      i18n.fnv1a("a") ~= i18n.fnv1a("b"))
check("fnv1a deterministic", i18n.fnv1a("你好世界") == i18n.fnv1a("你好世界"))

-- ---------------------------------------------------------------------------
-- 2. localize precedence
-- ---------------------------------------------------------------------------
i18n.strings = { greeting = "你好", b = "BOLD" }
i18n.lines = {
    ["scene.ks:" .. i18n.fnv1a("hi")] = "bonjour",
    ["scene.ks:" .. i18n.fnv1a("{greeting} world")] = "世界你好",
}

check("localize: line translation wins",
      i18n.localize("hi", "scene.ks") == "bonjour")
check("localize: scene mismatch falls through",
      i18n.localize("hi", "other.ks") == "hi")
check("localize: token expansion",
      i18n.localize("say {greeting}", "x.ks") == "say 你好")
check("localize: translated line may carry its own key tokens",
      i18n.localize("{greeting} world", "scene.ks") == "世界你好")
check("localize: unknown key passes through",
      i18n.localize("no {such_key} here", "x.ks") == "no {such_key} here")
check("localize: empty text", i18n.localize("", "x.ks") == "")

-- Markup whitelist: even a defined string-table key must not eat markup.
check("expand: {b} whitelisted", i18n.expand("{b}bold{/b}") == "{b}bold{/b}")
check("expand: {i}/{s} whitelisted",
      i18n.expand("{i}x{/i}{s}y{/s}") == "{i}x{/i}{s}y{/s}")
check("expand: {color=...} untouched",
      i18n.expand("{color=#ff0000}red") == "{color=#ff0000}red")
check("expand: {size=36} untouched",
      i18n.expand("a{size=36}b") == "a{size=36}b")
check("expand: real key still expands",
      i18n.expand("{greeting}") == "你好")

-- ---------------------------------------------------------------------------
-- 3. [ch]/[text] pipeline integration
-- ---------------------------------------------------------------------------
layers.add_layer(nil, {
    name = "message", layer_type = 0,
    x = 0, y = 520, w = 1280, h = 200, visible = true,
})
local function fresh_ctx()
    return {
        backlog = {},
        f = {}, sf = {}, tf = {}, mp = {}, variables = {},
        characters = {},
        current_scene = "scene.ks", currentScene = "scene.ks", token_index = 1,
        text_state = { line = 1, char_offset = 0, opacity = 255,
                       cursor_x = 32, cursor_y = 580, draws = {} },
        textCursorX = 32, textCursorY = 580,
    }
end

local ctxC = fresh_ctx()
TextCommands.ch(ctxC, { name = "A", text = "hi" })
local found = false
for _, d in ipairs(ctxC.text_state.draws) do
    if d.text == "bonjour" then found = true end
end
check("ch: translated line drawn", found)
check("ch: backlog uses translated plain",
      ctxC.backlog[1] ~= nil and ctxC.backlog[1].text == "bonjour")

local ctxT = fresh_ctx()
TextCommands.text(ctxT, { text = "say {greeting}" })
local foundT = false
for _, d in ipairs(ctxT.text_state.draws) do
    if d.text == "say 你好" then foundT = true end
end
check("text: token expansion drawn", foundT)

local ctxM = fresh_ctx()
TextCommands.ch(ctxM, { text = "{b}bold{/b} and {greeting}" })
local boldDraw, tailDraw, literalB = false, false, false
for _, d in ipairs(ctxM.text_state.draws) do
    if d.text == "bold" and d.bold == true then boldDraw = true end
    if d.text == " and 你好" then tailDraw = true end
    if d.text:find("{b}", 1, true) then literalB = true end
end
check("ch: markup survives localization (bold span)",
      boldDraw and tailDraw and not literalB)

-- ---------------------------------------------------------------------------
-- 3c. [button]/[sel] choice labels
-- ---------------------------------------------------------------------------
local choiceKey = "scene.ks:" .. i18n.fnv1a("Choice A")
i18n.lines[choiceKey] = "選択肢A"
local savedClick = _G._KAG_onClick
local ctxB = fresh_ctx()
TextCommands.button(ctxB, { text = "Choice A", target = "*a" })
pcall(function() TextCommands.endbutton(ctxB, {}) end)
check("button: registered label localized",
      ctxB._choiceButtonsActive ~= nil
      and ctxB._choiceButtonsActive[1].text == "選択肢A")
check("button: draw uses localized label", ctxB.text_state.draws[1] ~= nil
      and ctxB.text_state.draws[1].text == "1. 選択肢A")
-- endbutton's trailing yield raised inside pcall: the installed click
-- closure is leaked unless restored (suite runs one process).
_G._KAG_onClick = savedClick
ctxB._choiceMode = nil
ctxB._choiceButtonsActive = nil

-- [sel] alias shares the handler (KAG3 select syntax)
local ctxS = fresh_ctx()
TextCommands.sel(ctxS, { text = "Choice A", target = "*a" })
check("sel: alias localizes too",
      ctxS._choiceButtons ~= nil and ctxS._choiceButtons[1].text == "選択肢A")
i18n.lines[choiceKey] = nil

-- ---------------------------------------------------------------------------
-- 3d. language hot-switch full-page redraw (relocalize_page)
--  A [ch]/[text] records its pre-localize source; switching the string
--  table and calling relocalize_page re-draws the page in the new
--  language, re-localizes the backlog / active choices / cc_text and
--  seals the typewriter (fully revealed).
-- ---------------------------------------------------------------------------
local ctxR = fresh_ctx()
TextCommands.ch(ctxR, { name = "A", text = "hi" })
check("redraw: page source recorded", #ctxR.text_state.page_src == 1
      and ctxR.text_state.page_src[1].src == "hi")
check("redraw: backlog carries src", ctxR.backlog[1] ~= nil
      and ctxR.backlog[1].src == "hi"
      and ctxR.backlog[1].text == "bonjour")
local drawBefore = nil
for _, d in ipairs(ctxR.text_state.draws) do
    if d.text == "bonjour" then drawBefore = d end
end
check("redraw: draw marked as page source",
      drawBefore ~= nil and drawBefore._page_src == true)

-- Switch language: the same content-addressed key maps to a new text.
i18n.lines["scene.ks:" .. i18n.fnv1a("hi")] = "hallo"
TextCommands.relocalize_page(ctxR)
local foundNew = false
for _, d in ipairs(ctxR.text_state.draws) do
    if d.text == "hallo" then foundNew = true end
end
check("redraw: current line re-localized", foundNew)
local oldGone = true
for _, d in ipairs(ctxR.text_state.draws) do
    if d.text == "bonjour" then oldGone = false end
end
check("redraw: old draw dropped", oldGone)
check("redraw: backlog re-localized from src",
      ctxR.backlog[1] ~= nil and ctxR.backlog[1].text == "hallo")
check("redraw: typewriter sealed", (function()
    for _, d in ipairs(ctxR.text_state.draws) do
        if d.typewriter then return false end
    end
    return true
end)())
check("redraw: page source entry not mutated",
      ctxR.text_state.page_src[1].opts.msgY == 580
      and ctxR.text_state.page_src[1].src == "hi")

-- {key} tokens re-expand with the NEW string table on redraw.
local ctxK = fresh_ctx()
TextCommands.text(ctxK, { text = "say {greeting}" })
i18n.strings.greeting = "Hello"
TextCommands.relocalize_page(ctxK)
local foundK = false
for _, d in ipairs(ctxK.text_state.draws) do
    if d.text == "say Hello" then foundK = true end
end
check("redraw: {key} re-expands with new strings", foundK)
i18n.strings.greeting = "你好"

-- Translated strings may carry markup: the redraw re-parses it.
local mkKey = "scene.ks:" .. i18n.fnv1a("{b}bold{/b} hi")
i18n.lines[mkKey] = "{b}fett{/b} hallo"
local ctxM2 = fresh_ctx()
TextCommands.ch(ctxM2, { text = "{b}bold{/b} hi" })
TextCommands.relocalize_page(ctxM2)
local boldAfter, halloAfter = false, false
for _, d in ipairs(ctxM2.text_state.draws) do
    if d.text == "fett" and d.bold == true then boldAfter = true end
    -- The second span keeps its leading space (" hallo").
    if d.text == " hallo" then halloAfter = true end
end
check("redraw: translated markup re-parsed", boldAfter and halloAfter)
i18n.lines[mkKey] = nil

-- Untranslated lines fall through unchanged.
local ctxU = fresh_ctx()
TextCommands.ch(ctxU, { text = "untouched line" })
TextCommands.relocalize_page(ctxU)
local foundU = false
for _, d in ipairs(ctxU.text_state.draws) do
    if d.text == "untouched line" then foundU = true end
end
check("redraw: untranslated line unchanged", foundU)

-- NVL accumulated page: every line re-localizes; a translation that
-- wraps to more lines shifts the following lines down (y cascade).
local ctxN = fresh_ctx()
ctxN.nvl_mode = true
ctxN.nvl_prefix_fmt = "「%s」："
TextCommands.ch(ctxN, { name = "A", text = "hi" })
TextCommands.ch(ctxN, { name = "B", text = "hi" })
check("redraw: nvl page sources recorded", #ctxN.text_state.page_src == 2)
i18n.lines["scene.ks:" .. i18n.fnv1a("hi")] = string.rep("x", 200)
TextCommands.relocalize_page(ctxN)
local xCount, minY, maxY = 0, math.huge, -math.huge
for _, d in ipairs(ctxN.text_state.draws) do
    if d.text:find("^x+$") then
        xCount = xCount + 1
        if d.y < minY then minY = d.y end
        if d.y > maxY then maxY = d.y end
    end
end
check("redraw: nvl long translation wraps (>=4 lines)", xCount >= 4)
check("redraw: nvl second line shifted down (cascade)",
      maxY - minY >= 96)
check("redraw: nvl cursor follows last line",
      ctxN.textCursorY ~= nil and ctxN.textCursorY > 300)
i18n.lines["scene.ks:" .. i18n.fnv1a("hi")] = "hallo"

-- Active choice block: labels re-localize and the group re-renders.
i18n.lines[choiceKey] = "選択肢A"
local ctxCH = fresh_ctx()
TextCommands.button(ctxCH, { text = "Choice A", target = "*a" })
pcall(function() TextCommands.endbutton(ctxCH, {}) end)
i18n.lines[choiceKey] = "新選択"
TextCommands.relocalize_page(ctxCH)
check("redraw: active choice re-localized",
      ctxCH._choiceButtonsActive ~= nil
      and ctxCH._choiceButtonsActive[1].text == "新選択")
local choiceDraw = nil
for _, d in ipairs(ctxCH.text_state.draws) do
    if d.group == "choices" then choiceDraw = d end
end
check("redraw: choice group re-rendered",
      choiceDraw ~= nil and choiceDraw.text == "1. 新選択")
_G._KAG_onClick = savedClick
ctxCH._choiceMode = nil
ctxCH._choiceButtonsActive = nil
i18n.lines[choiceKey] = nil

-- Closed captions (cc_mode) re-localize from their recorded source.
local ctxCC = fresh_ctx()
ctxCC.cc_mode = true
TextCommands.ch(ctxCC, { name = "A", text = "hi", voice = "v.wav" })
check("redraw: cc source recorded", ctxCC.cc_text ~= nil
      and ctxCC.cc_text.src == "hi")
TextCommands.relocalize_page(ctxCC)
check("redraw: cc re-localized", ctxCC.cc_text ~= nil
      and ctxCC.cc_text.text == "hallo")

-- Defense: bare ctx / empty page no-op.
check("redraw: nil ctx no-op", TextCommands.relocalize_page(nil) == false)
local ctxE = fresh_ctx()
TextCommands.relocalize_page(ctxE)
check("redraw: empty page no-op", #ctxE.text_state.draws == 0
      and #ctxE.text_state.page_src == 0)

-- Page source resets with the page (non-NVL [ch] clears both).
local ctxP = fresh_ctx()
TextCommands.ch(ctxP, { text = "hi" })
TextCommands.ch(ctxP, { text = "hi" })
check("redraw: page source reset per line",
      #ctxP.text_state.page_src == 1)

-- Backlog entries without a source (older saves) stay untouched.
local blX = { { text = "old text", src = nil } }
local ctxX = { backlog = blX, current_scene = "s.ks" }
TextCommands.relocalize_backlog(ctxX)
check("redraw: backlog without src untouched", blX[1].text == "old text")

-- ---------------------------------------------------------------------------
-- 3b. i18n.load reads a tool-generated lang file (comments + return shape)
-- ---------------------------------------------------------------------------
local saved2 = { current = i18n.current, strings = i18n.strings,
                 lines = i18n.lines, fallback = i18n.fallback }
i18n.load("ja")
check("load: generated lang file (comments+return) loads",
      i18n.current == "ja" and type(i18n.lines) == "table")
check("load: dialogue line key present",
      i18n.lines["galgame_demo.ks:21c6c4b9"] ~= nil)
check("load: settings keys preserved alongside lines",
      i18n.strings.fullscreen ~= nil)
check("load: empty placeholder falls back to original",
      i18n.localize("Welcome to Caesura AmeKAG Engine Demo.",
                    "galgame_demo.ks")
      == "Welcome to Caesura AmeKAG Engine Demo.")
i18n.current, i18n.strings = saved2.current, saved2.strings
i18n.lines, i18n.fallback = saved2.lines, saved2.fallback

-- ---------------------------------------------------------------------------
-- 4. ks_i18n template generator
-- ---------------------------------------------------------------------------
local ks = require("ks_i18n")
-- Round 71: use a REPO-LOCAL tmp dir (tmp/test_i18n) instead of the OS temp
-- dir. The OS temp path is enumerated via popen(dir/ls) and can be
-- environment-sensitive on CI runners; a repo-local dir is guaranteed
-- writable and enumerable (same reasoning as test_carc_import reset_dir).
-- NOTE: scan_dir (fileutil) REJECTS backslash paths (C6 whitelist allows
-- only %w/_%.:-), so tmpdir must be FORWARD slashes on every platform.
local tmpdir = "tmp/test_i18n"
local function reset_tmp()
    pcall(os.execute, 'rm -rf "tmp/test_i18n"')
    pcall(os.execute, 'mkdir -p "tmp/test_i18n"')
end
reset_tmp()
local fixture = "demo/x.ks"
local f = io.open(tmpdir .. "/x.ks", "w")
f:write('[ch name="A" text="Hello world"]\n[p]\nplain line\n[text text="Second"]\n'
     .. '[button text="Yes" target="*y"]\n[endbutton]\n[sel text="No"]\n[endselect]\n')
f:close()

local body, total, scenes, kept = ks.build_template(tmpdir, nil)
check("ks_i18n: 5 messages extracted (incl. button/sel labels)", total == 5)
check("ks_i18n: scene counted", scenes == 1)
check("ks_i18n: hash keys present",
      body:find('x.ks:' .. i18n.fnv1a("Hello world"), 1, true) ~= nil)
check("ks_i18n: bare text token extracted",
      body:find('x.ks:' .. i18n.fnv1a("plain line"), 1, true) ~= nil)
check("ks_i18n: [text] extracted",
      body:find('x.ks:' .. i18n.fnv1a("Second"), 1, true) ~= nil)
check("ks_i18n: [button] label extracted",
      body:find('x.ks:' .. i18n.fnv1a("Yes"), 1, true) ~= nil)
check("ks_i18n: [sel] label extracted",
      body:find('x.ks:' .. i18n.fnv1a("No"), 1, true) ~= nil)
check("ks_i18n: original in comment",
      body:find("original: Hello world", 1, true) ~= nil)

-- Merge mode: existing translation preserved, new keys appended.
local existing = {
    lines = { ["x.ks:" .. i18n.fnv1a("Hello world")] = "Hallo Welt" },
    title_screen = "TITLE",
}
local merged, totalM = ks.build_template(tmpdir, existing)
check("ks_i18n merge: keeps translation",
      merged:find('"Hallo Welt"', 1, true) ~= nil)
check("ks_i18n merge: same key count", totalM == 5)
check("ks_i18n merge: preserves other fields",
      merged:find('title_screen = "TITLE"', 1, true) ~= nil)

-- load_lang must parse tool-generated files (comments + return) so a
-- real --update round-trip never drops hand-authored settings keys.
local genPath = tmpdir .. "/gen.lua"
local g = io.open(genPath, "w")
g:write(body)
g:close()
local reloaded = ks.load_lang(genPath)
check("ks_i18n load_lang: generated file parses",
      reloaded ~= nil and type(reloaded.lines) == "table")

-- Hand-written lang files are bare table literals ({...}, no return).
-- The value deliberately contains the word "return": a bare substring
-- detection would misfire here and silently drop the file.
local handPath = tmpdir .. "/hand.lua"
local h = io.open(handPath, "w")
h:write('{\n  title_screen = "手書き return テスト",\n  lines = { ["a.ks:1"] = "x" },\n}\n')
h:close()
local reloadedHand = ks.load_lang(handPath)
check("ks_i18n load_lang: hand-written literal parses",
      reloadedHand ~= nil and reloadedHand.title_screen == "手書き return テスト"
      and reloadedHand.lines["a.ks:1"] == "x")
os.remove(handPath)

-- ---------------------------------------------------------------------------
-- 5. find_missing — untranslated report (CI gate for translators)
-- ---------------------------------------------------------------------------
local report = ks.find_missing(tmpdir, existing)
check("find_missing: 4 of 5 missing (1 translated)",
      report.total == 5 and report.missing == 4)
local translatedMissing = false
for _, e in ipairs(report.entries) do
    if e.key == "x.ks:" .. i18n.fnv1a("Hello world") then
        translatedMissing = true
    end
end
check("find_missing: translated key excluded", not translatedMissing)
check("find_missing: original text carried for context",
      report.entries[1].original ~= nil and #report.entries[1].original > 0)
local noLang = ks.find_missing(tmpdir, nil)
check("find_missing: no lang data -> all missing", noLang.missing == 5)
local fullLang = { lines = {} }
for _, e in ipairs(ks.collect_entries(tmpdir)) do
    fullLang.lines[e.key] = "x"
end
check("find_missing: fully translated -> zero missing",
      ks.find_missing(tmpdir, fullLang).missing == 0)
os.remove(genPath)
os.remove(tmpdir .. "/x.ks")

-- ---------------------------------------------------------------------------
-- cleanup: restore i18n state and the backend global
-- ---------------------------------------------------------------------------
i18n.current = saved_current
i18n.strings = saved_strings
i18n.lines = saved_lines
i18n.fallback = saved_fallback

local failed = 0
for _, ok in ipairs(results) do
    if not ok then failed = failed + 1 end
end
if failed > 0 then
    print(string.format("I18N TESTS: %d passed, %d FAILED",
        #results - failed, failed))
    os.exit(1)
end
print(string.format("I18N TESTS DONE (%d passed)", #results))
