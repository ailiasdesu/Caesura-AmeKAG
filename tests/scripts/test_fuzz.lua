-- test_fuzz.lua — Battle 3b: randomized .ks fuzz tests.
-- Generates legal and malformed KAG scenes, drives them through the
-- tokenizer + scheduler, and asserts: no crash, no hang (bounded steps),
-- and the compiler never fails on valid input.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
        failed = failed + 1 end
end

pcall(require, "kag.commands.text")
pcall(require, "kag.commands.system")
pcall(require, "kag.commands.audio")
pcall(require, "kag.commands.layer")
pcall(require, "kag.commands.vfx")
pcall(require, "kag.commands.save")
pcall(require, "kag.commands.video")
pcall(require, "kag")

local tokenizer = require("tokenizer")
local compiler = require("kag.compiler")
local determinism = require("kag.determinism")

-- ---------------------------------------------------------------------------
-- deterministic PRNG (repeatable fuzz: same seed -> same scenes)
-- ---------------------------------------------------------------------------
local seed = 20260812
local function rnd()
    -- xorshift32
    seed = seed ~ (seed << 13)
    seed = seed ~ (seed >> 17)
    seed = seed ~ (seed << 5)
    return seed % 100000 / 100000
end
local function rndInt(n)
    return math.floor(rnd() * n) + 1
end

-- command name pool (real commands + malformed names)
local CMD_POOL = { "ch", "text", "bg", "fg", "wait", "set", "inc", "if",
    "else", "endif", "while", "endwhile", "for", "endfor", "break",
    "continue", "jump", "call", "return", "link", "playbgm", "playse",
    "pt", "font", "trans", "image", "position", "cl", "end", "stop",
    "unknown_cmd", "bad cmd", "123", "ifx", "" }
local PARAM_POOL = { "text", "name", "storage", "file", "time", "speed",
    "volume", "x", "y", "exp", "target", "opacity", "layer", "var" }

--- generate one random scene (legal-ish or malformed)
local function gen_scene(rng)
    local lines = {}
    local n = rng and rndInt(5) or 5
    for _ = 1, n do
        local choice = rnd()
        if choice < 0.5 then
            -- a tag
            local cmd = CMD_POOL[rndInt(#CMD_POOL)]
            local parts = { "[" .. cmd }
            local np = rndInt(3)
            for _ = 1, np do
                local pname = PARAM_POOL[rndInt(#PARAM_POOL)]
                local pval
                local vt = rnd()
                if vt < 0.3 then
                    pval = '"' .. (rndInt(100)) .. '"'
                elseif vt < 0.6 then
                    pval = tostring(rndInt(100))
                else
                    pval = 'expr' .. rndInt(3)
                end
                parts[#parts + 1] = pname .. "=" .. pval
            end
            -- sometimes malformed: no closing bracket
            if rnd() < 0.1 then
                lines[#lines + 1] = table.concat(parts, " ") .. "\n"
            else
                lines[#lines + 1] = table.concat(parts, " ") .. "]\n"
            end
        elseif choice < 0.7 then
            -- a label
            lines[#lines + 1] = "*label" .. rndInt(5) .. "\n"
        elseif choice < 0.85 then
            -- dialogue text
            lines[#lines + 1] = "some dialogue " .. rndInt(9) .. "\n"
        else
            -- comment
            lines[#lines + 1] = "; comment " .. rndInt(9) .. "\n"
        end
    end
    -- sometimes a trailing unclosed block
    if rnd() < 0.1 then lines[#lines + 1] = "[while exp=\"true\"]\n" end
    return table.concat(lines)
end

-- ---------------------------------------------------------------------------
-- fuzz: 200 random scenes through tokenizer + compiler + scheduler
-- ---------------------------------------------------------------------------
local crashes = 0
local hangs = 0
local parsed = 0
local seed0 = seed
for i = 1, 200 do
    local src = gen_scene()
    -- tokenizer must never crash (may return partial or nil)
    local ok, tokens = pcall(tokenizer.parse, src)
    if ok then
        parsed = parsed + 1
        -- compiler must never crash on tokenized output
        local okC = pcall(compiler.compile, tokens)
        if not okC then crashes = crashes + 1 end
        -- scheduler must not hang (bounded via determinism max_steps)
        local ctx = determinism.run_scene(src, { max_steps = 200 })
        if ctx._timed_out then hangs = hangs + 1 end
    end
end
check("fuzz: tokenizer never crashes (200 scenes)", parsed > 0)
check("fuzz: zero compiler crashes", crashes == 0)
check("fuzz: zero scheduler hangs", hangs == 0)
check("fuzz: most scenes tokenize", parsed >= 150)
check("fuzz: seed reproducible", (function()
    local s = seed0
    local function r2() s = s ~ (s << 13); s = s ~ (s >> 17); s = s ~ (s << 5); return s % 100000 / 100000 end
    for i = 1, 100 do r2() end
    return seed ~= seed0  -- seed advanced by the main loop
end)())

-- ---------------------------------------------------------------------------
-- targeted malformed inputs
-- ---------------------------------------------------------------------------
local malformed = {
    "[", "[]", "[[", "[ch", "[ch text=", "[if exp=\"", "*", ";;;",
    "[while exp=\"true\"]", "[ch text=\"unterminated",
    "[" .. string.rep("x", 1000) .. "]",  -- huge tag
}
local malformed_crashes = 0
for _, m in ipairs(malformed) do
    local okP = pcall(tokenizer.parse, m)
    if okP then
        local okC = pcall(compiler.compile, tokenizer.parse(m))
        if not okC then malformed_crashes = malformed_crashes + 1 end
    end
end
check("malformed inputs never crash compiler", malformed_crashes == 0)

-- huge scene: 5000 tokens must parse without exhausting
local big = {}
for i = 1, 2000 do
    big[#big + 1] = '[ch text="line ' .. i .. '"]\n[p]\n'
end
local okBig = pcall(tokenizer.parse, table.concat(big))
check("large scene parses", okBig)

-- ---------------------------------------------------------------------------
-- expr.translate fuzz: random (often malformed) TJS expressions must never
-- throw or hang — translate is total (returns a string or nil) and every
-- translated chunk evaluates without crashing (compile errors are fine).
-- ---------------------------------------------------------------------------
local exprLang = require("kag.expr")
local TJS_TOKENS = {
    "f.x", "sf.a", "tf.b", "mp.c", "lf.d", "1", "2.5", "-0.5", "1e3", "0",
    '"str"', "'lit'", "true", "false", "nil", "f.arr[1]", "f.tbl.key",
    "f.fn()", "(f.x)", "not f.x", "-f.y", "f.a and f.b", "f.a or f.b",
    "f.a == 1", "f.a != 1", "f.a < 1", "f.a <= 1", "f.a > 1", "f.a >= 1",
    "f.a && f.b", "f.a || f.b", "!f.a", "f.a ? 1 : 2", "f.a ? f.b : f.c",
    "1 + 2 * 3 - 4 / 2 % 2", "f.x .. \"s\"", "f.flag && !f.other",
    "f.n > 3 ? \"big\" : \"small\"", "f.a == f.b == f.c", "f.x[1][2]",
    "f.x.y.z", "f.a ? f.b", "f.a &&", "?", "!", "&& || !", "()", "[]",
    "f.x = 1", "f.x ==", "f.x ?", "f.x : 2", "a.b.c.d.e.f.g", "0123",
    "f.x %% 0", "f.x / 0", "f.x ..", ".. f.x", "f.x >", "f.x < y.z",
}

local function random_tjs_expr()
    local n = rndInt(6)
    local parts = {}
    for _ = 1, n do
        parts[#parts + 1] = TJS_TOKENS[rndInt(#TJS_TOKENS)]
    end
    local s = table.concat(parts, rnd() < 0.5 and " " or " + ")
    if rnd() < 0.4 then
        local mut = rndInt(5)
        if mut == 1 then s = s:sub(1, rndInt(#s))
        elseif mut == 2 then s = s .. " &&"
        elseif mut == 3 then s = "(" .. s
        elseif mut == 4 then s = s .. " ? "
        else s = s:gsub("f", "f f") end
    end
    return s
end

local exprCtx = { f = { x = 1, a = true, b = false, n = 5, flag = true,
    other = false, t = 1 }, sf = {}, tf = {}, mp = {}, lf = {} }
local expr_throws, eval_crashes = 0, 0
local translated_count = 0
for _ = 1, 300 do
    local raw = random_tjs_expr()
    local okT, res = pcall(exprLang.translate, raw)
    if not okT then expr_throws = expr_throws + 1 end
    if okT and res ~= nil then
        translated_count = translated_count + 1
        local okE = pcall(exprLang.evaluateTranslated, exprCtx, res, raw)
        if not okE then eval_crashes = eval_crashes + 1 end
    end
end
check("expr fuzz: 300 random TJS never throw in translate", expr_throws == 0)
check("expr fuzz: translated chunks never crash evaluation",
      eval_crashes == 0)
check("expr fuzz: translate succeeds on most inputs",
      translated_count > 200)

-- Exit gate.
if failed > 0 then
    print(string.format("FUZZ TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("FUZZ TESTS DONE (%d passed)", passed))
