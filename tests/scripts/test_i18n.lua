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
local ctxB = fresh_ctx()
TextCommands.button(ctxB, { text = "Choice A", target = "*a" })
pcall(function() TextCommands.endbutton(ctxB, {}) end)
check("button: registered label localized",
      ctxB._choiceButtonsActive ~= nil
      and ctxB._choiceButtonsActive[1].text == "選択肢A")
check("button: draw uses localized label", ctxB.text_state.draws[1] ~= nil
      and ctxB.text_state.draws[1].text == "1. 選択肢A")

-- [sel] alias shares the handler (KAG3 select syntax)
local ctxS = fresh_ctx()
TextCommands.sel(ctxS, { text = "Choice A", target = "*a" })
check("sel: alias localizes too",
      ctxS._choiceButtons ~= nil and ctxS._choiceButtons[1].text == "選択肢A")
i18n.lines[choiceKey] = nil

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
local tmpfile = os.tmpname():gsub("\\", "/")
local tmpdir = tmpfile:match("^(.*)[/\\]")
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
local handPath = tmpdir .. "/hand.lua"
local h = io.open(handPath, "w")
h:write('{\n  title_screen = "手書き",\n  lines = { ["a.ks:1"] = "x" },\n}\n')
h:close()
local reloadedHand = ks.load_lang(handPath)
check("ks_i18n load_lang: hand-written literal parses",
      reloadedHand ~= nil and reloadedHand.title_screen == "手書き"
      and reloadedHand.lines["a.ks:1"] == "x")
os.remove(handPath)
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
