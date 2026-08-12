-- test_aidev.lua — Battle 4c extension: AI-assisted DEVELOPMENT tests.
-- Mock the _G.AI binding; verify the local (deterministic) explainer,
-- LLM fix suggestions / scene generation with sanitization, structural
-- review, and graceful degradation — all without a real LLM.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
        failed = failed + 1 end
end

-- Mock AI binding (same shape as test_aiwriter.lua)
local mock_ai = {
    last_prompt = nil,
    last_system = nil,
    response = nil,
    available_result = true,
}
local function install_mock_ai()
    _G.AI = {
        available = function() return mock_ai.available_result end,
        query = function(prompt, opts)
            mock_ai.last_prompt = prompt
            mock_ai.last_system = opts and opts.system or nil
            if mock_ai.response == nil then
                return nil, "mock-llm-error"
            end
            return mock_ai.response, nil
        end,
    }
end
local function restore_ai()
    _G.AI = nil
end

local aidev = require("kag.aidev")

-- ---------------------------------------------------------------------------
-- 1. local explainer: deterministic rule table (no LLM needed)
-- ---------------------------------------------------------------------------
restore_ai()
local e1 = aidev.explain_diagnostic({ scene = "a.ks", line = 3,
    message = "unknown KAG command 'wiat' (will render as text at runtime)" })
check("explain unknown command", type(e1) == "string"
      and e1:find("拼写错误", 1, true) ~= nil)
local e2 = aidev.explain_diagnostic({ scene = "a.ks", line = 5,
    message = "expression in [if] does not compile: f.x &&" })
check("explain expr compile", e2:find("编译", 1, true) ~= nil)
local e3 = aidev.explain_diagnostic({ scene = "a.ks", line = 9,
    message = "parse stream stopped before end of input" })
check("explain truncation", e3:find("未闭合", 1, true) ~= nil)
local e4 = aidev.explain_diagnostic({ scene = "a.ks", line = 2,
    message = "param 'mode' must be one of: on, off, toggle" })
check("explain enum", e4:find("枚举", 1, true) ~= nil)
local e5 = aidev.explain_diagnostic({ scene = "a.ks", line = 4,
    message = "pt: param 'speed' expects a number, got \"fast\"" })
check("explain number", e5:find("数字", 1, true) ~= nil)
local e6 = aidev.explain_diagnostic({ scene = "a.ks", line = 7,
    message = "some unheard-of diagnostic text" })
check("explain fallback", type(e6) == "string" and #e6 > 20)

-- ---------------------------------------------------------------------------
-- 2. explain with LLM enrichment (mock)
-- ---------------------------------------------------------------------------
install_mock_ai()
mock_ai.response = "You mistyped the command name."
local e7 = aidev.explain_diagnostic({ scene = "a.ks", line = 3,
    message = "unknown KAG command 'wiat'", source = "[wiat time=100]" },
    { llm = true })
check("explain llm enriches", e7:find("AI 补充", 1, true) ~= nil
      and e7:find("mistyped", 1, true) ~= nil)
check("explain llm prompt has diag", mock_ai.last_prompt:find("wiat", 1, true) ~= nil)
mock_ai.response = nil
local e8 = aidev.explain_diagnostic({ scene = "a.ks", line = 3,
    message = "unknown KAG command 'wiat'" }, { llm = true })
check("explain llm degrade keeps local", e8:find("拼写错误", 1, true) ~= nil)

-- ---------------------------------------------------------------------------
-- 3. suggest_fix: sanitized correction, degradation
-- ---------------------------------------------------------------------------
mock_ai.response = 'That tag is wrong. Use:\n[wait time=100]\n'
local f1, ferr1 = aidev.suggest_fix(
    "*start\n[wiat time=100]\n[end]\n", { line = 2, message = "unknown KAG command 'wiat'" })
check("suggest_fix returns tags", type(f1) == "string"
      and f1:find("[wait time=100]", 1, true) ~= nil)
check("suggest_fix comments prose", f1:find("; (ai)", 1, true) ~= nil)
check("suggest_fix prompt has context", mock_ai.last_prompt:find("wiat", 1, true) ~= nil)
restore_ai()
local f2, ferr2 = aidev.suggest_fix("*start\n", { line = 1, message = "x" })
check("suggest_fix degrades", f2 == nil and ferr2 ~= nil)

-- ---------------------------------------------------------------------------
-- 4. gen_scene: full scene + self-review warnings
-- ---------------------------------------------------------------------------
install_mock_ai()
mock_ai.response = [[
*start
[bg storage="room.jpg"]
[ch name="Aoi" text="Welcome."]
[if exp="f.keys > 0"]
[button text="Open door" target="*door"]
[button text="Leave" target="*leave"]
[endif]
[wait time=500]
*door
[ch name="Aoi" text="It opens."]
[jump target="*leave"]
*leave
[ch name="Aoi" text="Bye."]
[end]
]]
local g1, gerr1 = aidev.gen_scene("a room with a locked door")
check("gen_scene returns scene", type(g1) == "string"
      and g1:find("[end]", 1, true) ~= nil
      and g1:find("*start", 1, true) ~= nil)
check("gen_scene keeps flow", g1:find("[if exp=", 1, true) ~= nil
      and g1:find("[endif]", 1, true) ~= nil
      and g1:find("[button", 1, true) ~= nil)
check("gen_scene prompt has spec", mock_ai.last_prompt:find("locked door", 1, true) ~= nil)

-- self-review: a generated scene missing [end] gets a warning comment
mock_ai.response = '*start\n[ch name="A" text="x"]\n'
local g2 = aidev.gen_scene("short")
check("gen_scene appends review warning", type(g2) == "string"
      and g2:find("; (aidev) review", 1, true) ~= nil)

-- ---------------------------------------------------------------------------
-- 5. review_scene: local structural scan (no LLM)
-- ---------------------------------------------------------------------------
restore_ai()
local r1 = aidev.review_scene(
    "*start\n[if exp=\"f.x\"]\n[ch text=\"a\"]\n[endif]\n[end]\n")
check("review clean scene", #r1 == 0)
local r2 = aidev.review_scene(
    "*start\n[if exp=\"f.x\"]\n[ch text=\"a\"]\n[while exp=\"f.y\"]\n[ch text=\"b\"]\n")
local unclosed_if, unclosed_while, missing_end = false, false, false
for _, w in ipairs(r2) do
    if w.message:find("if", 1, true) and w.message:find("未闭合", 1, true) then
        unclosed_if = true
    end
    if w.message:find("while", 1, true) and w.message:find("未闭合", 1, true) then
        unclosed_while = true
    end
    if w.message:find("[end]", 1, true) then missing_end = true end
end
check("review finds unclosed if", unclosed_if)
check("review finds unclosed while", unclosed_while)
check("review flags missing end", missing_end)
local r3 = aidev.review_scene("*start\n[endif]\n[end]\n")
local extra_endif = false
for _, w in ipairs(r3) do
    if w.message:find("多余的", 1, true) then extra_endif = true end
end
check("review finds stray endif", extra_endif)
local r4 = aidev.review_scene("not a [scene at all !!!")
check("review handles unparseable", type(r4) == "table" and #r4 >= 1)
-- case-mismatched flow command flagged (runtime renders it as text)
local r5 = aidev.review_scene("*start\n[ENDIF]\n[end]\n")
local case_warn = false
for _, w in ipairs(r5) do
    if w.message:find("大小写", 1, true) then case_warn = true end
end
check("review flags case-mismatched flow", case_warn)
-- json encoder escapes tabs / control chars from LLM replies
install_mock_ai()
mock_ai.response = "[ch text=\"a\tb\"]\n"
local j4 = aidev.json("suggest_fix", "*start\n", { line = 1, message = "x" })
check("json escapes tab", j4:find("a\\tb", 1, true) ~= nil)
restore_ai()

-- ---------------------------------------------------------------------------
-- 6. json bridge for the IDE
-- ---------------------------------------------------------------------------
install_mock_ai()
local j1 = aidev.json("available")
check("json available", j1:find('"text":"true"', 1, true) ~= nil)
mock_ai.response = "[wait time=100]\n"
local j2 = aidev.json("suggest_fix", "*start\n[wiat time=100]\n",
    { line = 2, message = "unknown KAG command 'wiat'" })
check("json suggest_fix", j2:find("wait time=100", 1, true) ~= nil
      and j2:find('"error"', 1, true) ~= nil)
local j3 = aidev.json("review_scene", "*start\n[if exp=\"f.x\"]\n[end]\n")
check("json review_scene findings", j3:find("line", 1, true) ~= nil)
restore_ai()

-- Exit gate.
if failed > 0 then
    print(string.format("AIDEV TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("AIDEV TESTS DONE (%d passed)", passed))
