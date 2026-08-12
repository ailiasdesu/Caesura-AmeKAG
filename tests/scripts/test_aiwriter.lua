-- test_aiwriter.lua — Battle 4c: AI-assisted scene writing tests.
-- Mock the _G.AI binding to simulate an LLM; verify tag generation,
-- scene continuation, sanitization, and graceful degradation.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
        failed = failed + 1 end
end

-- Mock AI binding: controllable responses
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

local aiwriter = require("kag.aiwriter")

-- ---------------------------------------------------------------------------
-- 1. degradation without LLM (no _G.AI binding)
-- ---------------------------------------------------------------------------
restore_ai()
check("available false without binding", aiwriter.available() == false)
local t1, e1 = aiwriter.generate_dialogue({ speakers = "A", topic = "x" })
check("generate degrades without binding", t1 == nil and e1 ~= nil)
local t2, e2 = aiwriter.continue_scene("*start\n[ch text=\"hi\"]\n")
check("continue degrades without binding", t2 == nil and e2 ~= nil)

-- ---------------------------------------------------------------------------
-- 2. sanitize: tags kept, prose commented
-- ---------------------------------------------------------------------------
local clean = aiwriter.sanitize_tags(
    '[ch name="A" text="hi"]\nSome prose line\n*label\n; comment\n[wait time=500]\n')
local hasTag = clean:find('[ch name="A" text="hi"]', 1, true) ~= nil
local hasLabel = clean:find("*label", 1, true) ~= nil
local hasWait = clean:find("[wait time=500]", 1, true) ~= nil
local proseCommented = clean:find("; (ai) Some prose line", 1, true) ~= nil
check("sanitize keeps ch tag", hasTag)
check("sanitize keeps label", hasLabel)
check("sanitize keeps wait tag", hasWait)
check("sanitize comments prose", proseCommented)
local empty = aiwriter.sanitize_tags("just prose only\n")
check("sanitize all-prose produces comment", empty:find("; (ai)", 1, true) ~= nil)
-- code-executing tags are blocked from AI output (review should-fix:
-- prompt injection could otherwise emit an iscript block)
local injected = aiwriter.sanitize_tags(
    '[ch text="ok"]\n[iscript]\nbackend.show_text("pwned")\n[/iscript]\n[endscript]\n[emb exp="os.exit(0)"]\n[eval exp="ctx.f.x = 1"]\n')
check("sanitize blocks iscript", injected:find("; (ai) blocked: [iscript]", 1, true) ~= nil)
check("sanitize blocks endscript", injected:find("blocked: [endscript]", 1, true) ~= nil)
check("sanitize blocks emb", injected:find("blocked: [emb", 1, true) ~= nil)
check("sanitize blocks eval", injected:find("blocked: [eval", 1, true) ~= nil)
check("sanitize keeps safe tags beside blocked", injected:find('[ch text="ok"]', 1, true) ~= nil)

-- ---------------------------------------------------------------------------
-- 3. generate_dialogue with mock LLM
-- ---------------------------------------------------------------------------
install_mock_ai()
mock_ai.response = '[ch name="Aoi" text="Hello!"]\n[ch name="Ryo" text="Hi."]\n'
local ok3, text3, err3 = true, aiwriter.generate_dialogue(
    { speakers = "Aoi, Ryo", topic = "greeting", lines = 2 })
check("generate returns tags", ok3 and type(text3) == "string"
      and text3:find("Aoi", 1, true) ~= nil)
check("generate passes system prompt", mock_ai.last_system ~= nil
      and mock_ai.last_system:find("KAG Neo-Genesis", 1, true) ~= nil)
check("generate prompt mentions speakers",
      mock_ai.last_prompt:find("Aoi, Ryo", 1, true) ~= nil)

-- mock error propagates
mock_ai.response = nil
local t4, e4 = aiwriter.generate_dialogue({ speakers = "A" })
check("generate mock error propagates", t4 == nil and e4 ~= nil)

-- ---------------------------------------------------------------------------
-- 4. continue_scene with mock LLM
-- ---------------------------------------------------------------------------
mock_ai.response = '[ch name="Aoi" text="And so it begins."]\n'
local t5, e5 = aiwriter.continue_scene("*start\n[ch text=\"hi\"]\n",
    { lines = 3 })
check("continue returns tags", type(t5) == "string"
      and t5:find("begins", 1, true) ~= nil)
check("continue prompt includes scene tail",
      mock_ai.last_prompt:find("current scene", 1, true) ~= nil
      and mock_ai.last_prompt:find("[ch text=\"hi\"]", 1, true) ~= nil)

-- ---------------------------------------------------------------------------
-- 5. json output for the IDE
-- ---------------------------------------------------------------------------
local j1 = aiwriter.json("available")
check("json available", j1:find('"text":"true"', 1, true) ~= nil)
mock_ai.response = '[ch name="A" text="x"]\n'
local j2 = aiwriter.json("generate_dialogue", { speakers = "A", topic = "t", lines = 1 })
check("json generate has text", j2:find('"text":', 1, true) ~= nil
      and j2:find("error", 1, true) ~= nil)

restore_ai()
-- Exit gate.
if failed > 0 then
    print(string.format("AIWRITER TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("AIWRITER TESTS DONE (%d passed)", passed))
