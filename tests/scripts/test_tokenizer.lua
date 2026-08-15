-- =============================================================================
--  test_tokenizer.lua — Tokenizer unit tests
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local tokenizer = require("tokenizer")

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
        print(string.format("  [PASS] %s", name))
    else
        failed = failed + 1
        print(string.format("  [FAIL] %s  -- %s", name, detail or ""))
    end
end

print("\n=== Tokenizer Tests ===\n")

-- 1. Basic tag parsing
do
    local tokens = tokenizer.parse("[bg]")
    check("bg tag count", #tokens == 1, "got " .. #tokens)
    if #tokens > 0 then
        check("bg type", tokens[1].type == "command")
        check("bg cmd name", tokens[1].cmd == "bg")
    end
end

-- 2. Tag with params
do
    local tokens = tokenizer.parse("[bg storage=test]")
    check("bg with param", #tokens == 1)
    if #tokens > 0 then
        check("param present", tokens[1].params ~= nil)
    end
end

-- 3. Text chunk
do
    local tokens = tokenizer.parse("Hello World")
    check("text parsed", #tokens >= 1)
end

-- 4. Multiple tags
do
    local tokens = tokenizer.parse("[bg][fg]")
    check("multi-tag count", #tokens == 2)
end

-- 5. Label
do
    local tokens = tokenizer.parse("*label_name")
    check("label parsed", #tokens == 1)
end

-- 6. If/endif
do
    local tokens = tokenizer.parse("[if exp='true'][endif]")
    check("if-endif token count", #tokens >= 2)
end

-- 7. Macro
do
    local tokens = tokenizer.parse("[macro name=test][endmacro]")
    check("macro token count", #tokens == 2)
end

-- Batch A token tests
local tokens_se = tokenizer.parse("[se file=\"click.wav\" loop=0]")
local tokens_stopse = tokenizer.parse("[stopse]")
local tokens_wait = tokenizer.parse("[wait time=500]")

-- REGRESSION (C++ demo e2e): the explicit P_se prefix must NOT shadow
-- longer command names once bare-value params exist
local t_setbgm = tokenizer.parse("[setbgmvolume volume=0.6]")
check("setbgmvolume distinct", t_setbgm[1].cmd == "setbgmvolume")
local t_waitbgm = tokenizer.parse("[waitbgm]")
check("waitbgm distinct", t_waitbgm[1].cmd == "waitbgm")
local t_setse = tokenizer.parse("[setsevolume v=1]")
check("setsevolume distinct", t_setse[1].cmd == "setsevolume")

check("se tag parsed", tokens_se[1].cmd == "se")
check("se file param present",
      type(tokens_se[1].params) == "table" and tokens_se[1].params[1][2] == "click.wav")
check("stopse tag parsed", tokens_stopse[1].cmd == "stopse")
check("fadebgm tag parsed", tokenizer.parse("[fadebgm time=1000]")[1].cmd == "fadebgm")
check("fadese tag parsed", tokenizer.parse("[fadese 200]")[1].cmd == "fadese")
check("wait tag parsed", tokens_wait[1].cmd == "wait")
check("wait time param present", tokens_wait[1].params[1][2] == "500")
check("delay tag parsed", tokenizer.parse("[delay 500]")[1].cmd == "delay")
check("skip tag parsed", tokenizer.parse("[skip]")[1].cmd == "skip")

-- Error reporting: parse failure carries a line number (binary search)
do
    local ok, err = pcall(function() tokenizer.parse("[") end)
    if ok then failed = failed + 1 else passed = passed + 1 print("  [PASS] parse failure raises") end
    if not ok and tostring(err):find("line") then passed = passed + 1 print("  [PASS] parse failure has line") else failed = failed + 1 end
end

print(string.format("\nResults: %d passed, %d failed", passed, failed))
-- REGRESSION (lpeg ^1 batch zero-width guard, review should-fix):
-- [cmd k=v ] must NOT gain an empty bare param; [cmd x=] falls to the
-- bare-value shape, never an empty named value
local t_trail = tokenizer.parse("[cmd k=v ]")
check("trailing-space param count", #t_trail[1].params == 1
      and t_trail[1].params[1][1] == "k" and t_trail[1].params[1][2] == "v")
local t_empty = tokenizer.parse("[cmd x=]")
check("empty-value falls to bare", t_empty[1].params[1][1] == "1"
      and t_empty[1].params[1][2] == "x=")
local t_q = tokenizer.parse('[cmd k=""]')
check("quoted empty kept", t_q[1].params[1][1] == "k"
-- (the Results line above prints before these regression checks;
-- the exit-code guard after them is authoritative -- review nit)
      and t_q[1].params[1][2] == "")

-- ---- Neo-Genesis block text (triple-quote) --------------------------------
do
    local toks = tokenizer.parse('"""line1\nline2\n"""')
    check("blocktext is a text token", toks[1].type == "text")
    check("blocktext content preserved",
          toks[1].content == "line1\nline2")
end

do
    local toks = tokenizer.parse('[ch text="a"]\n"""multi\nline\ntext"""\n[ch text="b"]')
    check("blocktext between commands", #toks == 3
          and toks[1].cmd == "ch" and toks[2].type == "text"
          and toks[3].cmd == "ch")
    check("blocktext multi-line content",
          toks[2].content == "multi\nline\ntext")
end

-- ---- comments ---------------------------------------------------------------
do
    local toks = tokenizer.parse('; header comment\n[ch text="x"]')
    check("leading comment skipped", #toks == 1 and toks[1].cmd == "ch")
    local toks2 = tokenizer.parse('[ch text="a"] ; inline note\n[ch text="b"]')
    check("inline comment skipped", #toks2 == 2
          and toks2[1].cmd == "ch" and toks2[2].cmd == "ch")
end

-- ---- comment-only / whitespace-only robustness (round 88 edge) --------------
-- A .ks file containing ONLY comments and/or whitespace/newlines must parse
-- to an empty token table ({}), exactly like the empty string -- never a
-- "Tokenizer: parse failed" error.
do
    local function isEmpty(input)
        local ok, res = pcall(tokenizer.parse, input)
        return ok and type(res) == "table" and #res == 0, res
    end
    local ok; ok, _ = isEmpty("; only a comment")
    check("comment-only returns empty {}", ok)
    ok, _ = isEmpty("; line1\n; line2\n")
    check("multi-comment-only returns empty {}", ok)
    ok, _ = isEmpty("   \n\t ")
    check("whitespace-only returns empty {}", ok)
    ok, _ = isEmpty("\n\n\n")
    check("blank-lines-only returns empty {}", ok)
    ok, _ = isEmpty("  ; comment\n  \n; more\n\t")
    check("comment+whitespace mix returns empty {}", ok)
end

-- ---- iscript raw region -----------------------------------------------------
do
    local toks = tokenizer.parse('[iscript]\nlocal x = 1\n-- [not a tag]\n[/endscript]')
    check("iscript single token", #toks == 1 and toks[1].type == "iscript")
    check("iscript body is raw (tags not tokenized)",
          toks[1].body:find("[not a tag]", 1, true) ~= nil)
end

-- ---- labels and mixed streams ----------------------------------------------
do
    local toks = tokenizer.parse('*start\nHello world\n[ch text="hi"]')
    check("label token carries name", toks[1].type == "label"
          and toks[1].name == "start")
    check("mixed stream token order", #toks == 3
          and toks[2].type == "text" and toks[3].cmd == "ch")
end

-- ---- robustness -------------------------------------------------------------
do
    local ok = pcall(tokenizer.parse, "[]")
    check("empty command raises (visible error)", not ok)
    local ok2 = pcall(tokenizer.parse, "[")
    check("unclosed command raises", not ok2)
end

do
    local toks = tokenizer.parse('[ch text="a [b] c"]')
    check("brackets inside quoted value", #toks == 1 and toks[1].cmd == "ch")
    local toks2 = tokenizer.parse('[se file="a=b.wav"]')
    check("equals inside quoted value", #toks2 == 1 and toks2[1].cmd == "se")
end

do
    local toks = tokenizer.parse('\239\187\191[bg]')
    check("BOM prefix tolerated", #toks == 1 and toks[1].cmd == "bg")
    local toks2 = tokenizer.parse('[bg]\r\n[fg]')
    check("CRLF line endings", #toks2 == 2
          and toks2[1].cmd == "bg" and toks2[2].cmd == "fg")
    local toks3 = tokenizer.parse('こんにちは世界')
    check("unicode text content preserved", toks3[1].type == "text"
          and toks3[1].content == "こんにちは世界")
end

-- ---- unclosed regions degrade gracefully (no crash) -------------------------
do
    local ok, toks = pcall(tokenizer.parse, '[iscript]\nlocal x = 1')
    check("unclosed iscript does not crash", ok)
    local ok2, toks2 = pcall(tokenizer.parse, '"""abc')
    check("unclosed blocktext does not crash", ok2)
end

-- Round 64: exclusion sets must not leak %n (ALL-controls) — a text run
-- stops only at the actual whitespace set, so a control byte like \x01
-- stays inside the text token instead of truncating it.
do
    local src = "hello\x01world"
    local toks = tokenizer.parse(src)
    check("text run keeps control byte", #toks == 1
        and toks[1].type == "text"
        and toks[1].content:find("hello\x01world", 1, true) ~= nil,
        #toks .. " tokens")
end

-- Round 66 perf baseline: a large scene (2000 commands + label + end)
-- parses within a generous budget and yields the exact token count.
do
    local parts = {}
    for i = 1, 2000 do
        parts[#parts + 1] = "[ch name=\"N\" text=\"line " .. i .. "\"]"
    end
    local big = table.concat(parts, "\n") .. "\n*end\n[end]\n"
    local t0 = os.clock()
    local toks = tokenizer.parse(big)
    local dt = os.clock() - t0
    check("2000-command scene parses", #toks == 2002, #toks .. " tokens")
    check("2000-command scene within budget", dt < 10.0, string.format("%.3fs", dt))
end


-- =============================================================================
-- Unicode & Multibyte Boundary Tests (deeper than round 10's BOM/unicode pass:
-- CJK/emoji/combining, full-width space, CJK quotes in params, invalid UTF-8,
-- UTF-16 BOM pass-through, mixed line endings, byte-accurate offsets, and a
-- 10K-char CJK perf budget -- this round's new angle).
-- =============================================================================

-- ---- A. multibyte text runs ------------------------------------------------
do
    local toks = tokenizer.parse("こんにちは世界")
    check("cjk text run single token", #toks == 1 and toks[1].type == "text"
        and toks[1].content == "こんにちは世界",
        #toks .. " tokens / " .. tostring(toks[1] and toks[1].content))
end

do
    local toks = tokenizer.parse("👋🎉混在テキスト😀")
    check("emoji + cjk text run preserved",
        #toks == 1 and toks[1].type == "text"
        and toks[1].content == "👋🎉混在テキスト😀",
        tostring(toks[1] and toks[1].content))
end

do
    -- U+0301 (combining acute) following a base letter must stay with it.
    local toks = tokenizer.parse("e\204\129x")   -- e + combining acute + x
    check("combining char kept in text", #toks == 1
        and toks[1].content:find("e\204\129x", 1, true) ~= nil,
        string.format("%q", tostring(toks[1] and toks[1].content)))
end

do
    -- ident is ASCII-only: a CJK command name is NOT in the grammar.
    local ok = pcall(tokenizer.parse, "[日本語 text=\"x\"]")
    check("cjk command name rejected", not ok)
end

do
    -- label ident is ASCII-only too.
    local ok = pcall(tokenizer.parse, "*ラベル")
    check("cjk label rejected", not ok)
end

-- ---- B. full-width / special whitespace ------------------------------------
do
    -- U+3000 (full-width space) is NOT in the ASCII space set that terminates
    -- a text run / bare value, so it stays inside the token.
    local toks = tokenizer.parse("a　b")          -- full-width space between
    check("fullwidth space is text not separator", #toks == 1
        and toks[1].type == "text" and toks[1].content == "a　b",
        string.format("%q", tostring(toks[1] and toks[1].content)))
end

do
    -- Full-width space inside a quoted parameter value is preserved as-is.
    local toks = tokenizer.parse("[ch text=a　b]")
    check("fullwidth space in param value",
        #toks == 1 and toks[1].params[1][1] == "text"
        and toks[1].params[1][2] == "a　b",
        string.format("%q", tostring(toks[1] and toks[1].params and toks[1].params[1] and toks[1].params[1][2])))
end

do
    -- Full-width space is not a param separator (ASCII space/tab/CR/LF only).
    local toks = tokenizer.parse("[ch text=a　b]")
    check("fullwidth space not a param separator",
        #toks == 1 and #toks[1].params == 1
        and toks[1].params[1][2] == "a　b",
        #(toks[1] and toks[1].params or {}) .. " params")
end

do
    -- Tab inside a quoted value is preserved; tab between params separates.
    local tq = tokenizer.parse('[c text="a\tb"]')
    check("tab preserved inside quoted value",
        tq[1].params[1][2] == "a\tb",
        string.format("%q", tostring(tq[1].params[1][2])))
    local ts = tokenizer.parse("[c a=1\tb=2]")
    check("tab is a param separator", #ts[1].params == 2
        and ts[1].params[1][1] == "a" and ts[1].params[2][1] == "b",
        #ts[1].params .. " params")
end

do
    -- Leading whitespace/tab is consumed by skip; trailing whitespace stays
    -- in the text content (text body stops only at [ * or newline).
    local toks = tokenizer.parse("   hi   ")
    check("leading space trimmed, trailing kept",
        #toks == 1 and toks[1].content == "hi   ",
        string.format("%q", tostring(toks[1] and toks[1].content)))
    local t2 = tokenizer.parse("\t[ch]")
    check("leading tab before command", #t2 == 1 and t2[1].cmd == "ch")
end

do
    -- CJK in bare positional values (full-width digits are not tonumber-able,
    -- but the tokenizer must preserve them verbatim for the handler).
    local d = tokenizer.parse("[delay ５００]")
    check("fullwidth digits in bare value",
        #d == 1 and d[1].params[1][2] == "５００",
        string.format("%q", tostring(d[1].params[1][2])))
    local c = tokenizer.parse("[ch 内容]")
    check("cjk bare value", #c == 1 and c[1].params[1][2] == "内容",
        string.format("%q", tostring(c[1].params[1][2])))
end

-- ---- C. quotes & multibyte -------------------------------------------------
do
    local toks = tokenizer.parse('[ch text="「引号」他强调"]')
    check("cjk quotes inside quoted value",
        #toks == 1 and toks[1].params[1][2] == "「引号」他强调",
        string.format("%q", tostring(toks[1].params[1][2])))
end

do
    local dq = tokenizer.parse('[ch text="😀🎉"]')
    check("emoji in double-quoted value", dq[1].params[1][2] == "😀🎉")
    local sq = tokenizer.parse("[ch text='😀']")
    check("emoji in single-quoted value", sq[1].params[1][2] == "😀")
end

do
    -- Unclosed quote with the bracket closed: value keeps the raw quote char,
    -- never crashes.
    local ok, toks = pcall(tokenizer.parse, '[ch text="開]')
    check("unclosed quote + cjk does not crash", ok)
    if ok then
        check("unclosed quote value keeps quote char",
            toks[1].params[1][2] == '"開',
            string.format("%q", tostring(toks[1].params[1][2])))
    end
end

do
    -- Full-width brackets are plain text, not command delimiters.
    local toks = tokenizer.parse("『ここ』")
    check("fullwidth brackets are text",
        #toks == 1 and toks[1].type == "text"
        and toks[1].content == "『ここ』",
        tostring(toks[1] and toks[1].content))
end

-- ---- D. BOM / encoding / line endings --------------------------------------
do
    -- BOM + multibyte: BOM is stripped, the CJK text that follows survives.
    local toks = tokenizer.parse('\239\187\191こんにちは[ch]')
    check("BOM then cjk text", #toks == 2
        and toks[1].type == "text" and toks[1].content == "こんにちは"
        and toks[2].cmd == "ch",
        #toks .. " tokens")
end

do
    -- UTF-16LE BOM (FF FE): no UTF-16 detection -- it is passed through as
    -- raw bytes (not skipped, not a crash). Locks the pass-through contract.
    local ok, toks = pcall(tokenizer.parse, "\255\254h")
    check("utf16le bom passes through (no crash)", ok)
    if ok then
        -- First token is text containing the raw FF FE bytes + 'h'.
        check("utf16le bom bytes preserved raw",
            #toks == 1 and toks[1].type == "text"
            and toks[1].content == "\255\254h",
            string.format("%q", tostring(toks[1] and toks[1].content)))
    end
end

do
    -- Invalid UTF-8 sequences (overlong/lone-continuation bytes) are
    -- tolerated and passed through byte-for-byte, never dropped/crash.
    local ok, toks = pcall(tokenizer.parse, "ab\255\254\128cd")
    check("invalid utf8 bytes tolerated", ok)
    if ok then
        check("invalid utf8 bytes preserved raw",
            #toks == 1 and toks[1].type == "text"
            and toks[1].content == "ab\255\254\128cd",
            string.format("%q", tostring(toks[1] and toks[1].content)))
    end
end

do
    -- Mixed CRLF / LF / CR line endings all work.
    local toks = tokenizer.parse("[a]\r\n[b]\n[c]\r[d]")
    check("mixed CRLF/LF/CR", #toks == 4
        and toks[1].cmd == "a" and toks[2].cmd == "b"
        and toks[3].cmd == "c" and toks[4].cmd == "d",
        #toks .. " tokens")
end

do
    -- CJK text and a command on the same line tokenize separately.
    local toks = tokenizer.parse("こんにちは[ch]")
    check("cjk text + cmd same line", #toks == 2
        and toks[1].type == "text" and toks[1].content == "こんにちは"
        and toks[2].cmd == "ch",
        #toks .. " tokens")
end

-- ---- E. comments / blocktext / iscript multibyte ---------------------------
do
    local toks = tokenizer.parse('; コメント 😊 空色\n[ch text="hi"]')
    check("comment with cjk + emoji skipped", #toks == 1
        and toks[1].cmd == "ch", #toks .. " tokens")
end

do
    -- A ; inside blocktext is literal text, not a comment.
    local toks = tokenizer.parse('\"\"\"line1\n; not a comment\nline2\"\"\"')
    check("comment marker inside blocktext is literal",
        #toks == 1 and toks[1].type == "text"
        and toks[1].content:find("line1\n; not a comment\nline2", 1, true) ~= nil,
        string.format("%q", tostring(toks[1] and toks[1].content)))
end

do
    -- iscript body keeps multibyte + Lua comments raw (not tokenized).
    local toks = tokenizer.parse('[iscript]\nlocal s = "日本語"; -- 😀 コメント\n[/endscript]')
    check("iscript preserves multibyte body", #toks == 1
        and toks[1].type == "iscript"
        and toks[1].body:find("日本語", 1, true) ~= nil
        and toks[1].body:find("😀", 1, true) ~= nil,
        tostring(toks[1] and toks[1].body))
end

-- ---- F. byte-accurate offsets across multibyte -----------------------------
do
    -- こんにちは = 5 CJK chars x 3 bytes = bytes 1..15; [ch] starts at 16.
    local o = tokenizer.parse_with_offsets("こんにちは[ch]")
    check("offsets cjk text", #o == 2 and o[1].type == "text"
        and o[1].offset == 1 and o[1].end_offset == 15,
        tostring(o[1] and o[1].offset .. "," .. o[1].end_offset))
    check("offsets cmd after cjk", o[2].cmd == "ch"
        and o[2].offset == 16 and o[2].end_offset == 19,
        tostring(o[2] and o[2].offset .. "," .. o[2].end_offset))
end

do
    -- Byte offsets are correct when a quoted parameter holds CJK.
    local o = tokenizer.parse_with_offsets('[x a="日本語"] 中')
    check("offsets with cjk in quoted value",
        #o == 2 and o[1].cmd == "x" and o[1].offset == 1
        and o[2].type == "text" and o[2].offset == 19 and o[2].end_offset == 21,
        (#o) .. " tokens; t1=" .. (o[1] and o[1].offset) .. " t2=" .. (o[2] and o[2].offset .. "," .. o[2].end_offset))
end

-- ---- G. 10K-CJK performance budget -----------------------------------------
do
    -- A single 10,000-character CJK text run must parse to one text token
    -- within a generous budget (CJK is 3 bytes/char => 30KB of source).
    local long = string.rep("人", 10000)
    local t0 = os.clock()
    local toks = tokenizer.parse(long)
    local dt = os.clock() - t0
    -- 人 is 3 bytes/char, so 10000 chars = 30000 bytes; Lua # is byte length.
    check("10K cjk single text token", #toks == 1
        and #toks[1].content == 30000, tostring(#toks) .. " tok(s), " .. #(toks[1] and toks[1].content or "") .. " bytes")
    check("10K cjk within budget", dt < 5.0, string.format("%.3fs", dt))
end

if failed > 0 then os.exit(1) end
