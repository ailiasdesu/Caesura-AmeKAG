-- =============================================================================
--  Caesura (AmeKAG) — kag/aiwriter.lua
--  AI-assisted scene writing (Battle 4c): generate dialogue lines or
--  continue an existing scene through the engine's local LLM binding
--  (Backend.ai_query — Ollama default, OpenAI-compatible auto-detected).
--  Exposed to the IDE via /api/eval (same pattern as kag/lsp.lua).
--
--  The LLM is asked to emit KAG Neo-Genesis tag lines ([ch ...]); the
--  output is sanitized so a non-conforming reply degrades to a comment
--  instead of corrupting the scene. All calls degrade gracefully when
--  the AI service is unavailable (returns nil + reason).
-- =============================================================================

local aiwriter = {}

local backend = require("backend")

-- Prompt fragments — keep the LLM on the KAG tag format.
local DIALOGUE_SYSTEM =
    "You write visual-novel dialogue for the KAG Neo-Genesis engine. "
    .. "Output ONLY valid KAG tags, one per line, using this exact form: "
    .. "[ch name=\"Character\" text=\"line\"] — no prose, no markdown, "
    .. "no code fences. Escape inner double quotes with \\\". "
    .. "Keep each line under 120 characters."

local CONTINUE_SYSTEM =
    "You continue a visual-novel scene for the KAG Neo-Genesis engine. "
    .. "Output ONLY valid KAG tags ([ch name=\"...\" text=\"...\"], "
    .. "[bg storage=\"...\"], [playbgm file=\"...\"], [wait time=...], "
    .. "[*label]) — no prose, no markdown, no code fences. Continue the "
    .. "scene naturally from where it left off."

--- aiwriter.available() → boolean
function aiwriter.available()
    return backend.ai_available()
end

--- aiwriter.generate_dialogue(opts) → text, err
--  opts: { speakers = "Aoi, Ryo", topic = "confession", lines = 5 }
--  Returns KAG [ch ...] lines joined by newlines, or nil + reason.
function aiwriter.generate_dialogue(opts)
    opts = opts or {}
    if not backend.ai_available() then
        return nil, "ai-unavailable (config.ai.endpoint reachable?)"
    end
    local speakers = opts.speakers or "Aoi, Ryo"
    local topic = opts.topic or "a quiet evening"
    local lines = tonumber(opts.lines) or 5
    local prompt = string.format(
        "Write a short %d-line dialogue exchange between %s about: %s. "
        .. "Vary the speakers. Keep it natural and concise.",
        lines, speakers, topic)
    local text, err = backend.ai_query(prompt, {
        system = opts.system or DIALOGUE_SYSTEM,
        model = opts.model,
    })
    if not text or text == "" then
        return nil, err or "empty-reply"
    end
    return aiwriter.sanitize_tags(text)
end

--- aiwriter.continue_scene(sceneText, opts) → text, err
--  Appends to an existing scene: sends the current scene tail to the LLM
--  and asks for a continuation. Returns new KAG lines (not including the
--  input).
function aiwriter.continue_scene(sceneText, opts)
    opts = opts or {}
    if not backend.ai_available() then
        return nil, "ai-unavailable"
    end
    local tail = sceneText or ""
    if #tail > 2000 then tail = tail:sub(-2000) end  -- context cap
    local prompt = string.format(
        "Here is the current scene:\n---\n%s\n---\n"
        .. "Continue it with %d new lines. Do not repeat what is shown.",
        tail, tonumber(opts.lines) or 5)
    local text, err = backend.ai_query(prompt, {
        system = opts.system or CONTINUE_SYSTEM,
        model = opts.model,
    })
    if not text or text == "" then
        return nil, err or "empty-reply"
    end
    return aiwriter.sanitize_tags(text)
end

--- aiwriter.sanitize_tags(text) → string
--  Keeps lines that look like KAG tags ([...], *label, ; comment) and
--  drops prose; wraps anything else as a ; comment so the scene stays
--  valid. Called on every AI reply.
--  Code-executing tags ([iscript]/[endscript]/[emb]/[eval]) are never
--  kept from an AI reply: a prompt-injected spec could otherwise make
--  the model emit an iscript block that executes in the scene (review
--  should-fix). They are commented out so the author sees them.
function aiwriter.sanitize_tags(text)
    local out = {}
    for line in (text .. "\n"):gmatch("(.-)\n") do
        local t = line:match("^%s*(.-)%s*$")
        if #t > 0 then
            if t:match("^%s*;") then
                -- comment lines always pass (even if they mention a tag)
                out[#out + 1] = t
            else
                -- code-executing tags are blocked ANYWHERE in the line
                -- (an allowed-tag line could smuggle [iscript] inline —
                -- review should-fix) and case-insensitively ([ISCRIPT]
                -- renders as text today but must not be relied on).
                -- The slash form [/endscript] is a REAL iscript closer in
                -- the tokenizer, so it is blocked too (a prompt-injected
                -- reply must not be able to close an author's block).
                local low = t:lower()
                if low:match("%[%s*/?%s*iscript")
                    or low:match("%[%s*/?%s*endscript")
                    or low:match("%[%s*emb") or low:match("%[%s*eval") then
                    out[#out + 1] = "; (ai) blocked: " .. t
                elseif t:match("^%[") or t:match("^%*") then
                    out[#out + 1] = t
                else
                    out[#out + 1] = "; (ai) " .. t
                end
            end
        end
    end
    if #out == 0 then return "; (ai) no valid lines generated" end
    return table.concat(out, "\n")
end

--- JSON string escape (shared by the json encoder).
local function json_str(s)
    s = tostring(s)
    return '"' .. s:gsub("\\", "\\\\"):gsub('"', '\\"')
        :gsub("\n", "\\n"):gsub("\r", "\\r") .. '"'
end

--- aiwriter.json(method, ...) → JSON string for the IDE (via /api/eval)
--  method: "available" | "generate_dialogue" | "continue_scene"
--  Returns a JSON array with {text, error} so the IDE can render either.
function aiwriter.json(method, ...)
    local text, err = nil, nil
    if method == "available" then
        text = tostring(aiwriter.available())
        err = ""
    elseif method == "generate_dialogue" then
        local opts = ...
        text, err = aiwriter.generate_dialogue(opts)
    elseif method == "continue_scene" then
        local scene, opts = ...
        text, err = aiwriter.continue_scene(scene, opts)
    end
    if text == nil then text = "" end
    return string.format('[{"text":%s,"error":%s}]',
        json_str(text), json_str(err or ""))
end

return aiwriter
