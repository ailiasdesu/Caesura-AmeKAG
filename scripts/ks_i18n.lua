-- ===========================================================================
--  ks_i18n.lua — Localization template generator (authoring tool)
--  Scans .ks scenes and emits/merges the per-line translation `lines`
--  template into assets/lang/<code>.lua. Keys are content-addressed:
--  "<scene>:<fnv1a(message)>" — identical to the runtime key derivation
--  in i18n.localize (scripts/i18n.lua), so generated templates translate
--  the exact dialogue the engine displays.
--
--  CLI usage:
--    lua scripts/ks_i18n.lua [--dir <scenes-dir>] [--lang <code>]
--                            [--out <path>] [--update]
--    --dir    scene directory to scan (default: "demo")
--    --lang   language code (default: "en")
--    --out    output path (default: assets/lang/<lang>.lua)
--    --update merge into an existing lang file, keeping current
--             translations and appending only new keys (default: fresh
--             template, existing translations lost)
--
--  Module usage (tests): require("ks_i18n") exposes extract_messages /
--  build_template / merge_template.
-- ===========================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local tokenizer = require("tokenizer")
local i18n = require("i18n")
local fileutil = require("fileutil")

local M = {}

-- ---------------------------------------------------------------------------
--  Extract dialogue messages from one .ks file. Mirrors the runtime path:
--  bare text tokens become [ch] {text=content} (compiler.to_array_tok),
--  and [ch]/[text] commands contribute their text/message param.
-- ---------------------------------------------------------------------------
function M.extract_messages(ks_text)
    local msgs = {}
    local tokens = tokenizer.parse(ks_text)
    for _, tok in ipairs(tokens) do
        if tok.type == "text" then
            if tok.content and #tok.content > 0 then
                msgs[#msgs + 1] = tok.content
            end
        elseif tok.type == "command" then
            local cmd = tok.cmd
            -- Tokenizer params are raw pair lists {{key,val},...};
            -- the runtime compiler converts them to named fields, so
            -- mirror that here (params.text / params.message).
            local p = {}
            for _, pair in ipairs(tok.params or {}) do
                if type(pair) == "table" and pair[1] then
                    p[pair[1]] = pair[2]
                end
            end
            if cmd == "ch" or cmd == "text" then
                local m = p.text or p.message
                if type(m) == "string" and #m > 0 then
                    msgs[#msgs + 1] = m
                end
            elseif cmd == "button" or cmd == "sel" then
                -- Choice labels: same content-addressed key space (same
                -- string in a [ch] and a [button] shares one translation).
                local m = p.text or p.caption
                if type(m) == "string" and #m > 0 then
                    msgs[#msgs + 1] = m
                end
            end
        end
    end
    return msgs
end

-- ---------------------------------------------------------------------------
--  Load an existing lang file as a table (for --update merge).
--  Accepts both hand-written bare table literals and tool-generated
--  files (leading comments + explicit `return`) — same comment-stripping
--  strategy as i18n.load.
-- ---------------------------------------------------------------------------
function M.load_lang(path)
    local f = io.open(path, "r")
    if not f then return nil end
    local txt = f:read("*a")
    f:close()
    -- Strip all leading comment lines, then detect a GENERATED file by an
    -- anchored top-level `return` (a bare substring test would misfire on
    -- hand-written files whose string values mention "return", silently
    -- losing settings keys on --update). The `^` anchor matches once per
    -- gsub pass, so loop until no comment line remains.
    local body = txt
    while body:match("^%s*%-%-") do
        body = body:gsub("^%s*%-%-[^\n]*\n?", "", 1)
    end
    local chunk = body:match("^%s*return") and load(body, "ks_i18n", "t", {})
        or load("return " .. body, "ks_i18n", "t", {})
    if not chunk then return nil end
    local ok, data = pcall(chunk)
    if not ok or type(data) ~= "table" then return nil end
    return data
end

-- ---------------------------------------------------------------------------
--  Collect all dialogue keys + originals from a scene dir (dedup by key).
-- ---------------------------------------------------------------------------
function M.collect_entries(dir)
    local files = fileutil.scan_dir(dir, "%.ks$")
    table.sort(files)

    local entries = {}
    local seen = {}
    local total_keys, total_scenes = 0, 0
    for _, fname in ipairs(files) do
        local f = io.open(dir .. "/" .. fname, "r")
        if f then
            local txt = f:read("*a")
            f:close()
            local msgs = M.extract_messages(txt)
            if #msgs > 0 then
                total_scenes = total_scenes + 1
                for _, msg in ipairs(msgs) do
                    local key = fname .. ":" .. i18n.fnv1a(msg)
                    if not seen[key] then
                        seen[key] = true
                        entries[#entries + 1] = { key = key, original = msg }
                        total_keys = total_keys + 1
                    end
                end
            end
        end
    end
    return entries, total_keys, total_scenes
end

-- ---------------------------------------------------------------------------
--  find_missing(dir, langData) -> { entries = {{key, original}, ...}, total }
--  Keys whose lang-file translation is absent or an empty placeholder —
--  the untranslated backlog a translator still has to fill.
-- ---------------------------------------------------------------------------
function M.find_missing(dir, langData)
    local entries = M.collect_entries(dir)
    local missing = {}
    local lines = langData and langData.lines or {}
    for _, e in ipairs(entries) do
        local v = lines[e.key]
        if v == nil or #tostring(v) == 0 then
            missing[#missing + 1] = e
        end
    end
    return { entries = missing, total = #entries, missing = #missing }
end

-- ---------------------------------------------------------------------------
--  serialize_field(k, v) -> generated Lua line (or nil for unsupported).
--  Serializes one top-level dictionary entry the lang file can carry:
--  string / number / boolean, plus TABLE values that are plural-form
--  entries (e.g. items = { one = "{n} item", other = "{n} items" }) or any
--  nested string table. Plural entries are emitted as an inline table with
--  sorted keys so --update roundtrips them without losing the variants.
-- ---------------------------------------------------------------------------
local function serialize_field(k, v)
    if type(v) == "string" then
        return string.format("  %s = %q,", k, v)
    elseif type(v) == "number" or type(v) == "boolean" then
        return string.format("  %s = %s,", k, tostring(v))
    elseif type(v) == "table" then
        local keys = {}
        for sk in pairs(v) do keys[#keys + 1] = sk end
        table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
        local inner = {}
        for _, sk in ipairs(keys) do
            local sv = v[sk]
            if type(sv) == "string" then
                inner[#inner + 1] = string.format("%s = %q", sk, sv)
            elseif type(sv) == "number" or type(sv) == "boolean" then
                inner[#inner + 1] = string.format("%s = %s", sk, tostring(sv))
            end
        end
        return string.format("  %s = { %s },", k, table.concat(inner, ", "))
    end
    return nil
end

-- ---------------------------------------------------------------------------
--  Build the template body for a scene dir (optionally merging an existing
--  lang table). Returns the .lua file content as a string.
-- ---------------------------------------------------------------------------
function M.build_template(dir, existing)
    local entries, total_keys, total_scenes = M.collect_entries(dir)

    local out_lines = {}
    out_lines[#out_lines + 1] = "-- Auto-generated by scripts/ks_i18n.lua (do not edit by hand)"
    out_lines[#out_lines + 1] = "-- Scene: " .. dir .. " | keys: " .. total_keys
    out_lines[#out_lines + 1] = "return {"
    -- Merge mode: preserve the existing top-level fields (settings keys,
    -- plural entries, etc.) so --update never destroys hand-authored data.
    if existing then
        for k, v in pairs(existing) do
            if k ~= "lines" then
                local s = serialize_field(k, v)
                if s then out_lines[#out_lines + 1] = s end
            end
        end
    end
    out_lines[#out_lines + 1] = "  lines = {"
    local prev_scene = nil
    for _, e in ipairs(entries) do
        local scene = e.key:match("^(.-):")
        if scene ~= prev_scene then
            out_lines[#out_lines + 1] = "    -- " .. scene
            prev_scene = scene
        end
        local translation = ""
        if existing and existing.lines and existing.lines[e.key] ~= nil then
            translation = existing.lines[e.key]
        end
        -- Keep the original as a context comment for translators.
        local orig_esc = e.original:gsub("%-%-", "—")
        out_lines[#out_lines + 1] = string.format(
            '    ["%s"] = %q, -- original: %s', e.key, translation, orig_esc)
    end
    out_lines[#out_lines + 1] = "  },"
    out_lines[#out_lines + 1] = "}"

    local kept = 0
    if existing and existing.lines then
        for _, e in ipairs(entries) do
            if existing.lines[e.key] ~= nil
                and #tostring(existing.lines[e.key]) > 0 then
                kept = kept + 1
            end
        end
    end
    return table.concat(out_lines, "\n") .. "\n", total_keys, total_scenes, kept
end

-- ---------------------------------------------------------------------------
--  CLI main (only when run as a script, not when required by tests)
-- ---------------------------------------------------------------------------
local is_cli = arg and arg[0] and arg[0]:find("ks_i18n%.lua$") ~= nil
if is_cli then
    local args = {}
    for i = 1, #arg do
        local a = arg[i]
        if a == "--dir" then args.dir = arg[i + 1]
        elseif a == "--lang" then args.lang = arg[i + 1]
        elseif a == "--out" then args.out = arg[i + 1]
        elseif a == "--update" then args.update = true
        elseif a == "--missing" then args.missing = true
        end
    end
    local dir = args.dir or "demo"
    local lang = args.lang or "en"
    local out = args.out or ("assets/lang/" .. lang .. ".lua")

    if args.missing then
        -- Untranslated report: list keys whose translation is absent or
        -- an empty placeholder, grouped by scene. Exit 1 when any key is
        -- still missing (CI gate for translation completeness).
        local langData = M.load_lang(out)
        local report = M.find_missing(dir, langData)
        local prev_scene = nil
        for _, e in ipairs(report.entries) do
            local scene = e.key:match("^(.-):")
            if scene ~= prev_scene then
                print("-- " .. scene)
                prev_scene = scene
            end
            local orig_esc = e.original:gsub("%-%-", "—")
            print(string.format('  ["%s"] = "", -- %s', e.key, orig_esc))
        end
        print(string.format(
            "ks_i18n --missing: %d/%d keys untranslated in %s (%s)",
            report.missing, report.total, out, lang))
        os.exit(report.missing > 0 and 1 or 0)
    end

    local existing = nil
    if args.update then
        existing = M.load_lang(out) or {}
        existing.lines = type(existing.lines) == "table" and existing.lines or {}
    end

    local body, total_keys, total_scenes, kept =
        M.build_template(dir, existing)
    local fout = io.open(out, "w")
    if not fout then
        print("ks_i18n: cannot write " .. out)
        os.exit(1)
    end
    fout:write(body)
    fout:close()

    local keptMsg = ""
    if args.update then
        keptMsg = string.format(" (kept %d existing translations)", kept or 0)
    end
    print(string.format(
        "ks_i18n: %d messages from %d scenes -> %s%s",
        total_keys, total_scenes, out, keptMsg))
end

return M
