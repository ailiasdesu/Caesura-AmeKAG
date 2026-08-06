-- test_multiline.lua — multi-line text blocks (""" ... """)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local tokenizer = require("tokenizer")
local scheduler = require("scheduler")
require("kag.commands.text")

-- ---- tokenizer: block parses as one text token with newlines --------------
do
    local toks = tokenizer.parse([[
[ch text="before"]
"""
Line one.
Line two.
"""
[ch text="after"]
]])
    local texts = {}
    for _, t in ipairs(toks) do
        if t.type == "text" then texts[#texts + 1] = t.content end
    end
    check("block yields one text token", #texts == 1, #texts)
    check("block keeps interior newline",
        texts[1] == "Line one.\nLine two.", string.format("%q", texts[1]))
end

-- ---- scheduler: block renders as one [ch] with the full text --------------
do
    local dispatched = {}
    local kag_orig = package.loaded["kag"]
    local kag = { ch = function(c2, p2) dispatched[#dispatched + 1] = { "ch", p2 } end }
    package.loaded["kag"] = kag
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "ml.ks", token_index = 1, stop_flag = false }
    local tokens = tokenizer.parse([[
"""
First paragraph line.
Second paragraph line.
"""
]])
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    check("block dispatches as single ch",
        #dispatched == 1 and dispatched[1][1] == "ch", #dispatched)
    check("ch text contains both lines with newline",
        dispatched[1] and dispatched[1][2].text
            == "First paragraph line.\nSecond paragraph line.",
        dispatched[1] and string.format("%q", dispatched[1][2].text))
end

-- ---- block + interpolation ($f.name) ---------------------------------------
do
    local dispatched = {}
    local kag_orig = package.loaded["kag"]
    local kag = { ch = function(c2, p2) dispatched[#dispatched + 1] = { "ch", p2 } end }
    package.loaded["kag"] = kag
    local ctx = { f = { name = "Aoi" }, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "ml2.ks", token_index = 1, stop_flag = false }
    local tokens = tokenizer.parse([[
"""
Hello, $f.name.
Welcome back.
"""
]])
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    check("block text interpolates variables",
        dispatched[1] and dispatched[1][2].text
            == "Hello, Aoi.\nWelcome back.",
        dispatched[1] and string.format("%q", dispatched[1][2].text))
end

-- ---- block delimiters inside ordinary text stay literal -------------------
do
    local toks = tokenizer.parse([[He said """hello""" to me.]])
    local texts = {}
    for _, t in ipairs(toks) do
        if t.type == "text" then texts[#texts + 1] = t.content end
    end
    -- Block markers only take effect at a token boundary (line start or
    -- after a command); inside a running text line they are literal.
    check("inline quotes stay literal in text",
        #texts == 1 and texts[1] == 'He said """hello""" to me.',
        table.concat(texts, " | "))
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("MULTILINE TESTS DONE")
