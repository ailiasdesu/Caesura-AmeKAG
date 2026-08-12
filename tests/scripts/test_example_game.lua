-- test_example_game.lua — example game structural regression tests
-- Verifies demo/example_game/story.ks stays valid: tokenizes, all jump
-- targets resolve, macro definitions intact, choices reachable, and the
-- compiled stream carries the full label index.
--
-- NOTE: tokenizer.parse returns RECORD format ({type=,cmd=,params=});
-- compiler.compile rewrites the array in place to ARRAY format
-- ({cmd, params}) -- structure checks run on the record format FIRST,
-- then compile() for the label-index / expression checks.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local tokenizer = require("tokenizer")
local compiler = require("kag.compiler")

local PATH = "demo/example_game/story.ks"
local f = io.open(PATH, "r")
if not f then
    print("FAIL cannot open " .. PATH)
    os.exit(1)
end
local src = f:read("*a")
f:close()

-- 1. tokenizes cleanly (record format)
local toks = tokenizer.parse(src)
check("story.ks tokenizes", type(toks) == "table" and #toks > 100)

-- 2. macro definition present (scene_intro) -- record format check
local macro_found = false
for _, t in ipairs(toks) do
    if t.type == "command" and t.cmd == "macro" then
        local name = nil
        for _, pair in ipairs(t.params or {}) do
            if type(pair) == "table" then
                if pair[1] == "1" or pair[1] == 1 then name = pair[2]
                elseif pair[1] == "name" then name = pair[2] end
            end
        end
        if name == "scene_intro" then macro_found = true end
    end
end
check("scene_intro macro defined", macro_found)

-- 3. choices present and targets resolve (record format)
local sel_targets = {}
for _, t in ipairs(toks) do
    if t.type == "command" and (t.cmd == "sel" or t.cmd == "button") then
        for _, pair in ipairs(t.params or {}) do
            if type(pair) == "table" and pair[1] == "target"
                and type(pair[2]) == "string" and pair[2]:sub(1, 1) == "*" then
                sel_targets[#sel_targets + 1] = pair[2]:sub(2)
            end
        end
    end
end
check("choices present", #sel_targets >= 2)

-- 4. ending unlocks present (gallery integration)
local ending_ids = {}
for _, t in ipairs(toks) do
    if t.type == "command" and t.cmd == "ending" then
        for _, pair in ipairs(t.params or {}) do
            if type(pair) == "table" and pair[1] == "id" then
                ending_ids[#ending_ids + 1] = pair[2]
            end
        end
    end
end
check("three endings declared", #ending_ids == 3)

-- 5. expression syntax compiles (TJS && in [if]) -- record format
local exprLang = require("kag.expr")
local expr_ok = true
for _, t in ipairs(toks) do
    if t.type == "command" and t.cmd == "if" then
        local exp = nil
        for _, pair in ipairs(t.params or {}) do
            if type(pair) == "table" and pair[1] == "exp" then exp = pair[2] end
        end
        if type(exp) == "string" then
            local ok = pcall(function()
                local fn = load("return " .. exprLang.translate(exp),
                                "=t", "t", {})
                assert(fn)
            end)
            if not ok then expr_ok = false end
        end
    end
end
check("all [if] expressions compile", expr_ok)

-- 6. every intra-scene [jump target=*x] resolves -- needs the label index,
--    so compile FIRST (rewrites to array format) then check jumps by
--    scanning the RECORD list captured before compile.
local jumps = {}
for _, t in ipairs(toks) do
    if t.type == "command" and t.cmd == "jump" then
        local target = nil
        for _, pair in ipairs(t.params or {}) do
            if type(pair) == "table" and pair[1] == "target" then
                target = pair[2]
            end
        end
        if type(target) == "string" and target:sub(1, 1) == "*" then
            jumps[#jumps + 1] = target:sub(2)
        end
    end
end

compiler.compile(toks)
local labels = toks._compiled.labels
check("all labels indexed", labels.route_library and labels.route_rooftop
      and labels.route_gate and labels.investigate and labels.leave_library
      and labels.ending_good and labels.ending_normal and labels.ending_bad
      and labels.credits ~= nil)

local unresolved = {}
for _, name in ipairs(jumps) do
    if not labels[name] then unresolved[#unresolved + 1] = name end
end
check("all intra-scene jump targets resolve", #unresolved == 0)

local all_resolve = true
for _, name in ipairs(sel_targets) do
    if not labels[name] then all_resolve = false end
end
check("all choice targets resolve", all_resolve)

-- 7. no unknown commands (all handler-backed or macros or flow)
local KNOWN = {}
local kag_ok = pcall(require, "kag")
if kag_ok then
    local kag_tbl = package.loaded["kag"]
    for k in pairs(kag_tbl) do
        if type(k) == "string" then KNOWN[k] = true end
    end
end
local schema = require("kag.schema")
pcall(require, "kag.commands.text")
pcall(require, "kag.commands.system")
pcall(require, "kag.commands.audio")
pcall(require, "kag.commands.transition")
pcall(require, "kag.commands.layer")
pcall(require, "kag.commands.vfx")
pcall(require, "kag.commands.video")
pcall(require, "kag.commands.save")
local FLOW = { ["if"] = true, ["else"] = true, ["endif"] = true,
    ["while"] = true, ["endwhile"] = true, ["for"] = true, ["endfor"] = true,
    ["break"] = true, ["continue"] = true, ["jump"] = true, ["call"] = true,
    ["return"] = true, ["label"] = true, ["macro"] = true, ["endmacro"] = true,
    ["erasemacro"] = true, ["eval"] = true, ["emb"] = true, ["iscript"] = true,
    ["select"] = true, ["sel"] = true, ["endselect"] = true, ["link"] = true,
    ["end"] = true, ["stop"] = true, ["switch"] = true, ["endswitch"] = true,
    ["case"] = true, ["endcase"] = true, ["default"] = true,
    ["scene_intro"] = true }
local unknown = {}
for _, t in ipairs(toks) do
    local cmd = t[1] or (t.type == "command" and t.cmd)
    if cmd and type(cmd) == "string" then
        if not KNOWN[cmd] and not schema.isMigrated(cmd) and not FLOW[cmd] then
            unknown[#unknown + 1] = cmd
        end
    end
end
check("no unknown commands", #unknown == 0)

if failed > 0 then
    print(string.format("EXAMPLE GAME TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("EXAMPLE GAME TESTS DONE (%d passed)", passed))
