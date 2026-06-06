-- =============================================================================
--  Caesura (AmeKAG) — test_full_story_parse.lua
--  Parses full_story.ks and validates all P0+P1 KAG tags are present
-- =============================================================================

package.path = package.path .. ";scripts/?.lua;scripts/?/init.lua"

local function findScript(path)
    local candidates = { path, "tests/" .. path }
    for _, cand in ipairs(candidates) do
        local f = io.open(cand, "r")
        if f then f:close(); return cand end
    end
    return nil
end

local passed, failed = 0, 0

local function test(name, fn)
    local ok, err = pcall(fn)
    if ok then
        passed = passed + 1
        print("[PASS] " .. name)
    else
        failed = failed + 1
        print("[FAIL] " .. name .. " — " .. tostring(err))
    end
end

print("=== Caesura Alpha: full_story.ks Tag Coverage Test ===\n")

-- Test 1: Parse full_story.ks
test("Tokenizer parses full_story.ks without errors", function()
    local tokenizer = require("tokenizer")
    local path = "tests/scripts/full_story.ks"
    local tokens = tokenizer.parse_file(path)
    assert(tokens ~= nil, "parse_file returned nil")
    assert(#tokens > 0, "no tokens parsed")
    print("  Parsed " .. #tokens .. " tokens from " .. path)
end)

-- Test 2: Count tag coverage
test("All expected KAG tag types present", function()
    local tokenizer = require("tokenizer")
    local path = "tests/scripts/full_story.ks"
    local tokens = tokenizer.parse_file(path)

    local cmdCount = {}
    local labelCount = 0
    local commentCount = 0

    for _, tok in ipairs(tokens) do
        if tok.type == "command" then
            local cmd = tok.cmd
            cmdCount[cmd] = (cmdCount[cmd] or 0) + 1
        elseif tok.type == "text" and tok.content and tok.content:match("^%*") then
            labelCount = labelCount + 1
        end
    end

    -- All Layer tags
    local layerTags = {"bg", "fg", "cl", "image", "position", "layopt"}
    for _, tag in ipairs(layerTags) do
        assert(cmdCount[tag] and cmdCount[tag] > 0, "Missing layer tag: " .. tag)
    end

    -- All Text tags
    local textTags = {"ch", "text", "l", "r", "er", "p", "ruby", "font", "skip", "reset", "pt"}
    for _, tag in ipairs(textTags) do
        assert(cmdCount[tag] and cmdCount[tag] > 0, "Missing text tag: " .. tag)
    end

    -- All Audio tags
    local audioTags = {"playbgm", "fadebgm", "stopbgm", "playse", "stopse", "playvoice"}
    for _, tag in ipairs(audioTags) do
        assert(cmdCount[tag] and cmdCount[tag] > 0, "Missing audio tag: " .. tag)
    end

    -- All System tags
    local systemTags = {"wait", "emb", "eval", "history"}
    for _, tag in ipairs(systemTags) do
        assert(cmdCount[tag] and cmdCount[tag] > 0, "Missing system tag: " .. tag)
    end

    -- All Save tags
    local saveTags = {"save", "load", "saveplace", "loadplace"}
    for _, tag in ipairs(saveTags) do
        assert(cmdCount[tag] and cmdCount[tag] > 0, "Missing save tag: " .. tag)
    end

    -- All Transition tags
    local transTags = {"trans", "move", "quake", "fade"}
    for _, tag in ipairs(transTags) do
        assert(cmdCount[tag] and cmdCount[tag] > 0, "Missing transition tag: " .. tag)
    end

    -- Video
    assert(cmdCount["video"] and cmdCount["video"] > 0, "Missing video tag: video")
    assert(cmdCount["stopvideo"] and cmdCount["stopvideo"] > 0, "Missing video tag: stopvideo")

    -- VFX
    assert(cmdCount["vfx"] and cmdCount["vfx"] > 0, "Missing vfx tag: vfx")

    -- Preload
    assert(cmdCount["preload"] and cmdCount["preload"] > 0, "Missing preload tag: preload")

    -- Flow control
    local flowTags = {"jump", "call", "return", "if", "endif"}
    for _, tag in ipairs(flowTags) do
        assert(cmdCount[tag] and cmdCount[tag] > 0, "Missing flow tag: " .. tag)
    end
    assert(cmdCount["end"] and cmdCount["end"] > 0, "Missing flow tag: end")

    -- Labels
    assert(labelCount >= 10, "Expected >= 10 labels, got " .. labelCount)

    print("  Layer tags:   " .. table.concat(layerTags, ", "))
    print("  Text tags:    " .. table.concat(textTags, ", "))
    print("  Audio tags:   " .. table.concat(audioTags, ", "))
    print("  System tags:  " .. table.concat(systemTags, ", "))
    print("  Save tags:    " .. table.concat(saveTags, ", "))
    print("  Transition:   " .. table.concat(transTags, ", "))
    print("  Video/VFX:    video, stopvideo, vfx")
    print("  Resource:     preload")
    print("  Flow:         jump, call, return, if, endif, end")
    print("  Labels found: " .. labelCount)
    print("  Comments:     " .. commentCount)
end)

-- Test 3: Scheduler simulation
test("Scheduler can walk all tokens without errors", function()
    local tokenizer = require("tokenizer")
    local path = "tests/scripts/full_story.ks"
    local tokens = tokenizer.parse_file(path)

    -- Simulate ctx
    local ctx = {
        tokens = tokens,
        token_index = 1,
        call_stack = {},
        f = {}, sf = {}, tf = {},
        layers = {}, backlog = {},
        active_operations = {},
        stop_flag = false,
        input_focus = "kag",
        labelMap = {},
        skip_mode = false,
        text_state = { line = 1, char_offset = 0 },
    }

    -- Build label map (labels are text tokens starting with *)
    for i, tok in ipairs(tokens) do
        if tok.type == "text" and tok.content and tok.content:match("^%*(.+)") then
            ctx.labelMap[tok.content] = i
        end
    end

    -- Simulate dispatch (just verify commands exist in KAG table)
    local KAG = require("kag")
    local errors = 0
    local skipCommands = {jump=true, call=true, ["return"]=true, ["if"]=true,
                          endif=true, label=true}
    skipCommands["end"] = true

    for i = ctx.token_index, #ctx.tokens do
        local tok = ctx.tokens[i]
        if tok.type == "command" then
            local cmd = tok.cmd
            if not skipCommands[cmd] and KAG[cmd] == nil then
                print("  WARNING: No KAG handler for [" .. cmd .. "] at token " .. i)
                errors = errors + 1
            end
        end
    end

    assert(errors == 0, errors .. " commands have no KAG handler")
    print("  Walked " .. #tokens .. " tokens, 0 dispatch errors")
end)

-- Summary
print("\n" .. string.rep("=", 60))
print("  FULL STORY PARSE TEST RESULTS")
print(string.rep("=", 60))
print(string.format("  PASS:  %d", passed))
print(string.format("  FAIL:  %d", failed))
print(string.format("  TOTAL: %d", passed + failed))
print(string.rep("=", 60))

os.exit(failed > 0 and 1 or 0)


