-- test_i18n_cmd.lua — [i18n language=] command (round 76)
package.path = "scripts/?.lua;scripts/kag/?.lua;scripts/kag/commands/?.lua;" .. package.path

local passed, failed = 0, 0
local function check(name, cond, extra)
    if cond then passed = passed + 1 print("PASS " .. name)
    else failed = failed + 1 print("FAIL " .. name .. (extra and (" -- " .. tostring(extra)) or "")) end
end

local sys = require("kag.commands.system")
local i18n = require("i18n")

-- 1. handler + contract registered
check("i18n handler exists", type(sys.i18n) == "function")
local schema = require("kag.schema")
local c = schema.dumpContracts()["i18n"]
check("i18n contract registered", c ~= nil and c.language ~= nil and c.language.required == true, tostring(c and c.language and c.language.required))
-- auto-registration into the kag table
local okk, kag = pcall(require, "kag")
check("i18n auto-registered in kag table", okk and type(kag) == "table" and type(kag.i18n) == "function")

-- 2. missing language -> graceful notice, no crash
local ctx0 = { f = {}, sf = {}, tf = {}, mp = {}, settingsValues = {},
    text_state = {}, backlog = {}, _choiceButtons = {}, current_scene = "t.ks" }
local ok1, r1 = pcall(sys.i18n, ctx0, {})
check("i18n missing language returns true", ok1 and r1 == true, tostring(r1))

-- 3. set_language path (unknown code falls back; no crash)
local prevLang = i18n.current_language()
local ctx1 = { f = {}, sf = {}, tf = {}, mp = {}, settingsValues = {},
    text_state = {}, backlog = {}, _choiceButtons = {}, current_scene = "t.ks" }
local ok2, r2 = pcall(sys.i18n, ctx1, { language = "zz_unknown" })
check("i18n unknown code returns true", ok2 and r2 == true)
check("i18n sets settingsValues.language", ctx1.settingsValues.language == "zz_unknown", tostring(ctx1.settingsValues.language))
check("i18n switches current_language", i18n.current_language() == "zz_unknown", tostring(i18n.current_language()))

-- 4. known code (en built-in)
local ctx2 = { f = {}, sf = {}, tf = {}, mp = {}, settingsValues = {},
    text_state = {}, backlog = {}, _choiceButtons = {}, current_scene = "t.ks" }
local ok3 = pcall(sys.i18n, ctx2, { language = "en" })
check("i18n en returns true", ok3)
check("i18n en switches current_language", i18n.current_language() == "en", tostring(i18n.current_language()))

-- 5. scheduler e2e: [i18n language="xx"] token dispatches without error
local scheduler = require("scheduler")
local ctx3 = { f = {}, sf = {}, tf = {}, mp = {}, lf = {}, variables = {},
    tokens = { { "i18n", { language = "de" } } }, token_index = 1,
    current_scene = "t.ks", settingsValues = {}, text_state = {},
    backlog = {}, _choiceButtons = {} }
local co = coroutine.create(function() scheduler.run(ctx3, ctx3.tokens, 1) end)
local ok4, err4 = true, nil
while coroutine.status(co) ~= "dead" do
    local okStep, errStep = coroutine.resume(co, 16)
    if not okStep then ok4, err4 = false, errStep break end
    if ctx3.waiting_input then ctx3.waiting_input = false end
end
check("i18n dispatches through scheduler", ok4, tostring(err4))
check("i18n scheduler sets language", ctx3.settingsValues.language == "de", tostring(ctx3.settingsValues.language))

-- restore the previous language for suite isolation
pcall(i18n.set_language, prevLang or "en")

if failed > 0 then os.exit(1) end
print("I18N CMD TESTS DONE")