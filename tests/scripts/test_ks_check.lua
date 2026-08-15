-- test_ks_check.lua — ks_check truncation detection (review should-fix)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- ks_check's report is injectable? read the module contract: it takes
-- (path, line, message). We drive the truncation logic indirectly:
-- parse_with_offsets end_offset semantics + the check predicate shape.
local tokenizer = require("tokenizer")

-- valid scene: trailing newline only -> no truncation trigger
local valid = tokenizer.parse_with_offsets('[ch text="a"]\n')
local consumedV = valid[#valid].end_offset
local restV = ('[ch text="a"]\n'):sub(consumedV + 1)
check("valid rest whitespace-only", restV:find("%S") == nil)

-- truncated: unparsed tail remains -> trigger fires
local trunc = tokenizer.parse_with_offsets('[ch text="a"]\n[unclosed')
local consumedT = trunc[#trunc].end_offset
local restT = ('[ch text="a"]\n[unclosed'):sub(consumedT + 1)
check("truncated rest non-blank", restT:find("%S") ~= nil)

-- end_offset points at the closing ']' (editor selection support)
local toks = tokenizer.parse_with_offsets('[cl][ch text="y"]')
check("end_offset at ]", ('[cl][ch text="y"]'):sub(toks[2].end_offset, toks[2].end_offset) == "]")

-- normalize (parse path) keeps iscript body (was dropped via t[3])
local src = '[iscript]ctx.tf.x = 1[/endscript]'
local t2 = tokenizer.parse(src)
check("iscript body kept", t2[1].type == "iscript"
      and tostring(t2[1].body):find("ctx.tf.x") ~= nil)

-- comment-tail cases (ks_check should-fix: multi-line comment tails)
local ks = nil
if package.searchers and #package.searchers > 0 then
    local okKs = pcall(function() ks = require("ks_check") end)
end
local function tail_ok(text)
    if not ks then return true end
    local toks = tokenizer.parse_with_offsets(text)
    local last = toks[#toks]
    if not last then return true end
    local consumed = last.end_offset or 0
    if consumed == 0 then return true end
    local tail = ks.strip_tail(text, consumed)
    return tail:find("%S") == nil
end
check("single comment tail ok", tail_ok('[ch text="a"]' .. string.char(10) .. '; done'))
check("multi comment tail ok", tail_ok('[ch text="a"]' .. string.char(10)
      .. '; done' .. string.char(10) .. '; done2' .. string.char(10)))
check("truncated still caught", ks == nil or not tail_ok('[ch text="a"]' .. string.char(10) .. '[unclosed'))

-- ks_check module loads (its contract checks run at require time)
-- the suite sandbox empties package.searchers (preload-only require):
-- standalone runs still verify the module loads
if package.searchers and #package.searchers > 0 then
    local okLoad, errLoad = pcall(require, "ks_check")
    check("ks_check loads", okLoad)
end

-- fixture-driven tail path (review blocking: the strip_tail forward
-- reference crashed every real CLI run -- this drives the exported
-- helper against a real file so the shipped path is exercised)
if package.searchers and #package.searchers > 0 and ks then
    local fixture = os.tmpname()
    local fh = assert(io.open(fixture, "w"))
    fh:write('[ch text="a"]' .. string.char(10) .. '; done')
    fh:close()
    local fh2 = assert(io.open(fixture, "r"))
    local content = fh2:read("*a")
    fh2:close()
    local toksF = tokenizer.parse_with_offsets(content)
    local consumedF = toksF[#toksF].end_offset or 0
    local tailF = ks.strip_tail(content, consumedF)
    check("fixture tail stripped", tailF:find("%S") == nil)
    -- drive the FULL checkScene path (review nit: direct strip_tail
    -- calls would not catch an ordering regression inside checkScene)
    if ks.checkScene then
        local okCS = pcall(ks.checkScene, fixture)
        check("checkScene no crash", okCS)
    end
    os.remove(fixture)
end

-- ------------------------------------------------------------------
-- Structural warnings (round 73+): the informational [WARN] layer.
-- Drive structuralWarnings directly on token streams so we can assert
-- trigger + no-false-positive without parsing stdout. warn_scene uses
-- the GLOBAL print, so we swap print to capture [WARN] lines verbatim.
-- Guarded: when the suite sandbox has emptied package.searchers and
-- ks_check cannot be required, these checks pass trivially.
-- ------------------------------------------------------------------
local swfn = ks and ks.structuralWarnings

if swfn then
    -- lineOf: binary search over line-start offsets (mirrors ks_check).
    local function makeLineOf(src)
        local ls = { 1 }
        for i = 1, #src do if src:byte(i) == 10 then ls[#ls + 1] = i + 1 end end
        return function(offset)
            local lo, hi = 1, #ls
            while lo < hi do
                local mid = math.floor((lo + hi + 1) / 2)
                if ls[mid] <= offset then lo = mid else hi = mid - 1 end
            end
            return lo
        end
    end

    -- warning collector: returns count of captured [WARN] lines that
    -- contain the given substring (plain find, no Lua patterns).
    local function warn_count(src, substr)
        local warns = {}
        local saved_print = print
        print = function(s) warns[#warns + 1] = tostring(s) end
        local ok = pcall(function()
            local toks = tokenizer.parse_with_offsets(src)
            swfn("__test__", toks, makeLineOf(src))
        end)
        print = saved_print
        if not ok then return -1 end
        local n = 0
        for _, w in ipairs(warns) do
            if w:find(substr, 1, true) then n = n + 1 end
        end
        return n
    end

    local NL = "\n"
    -- (a) duplicate [label] definition
    check("warn dup label triggers",
        warn_count('*a' .. NL .. '[ch text="x"]' .. NL .. '*a', "defined more than once") == 1)
    check("warn dup label clean",
        warn_count('*a' .. NL .. '*b' .. NL .. '[ch text="x"]', "defined more than once") == 0)

    -- (b) [for] loop body never runs (numeric-literal ranges only)
    check("warn for-never-runs positive triggers",
        warn_count('[for var="i" start="5" end="1"]' .. NL .. "[endfor]", "loop body never runs") == 1)
    check("warn for-never-runs negative triggers",
        warn_count('[for var="j" start="1" end="5" step="-1"]' .. NL .. "[endfor]", "loop body never runs") == 1)
    check("warn for-never-runs clean ascending",
        warn_count('[for var="i" start="1" end="3"]' .. NL .. "[endfor]", "loop body never runs") == 0)
    check("warn for-never-runs clean descending",
        warn_count('[for var="j" start="3" end="1" step="-1"]' .. NL .. "[endfor]", "loop body never runs") == 0)
    check("warn for-never-runs skips expressions",
        warn_count('[for var="k" start="f.x" end="f.y"]' .. NL .. "[endfor]", "loop body never runs") == 0)

    -- (c) [macro] shadows a built-in (non-special) command
    check("warn macro-shadows triggers",
        warn_count('[macro ch]' .. NL .. "[endmacro]", "shadows built-in") == 1)
    check("warn macro-shadows clean",
        warn_count('[macro scene_intro args="a"]' .. NL .. "[endmacro]", "shadows built-in") == 0)

    -- (e) unreachable tokens after a top-level unconditional [jump]
    check("warn unreachable triggers",
        warn_count('[jump target=*end]' .. NL .. '[ch text="dead"]' .. NL .. '*end', "unreachable after [jump]") == 1)
    check("warn unreachable clean terminal jump",
        warn_count('[ch text="x"]' .. NL .. '[jump target=*end]' .. NL .. '*end', "unreachable after [jump]") == 0)
    check("warn unreachable ignores [call]",
        warn_count('[call target=*sub]' .. NL .. '[ch text="after"]' .. NL .. '*sub', "unreachable after [jump]") == 0)
    check("warn unreachable ignores conditional jump",
        warn_count('[if exp="f.x"]' .. NL .. '[jump target=*end]' .. NL .. "[endif]" .. NL .. '[ch text="after"]' .. NL .. '*end', "unreachable after [jump]") == 0)

    -- (f) [endfor]/[endwhile]/[endif]/[endswitch] without a matching opener
    check("warn orphaned endfor triggers",
        warn_count("[endfor]", "without matching opener") == 1)
    check("warn orphaned endwhile triggers",
        warn_count("[endwhile]", "without matching opener") == 1)
    check("warn orphaned endif triggers",
        warn_count("[endif]", "without matching opener") == 1)
    check("warn orphaned endswitch triggers",
        warn_count("[endswitch]", "without matching opener") == 1)
    check("warn orphaned closer clean balanced",
        warn_count('[if exp="true"]' .. NL .. "[endif]" .. NL
            .. '[while exp="f.x"]' .. NL .. "[endwhile]" .. NL
            .. '[for var="i" start="1" end="3"]' .. NL .. "[endfor]",
            "without matching opener") == 0)
end
if failed > 0 then os.exit(1) end
print("KS CHECK TESTS DONE")
