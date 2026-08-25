-- =============================================================================
--  Caesura (AmeKAG) — kag/aidev.lua
--  AI-assisted DEVELOPMENT workflows (Battle 4c extension): the LLM serves
--  the developer, not just the story — explain diagnostics, propose fixes,
--  generate scene skeletons, and review scenes. Deterministic local paths
--  (rule table / structural scan) work WITHOUT an LLM; the LLM enriches
--  when available.
--
--  Transport is the same as kag/aiwriter.lua: backend.ai_query (Ollama
--  default, OpenAI-compatible auto-detected) + json() for the IDE via
--  /api/eval. Every call degrades gracefully (nil + reason).
-- =============================================================================

local aidev = {}

local backend = require("backend")

-- ---------------------------------------------------------------------------
-- Local diagnostic explainer (deterministic, no LLM needed). Keys are
-- substrings of the messages emitted by ks_check / lsp diagnostics;
-- first match wins, ordered from specific to generic.
-- ---------------------------------------------------------------------------
local EXPLAIN_RULES = {
    { pattern = "unknown KAG command",
      text = "该标签既不是已迁移的命令契约、已知 handler，也不是本地宏定义。常见原因：命令名拼写错误（如 [wiat]）、调用宏前未定义 [macro name=...]、或该命令尚未迁移。运行时它会作为普通文本显示并输出 [WARN]。" },
    { pattern = "does not compile",
      text = "表达式（exp 参数）无法编译。检查 TJS 语法（&& || ! != ?: 三元）、括号配对与变量名拼写；也可以先用 [eval] 单独验证表达式。编辑器输入时该诊断会实时出现。" },
    { pattern = "stopped before end of input",
      text = "解析流在输入结束前停止：通常是未闭合的 [ 标签、引号不配对，或文件末尾存在非标签内容。检查报错行附近（含尾随注释之外的文本）。" },
    { pattern = "must be one of",
      text = "该参数是枚举类型，取值不在允许集合内。查看命令契约中的 choices 列表（docs/api/command-contracts.md 或 LSP 补全提示）。" },
    { pattern = "expects a number",
      text = "该参数需要数字，收到的是其它类型。检查是否把变量名直接传入而未用 ${} 插值，或引号内混入了文本。" },
    { pattern = "required",
      text = "该命令缺少必需参数。对照命令契约补齐（编辑器 LSP 会提示参数列表）。" },
    { pattern = "cannot open file",
      text = "文件无法打开：路径不存在、大小写不匹配（Windows 不区分、Linux/macOS 区分）或权限不足。检查 storage/file 参数与资源目录。" },
    { pattern = "exceeded",
      text = "超出了执行预算/上限：通常是 [while]/[for]/[until] 条件永不满足导致死循环，或单帧工作量过大。检查循环条件与 timeout。" },
    { pattern = "expression in",
      text = "该命令携带的表达式编译失败，具体原因见诊断原文。可先用 [eval exp=...] 分段验证。" },
}

local DEFAULT_EXPLAIN =
    "该诊断来自静态检查器（ks_check/LSP）。对照命令契约（docs/api/command-contracts.md）"
    .. "与语言教程（docs/guides/kag-language-tour.md）检查报错行；若为运行时预算类问题，检查循环与等待命令。"

local DEVSYSTEM =
    "You are a KAG Neo-Genesis engine expert helping a developer. "
    .. "Answer concisely in the same language as the question. "
    .. "If you propose script changes, output ONLY valid KAG tags, "
    .. "one per line, no prose, no code fences."

--- aidev.explain_diagnostic(diag, opts) → text
--  diag = { scene, line, message, source } (source = offending line, optional)
--  opts.llm = true → also ask the LLM when available; the local rule
--  explanation is always the base (deterministic, offline-safe).
function aidev.explain_diagnostic(diag, opts)
    opts = opts or {}
    diag = diag or {}
    local message = tostring(diag.message or "")
    local base = DEFAULT_EXPLAIN
    for _, rule in ipairs(EXPLAIN_RULES) do
        if message:find(rule.pattern, 1, true) then
            base = rule.text
            break
        end
    end
    if not (opts.llm and backend.ai_available()) then
        return base
    end
    local prompt = string.format(
        "A developer hit this KAG Neo-Genesis diagnostic:\n"
        .. "scene: %s, line: %s\nmessage: %s\nsource line: %s\n"
        .. "Explain the likely cause and the fix in 2-4 sentences.",
        tostring(diag.scene or "?"), tostring(diag.line or "?"),
        message, tostring(diag.source or ""))
    local text, err = backend.ai_query(prompt, { system = DEVSYSTEM })
    if not text or text == "" then
        return base .. "\n(ai enrichment unavailable: " .. tostring(err or "empty-reply") .. ")"
    end
    return base .. "\n\n--- AI 补充 ---\n" .. text
end

--- aidev.suggest_fix(sceneText, diag, opts) → text, err
--  Asks the LLM for the corrected KAG tags near the diagnostic line;
--  the reply is sanitized (non-tag prose becomes comments).
function aidev.suggest_fix(sceneText, diag, opts)
    opts = opts or {}
    diag = diag or {}
    if not backend.ai_available() then
        return nil, "ai-unavailable"
    end
    local scene = tostring(sceneText or "")
    local line = tonumber(diag.line) or 0
    -- context window around the offending line
    local lines = {}
    for ln in (scene .. "\n"):gmatch("(.-)\n") do lines[#lines + 1] = ln end
    local from, to = math.max(1, line - 3), math.min(#lines, line + 2)
    local ctx = table.concat(lines, "\n", from, to)
    local prompt = string.format(
        "A developer got this diagnostic on line %d:\n%s\n"
        .. "Here is the context:\n---\n%s\n---\n"
        .. "Output the corrected KAG tags (the lines that should replace "
        .. "the offending line), one per line. If the fix is structural, "
        .. "output the minimal corrected block.",
        line, tostring(diag.message or ""), ctx)
    local text, err = backend.ai_query(prompt, { system = DEVSYSTEM })
    if not text or text == "" then
        return nil, err or "empty-reply"
    end
    return require("kag.aiwriter").sanitize_tags(text)
end

--- aidev.gen_scene(spec, opts) → text, err
--  Generates a full scene skeleton (labels, flow, choices, dialogue) from
--  a prose spec. Output is sanitized, then locally reviewed; structural
--  warnings are appended as ; (aidev) comment lines so the scene stays
--  valid and the developer sees them in-editor.
function aidev.gen_scene(spec, opts)
    opts = opts or {}
    if not backend.ai_available() then
        return nil, "ai-unavailable"
    end
    local prompt = string.format(
        "Write a complete KAG Neo-Genesis scene from this spec:\n%s\n"
        .. "Rules: start with *start; use [bg storage=...], [ch name=... text=...], "
        .. "[wait time=...], [if exp=...]/[endif] branches, "
        .. "[button text=... target=*label]...[endbutton] choices, and "
        .. "[jump target=*label] endings. End with [end]. Keep every line "
        .. "under 120 characters.",
        tostring(spec or ""))
    local text, err = backend.ai_query(prompt, {
        system = "You write KAG Neo-Genesis scenes. Output ONLY valid KAG tags, "
            .. "one per line, no prose, no markdown, no code fences.",
        model = opts.model,
    })
    if not text or text == "" then
        return nil, err or "empty-reply"
    end
    text = require("kag.aiwriter").sanitize_tags(text)
    -- self-check: append structural warnings as comments
    local warnings = aidev.review_scene(text)
    if #warnings > 0 then
        for _, w in ipairs(warnings) do
            text = text .. "\n; (aidev) review: " .. w.message
        end
    end
    return text
end

--- aidev.review_scene(sceneText, opts) → list of {line, message}
--  Local structural scan (deterministic): flow-tag balance + scene
--  termination. opts.llm = true appends an AI semantic review (pacing,
--  dead ends, missing endings) when available.
local BLOCK_PAIRS = {
    { open = "if", close = "endif" },
    { open = "while", close = "endwhile" },
    { open = "for", close = "endfor" },
    { open = "switch", close = "endswitch" },
    { open = "macro", close = "endmacro" },
    { open = "button", close = "endbutton" },
}
-- Flow commands the runtime matches case-sensitively; a case-mismatched
-- spelling renders as text (with [WARN]) instead of controlling flow.
local FLOW_LOWER = {}
for _, pair in ipairs(BLOCK_PAIRS) do
    FLOW_LOWER[pair.open] = true
    FLOW_LOWER[pair.close] = true
end
FLOW_LOWER["end"] = true
FLOW_LOWER["stop"] = true
function aidev.review_scene(sceneText, opts)
    opts = opts or {}
    local findings = {}
    local text = tostring(sceneText or "")
    local ok, tokens = pcall(function()
        return require("tokenizer").parse_with_offsets(text)
    end)
    if not ok or not tokens then
        return { { line = 0, message = "场景无法解析（tokenize 失败）" } }
    end
    -- byte offset -> line number (line-start index + binary search,
    -- same approach as ks_check.lua)
    local lineStarts = { 1 }
    for i = 1, #text do
        if text:byte(i) == 10 then lineStarts[#lineStarts + 1] = i + 1 end
    end
    local function lineOf(offset)
        local lo, hi = 1, #lineStarts
        while lo < hi do
            local mid = (lo + hi + 1) // 2
            if lineStarts[mid] <= offset then lo = mid else hi = mid - 1 end
        end
        return lo
    end
    -- flow balance: opens increment, closes decrement; a close with no
    -- open pending is a stray closer
    local depth = {}
    for _, pair in ipairs(BLOCK_PAIRS) do depth[pair.open] = 0 end
    local closers = {}
    for _, pair in ipairs(BLOCK_PAIRS) do closers[pair.close] = pair.open end
    local seen_end = false
    for _, tok in ipairs(tokens) do
        local cmd = tok.type == "command" and tok.cmd or nil
        -- case-mismatched flow command: runtime renders it as text
        -- (with [WARN]); flag it so the review cannot pass it silently
        if cmd and cmd ~= cmd:lower() and FLOW_LOWER[cmd:lower()] then
            findings[#findings + 1] = {
                line = lineOf(tok.offset or 1),
                message = "大小写不匹配的流命令 [" .. cmd
                    .. "]（运行时按文本渲染并告警，请改为 [" .. cmd:lower() .. "]）",
            }
        end
        if cmd == "end" or cmd == "stop" then seen_end = true end
        if closers[cmd] then
            local open = closers[cmd]
            if depth[open] == 0 then
                findings[#findings + 1] = {
                    line = lineOf(tok.offset or 1),
                    message = "多余的 [" .. cmd .. "]：没有配对的 [" .. open .. "]",
                }
            else
                depth[open] = depth[open] - 1
            end
        elseif depth[cmd] ~= nil then
            depth[cmd] = depth[cmd] + 1
        end
    end
    for _, pair in ipairs(BLOCK_PAIRS) do
        if depth[pair.open] > 0 then
            findings[#findings + 1] = {
                line = 0,
                message = "未闭合的 [" .. pair.open .. "]：缺少 " .. depth[pair.open]
                    .. " 个 [" .. pair.close .. "]",
            }
        end
    end
    if not seen_end then
        findings[#findings + 1] = {
            line = 0,
            message = "场景没有 [end]：游戏流程可能悬空",
        }
    end
    -- optional AI semantic pass
    if opts.llm and backend.ai_available() then
        local prompt = string.format(
            "Review this KAG scene for narrative/flow issues (dead ends, "
            .. "missing choices, pacing):\n---\n%s\n---\n"
            .. "List at most 3 issues as 'line N: issue' lines.",
            text:sub(1, 3000))
        local reply, err = backend.ai_query(prompt, { system = DEVSYSTEM })
        if reply and reply ~= "" then
            for ln in (reply .. "\n"):gmatch("(.-)\n") do
                local t = ln:match("^%s*(.-)%s*$")
                if #t > 0 and not t:match("^%[") then
                    findings[#findings + 1] = { line = -1, message = t }
                end
            end
        end
    end
    return findings
end

--- JSON string escape (shared by the json encoder). Escapes backslash,
--  quotes, \n \r \t and any remaining C0 control char (\uXXXX) so LLM
--  replies with tabs/control bytes still yield valid JSON.
local function json_str(s)
    s = tostring(s)
    s = s:gsub("\\", "\\\\"):gsub('"', '\\"')
        :gsub("\n", "\\n"):gsub("\r", "\\r"):gsub("\t", "\\t")
    s = s:gsub("[%c]", function(c)
        return string.format("\\u%04x", c:byte())
    end)
    return '"' .. s .. '"'
end

--- aidev.json(method, ...) → JSON string for the IDE (via /api/eval)
--  method: "available" | "explain_diagnostic" | "suggest_fix"
--          | "gen_scene" | "review_scene"
--  Returns [{"text":..., "error":...}] so the IDE renders either; for
--  review_scene, text is the findings JSON-encoded as a string.
function aidev.json(method, ...)
    local text, err = nil, nil
    if method == "available" then
        text = tostring(backend.ai_available())
        err = ""
    elseif method == "explain_diagnostic" then
        local diag, opts = ...
        text = aidev.explain_diagnostic(diag, opts)
    elseif method == "suggest_fix" then
        local scene, diag, opts = ...
        text, err = aidev.suggest_fix(scene, diag, opts)
    elseif method == "gen_scene" then
        local spec, opts = ...
        text, err = aidev.gen_scene(spec, opts)
    elseif method == "review_scene" then
        local scene, opts = ...
        local findings = aidev.review_scene(scene, opts)
        local parts = {}
        for _, f in ipairs(findings) do
            parts[#parts + 1] = string.format('{"line":%d,"message":%s}',
                f.line, json_str(f.message))
        end
        text = "[" .. table.concat(parts, ",") .. "]"
        err = ""
    end
    if text == nil then text = "" end
    return string.format('[{"text":%s,"error":%s}]',
        json_str(text), json_str(err or ""))
end

return aidev
