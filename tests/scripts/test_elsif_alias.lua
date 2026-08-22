-- =============================================================================
--  test_elsif_alias.lua — KAG3 [elsif] alias consistency across BOTH parse
--  paths (P1 toolchain fix, round 122):
--    * tokenizer.parse        -> normalize() applied aliases all along;
--    * tokenizer.parse_with_offsets  -> DID NOT, so ks_check/LSP flagged
--      valid [elsif] scenes "unknown KAG command" while the runtime
--      scheduler executed them fine (scheduler.lua normalizes defensively).
--  Fix: parse_with_offsets routes command names through
--  tokenizer.normalize_cmd (string-only rewrite; Cp()-derived offsets are
--  untouched). docs/compatibility.md documents [elsif] as a KAG3 alias of
--  [elseif].
-- =============================================================================
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local tokenizer = require("tokenizer")

-- ---------------------------------------------------------------------------
-- A. parse_with_offsets: alias normalization + byte-offset accuracy
-- ---------------------------------------------------------------------------
local tag = '[elsif exp="f.score > 70"]'
local toks = tokenizer.parse_with_offsets(tag)
check("A1: one token", #toks == 1)
check("A2: cmd normalized to elseif", toks[1] and toks[1].cmd == "elseif")
check("A3: type is command", toks[1] and toks[1].type == "command")
check("A4: exp param preserved",
      toks[1] and toks[1].params and toks[1].params[1]
      and toks[1].params[1][1] == "exp"
      and toks[1].params[1][2] == "f.score > 70")
check("A5: offset at first byte", toks[1] and toks[1].offset == 1)
check("A6: end_offset covers whole tag",
      toks[1] and toks[1].end_offset == #tag)

-- Canonical spelling passes through unchanged (no double-mapping surprises)
local canon = tokenizer.parse_with_offsets('[elseif exp="x"]')
check("A7: canonical elseif unchanged", canon[1] and canon[1].cmd == "elseif")

-- Chain scene: every flow keyword keeps its own exact '[' byte position,
-- elsif lands between if and else in the normalized command stream.
local src = '[if exp="f.score > 90"]\n[ch text="great"]\n[elsif exp="f.score > 70"]\n[ch text="good"]\n[else]\n[ch text="low"]\n[endif]\n[ch text="after"]'
local chain = tokenizer.parse_with_offsets(src)
local cmds = {}
for _, t in ipairs(chain) do cmds[#cmds + 1] = t.cmd end
check("A8: chain cmd sequence normalized",
      table.concat(cmds, ",") == "if,ch,elseif,ch,else,ch,endif,ch")

local off_ok = true
for _, t in ipairs(chain) do
    if t.type == "command" then
        -- Command offsets point at their own '[' byte (text tokens start at
        -- their first character by design); end_offset must cover forward.
        if src:sub(t.offset, t.offset) ~= "[" then off_ok = false end
        if t.end_offset == nil or t.end_offset < t.offset then off_ok = false end
    end
end
check("A9: every command offset points at its '[' byte", off_ok)

local els = nil
for _, t in ipairs(chain) do if t.cmd == "elseif" then els = t break end end
check("A10: elsif offset matches source position",
      els ~= nil and els.offset == src:find("[elsif", 1, true))

-- Both parse paths agree (toolchain-consistency invariant)
local via_parse = tokenizer.parse(src)
local pcmds = {}
for _, t in ipairs(via_parse) do
    if t.type == "command" then pcmds[#pcmds + 1] = t.cmd end
end
check("A11: parse() and parse_with_offsets() agree",
      table.concat(pcmds, ",") == table.concat(cmds, ","))

-- ---------------------------------------------------------------------------
-- B. Runtime: scheduler executes an [elsif] chain tokenized WITH OFFSETS
--    (production shape: parse_with_offsets -> array tokens -> scheduler.run;
--    kag.compiler.to_array_tok performs the same record->array mapping).
-- ---------------------------------------------------------------------------
local scheduler = require("scheduler")
local function runScene(score)
    local recs = tokenizer.parse_with_offsets(src)
    local tokens = {}
    for _, t in ipairs(recs) do tokens[#tokens + 1] = { t.cmd, t.params or {} } end
    local d = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) d[#d + 1] = { k, p2 } end
    end})
    local ctx = { f = { score = score }, tf = {}, sf = {}, mp = {},
        macros = nil, macro_args = nil, current_scene = "t.ks", token_index = 1 }
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    return d
end

local d95 = runScene(95)
check("B1: score 95 -> if branch (great)", d95[1] and d95[1][2].text == "great")
check("B2: chain terminates after if branch", d95[2] and d95[2][2].text == "after")
local d80 = runScene(80)
check("B3: score 80 -> elsif branch (good)", d80[1] and d80[1][2].text == "good")
check("B4: score 80 skips later branches", d80[2] and d80[2][2].text == "after")
local d30 = runScene(30)
check("B5: score 30 -> else branch (low)", d30[1] and d30[1][2].text == "low")

-- ---------------------------------------------------------------------------
-- C. ks_check CLI: valid [elsif] scene exits clean; typo control still fails
--    (guards against a false-green from a silently-failed subprocess).
-- ---------------------------------------------------------------------------
local SEP = package.config:sub(1, 1)
local IS_WIN = SEP == "\\"
os.execute(IS_WIN and 'mkdir "tmp\\test_elsif_alias" 2>nul'
                    or 'mkdir -p "tmp/test_elsif_alias"')

local function writeScene(rel, body)
    local f = io.open(rel, "w")
    if not f then return false end
    f:write(body)
    f:close()
    return true
end
local CLEAN = "tmp/test_elsif_alias/cli_clean.ks"
local BAD   = "tmp/test_elsif_alias/cli_typo.ks"
local okC = writeScene(CLEAN,
    '*start\n[if exp="f.score > 90"]\n[ch text="great"]\n' ..
    '[elsif exp="f.score > 70"]\n[ch text="good"]\n[else]\n' ..
    '[ch text="low"]\n[endif]\n[end]\n')
local okB = writeScene(BAD,
    '*start\n[ch text="hi"]\n[wiat time="500"]\n[end]\n')

if okC and okB then
    -- locate the interpreter (vendored lua first), wrap Windows paths --
    -- mirrors test_ks_i18n_flow.lua's wrapCmd (parens-safe call "...").
    local SAVE_ARG = _G.arg
    local cands = {}
    if type(SAVE_ARG) == "table" and type(SAVE_ARG[-1]) == "string"
        and #SAVE_ARG[-1] > 0 then cands[#cands + 1] = SAVE_ARG[-1] end
    for _, c in ipairs({ "external" .. SEP .. "lua" .. SEP .. "lua.exe",
                         "external" .. SEP .. "lua" .. SEP .. "lua" }) do
        cands[#cands + 1] = c
    end
    cands[#cands + 1] = IS_WIN and "lua.exe" or "lua5.4"
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

    local rep = nil
    local pf = io.popen(wrapCmd(' scripts/ks_check.lua "' .. CLEAN .. '"'))
    if pf then rep = pf:read("*a"); local codeOK = pf:close(); rep = (rep or "") .. "\x01EXIT_" .. tostring(codeOK == true) end
    _G.arg = SAVE_ARG
    if rep then
        check("C1: elsif scene: no unknown-command report",
              not rep:find("unknown KAG command", 1, true))
        check("C2: elsif scene: ks_check exit 0", rep:find("\x01EXIT_true", 1, true) ~= nil)
    else
        print("SKIP C1/C2: no LUA interpreter located")
    end

    local rep2 = nil
    _G.arg = SAVE_ARG
    local pf2 = io.popen(wrapCmd(' scripts/ks_check.lua "' .. BAD .. '"'))
    if pf2 then rep2 = pf2:read("*a"); pf2:close() end
    _G.arg = SAVE_ARG
    if rep2 ~= nil then
        check("C3: typo control [wiat] still flagged",
              rep2:find("unknown KAG command 'wiat'", 1, true) ~= nil)
    else
        print("SKIP C3: no LUA interpreter located")
    end
else
    print("SKIP C1-C3: temp scene dir unavailable")
end

-- best-effort cleanup of the temp scenes (keep tmp/ dir; other suites use it too)
os.remove(CLEAN)
os.remove(BAD)

print(string.format("ELSIF ALIAS TESTS: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
print("ELSIF ALIAS TESTS DONE")
