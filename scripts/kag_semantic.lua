-- =============================================================================
--  Caesura (AmeKAG) — kag_semantic.lua (CLI Frontend)
--  Unified KAG Semantic Layer CLI Tool
--
--  Usage:
--    lua scripts/kag_semantic.lua json <scene.ks> [-o output.json]
--    lua scripts/kag_semantic.lua flow <scene.ks|dir> [--format mermaid|json] [--lint] [-o output.md]
--    lua scripts/kag_semantic.lua i18n <scene.ks|dir> [--format csv|po] [--lang zh] [-o output.csv]
--    lua scripts/kag_semantic.lua lint <scene.ks|dir>
-- =============================================================================

local BS = string.char(92)
local here = (arg and arg[0] and arg[0]:match("(.*[/" .. BS .. "])")) or "scripts/"
if not package.path:find(here, 1, true) then
    package.path = here .. "?.lua;" .. here .. "?/init.lua;" .. package.path
end

local semantic = require("kag.semantic")

local function get_ks_files(input_path)
    local files = {}
    local f = io.open(input_path, "r")
    if f then
        f:close()
        files[1] = input_path
        return files
    end
    
    -- Strip trailing slashes
    local clean_dir = input_path:gsub("[/\\]+$", "")
    
    -- Check common relative files first
    local common_subfiles = { "story.ks", "story_lastletter.ks", "stress.ks", "showcase.ks", "galgame_demo.ks" }
    for _, sf in ipairs(common_subfiles) do
        local test_path = clean_dir .. "/" .. sf
        local tf = io.open(test_path, "r")
        if tf then
            tf:close()
            files[#files + 1] = test_path
        end
    end
    
    if #files == 0 then
        -- POSIX / git-bash find with forward slashes
        local norm_dir = clean_dir:gsub("\\", "/")
        local p = io.popen('git ls-files "' .. norm_dir .. '/*.ks" 2>nul || dir /b /s "' .. clean_dir .. '\\*.ks" 2>nul')
        if p then
            for line in p:lines() do
                local clean = line:match("^%s*(.-)%s*$")
                if clean and clean:find("%.ks$") then
                    files[#files + 1] = clean
                end
            end
            p:close()
        end
    end
    
    table.sort(files)
    return files
end

local function print_help()
    print([[
Caesura (AmeKAG) — KAG Unified Semantic CLI

Commands:
  json <input.ks> [-o out.json]          Export full AST / Semantic Model as JSON
  flow <input.ks|dir> [--format mermaid|json] [--lint] [-o out.md]
                                         Generate Story Flow Graph & Branching Topology
  i18n <input.ks|dir> [--format csv|po] [--lang zh] [-o out.csv]
                                         Extract translatable strings from AST
  lint <input.ks|dir>                    Run full semantic contract & reachability lint
]])
end

local cmd = arg[1]
if not cmd or cmd == "-h" or cmd == "--help" then
    print_help()
    os.exit(0)
end

local input_target = arg[2]
if not input_target then
    io.stderr:write("Error: missing input file or directory.\n")
    print_help()
    os.exit(1)
end

-- Parse flags
local flags = {
    out = nil,
    format = nil,
    lang = "zh",
    lint = false,
}

local i = 3
while i <= #arg do
    local a = arg[i]
    if a == "-o" or a == "--out" then
        flags.out = arg[i + 1]
        i = i + 2
    elseif a == "--format" then
        flags.format = arg[i + 1]
        i = i + 2
    elseif a == "--lang" then
        flags.lang = arg[i + 1]
        i = i + 2
    elseif a == "--lint" then
        flags.lint = true
        i = i + 1
    else
        i = i + 1
    end
end

local files = get_ks_files(input_target)
if #files == 0 then
    io.stderr:write("Error: no .ks files found matching: " .. input_target .. "\n")
    os.exit(1)
end

-- -----------------------------------------------------------------------------
-- Subcommand Dispatch
-- -----------------------------------------------------------------------------
if cmd == "json" or cmd == "ast" then
    local m = semantic.parse_file(files[1])
    local out_str = semantic.to_json(m)
    if flags.out then
        local outf = io.open(flags.out, "w")
        if outf then
            outf:write(out_str)
            outf:close()
            print("Wrote JSON AST to " .. flags.out)
        else
            io.stderr:write("Error: cannot write to " .. flags.out .. "\n")
        end
    else
        print(out_str)
    end

elseif cmd == "flow" then
    local fmt = flags.format or "mermaid"
    local all_mermaid = {}
    local all_json_models = {}
    local total_errors = 0
    local total_warns = 0
    
    for _, fpath in ipairs(files) do
        local m = semantic.parse_file(fpath)
        all_mermaid[#all_mermaid + 1] = semantic.to_mermaid(m)
        all_json_models[m.basename] = m.flow_graph
        
        if flags.lint then
            for _, diag in ipairs(m.diagnostics) do
                if diag.severity == "ERROR" then
                    total_errors = total_errors + 1
                    print(string.format("[%s:L%d] ERROR (%s): %s", m.basename, diag.line, diag.code, diag.message))
                elseif diag.severity == "WARN" then
                    total_warns = total_warns + 1
                    print(string.format("[%s:L%d] WARN (%s): %s", m.basename, diag.line, diag.code, diag.message))
                end
            end
        end
    end
    
    if flags.lint then
        print(string.format("\n=== Story Flow Diagnostics (%d scenes) ===", #files))
        if total_errors == 0 then
            print("OK: All story jumps, calls, and branches are sound with zero broken targets.")
        else
            print(string.format("Diagnostics found %d errors, %d warnings.", total_errors, total_warns))
        end
        print("")
    end
    
    local out_str = ""
    if fmt == "mermaid" or fmt == "markdown" then
        out_str = "# Story Flow Diagram\n\n" .. table.concat(all_mermaid, "\n\n")
    elseif fmt == "json" then
        out_str = semantic.to_json(all_json_models)
    end
    
    if flags.out then
        local outf = io.open(flags.out, "w")
        if outf then
            outf:write(out_str)
            outf:close()
            print("Wrote story flow to " .. flags.out)
        else
            io.stderr:write("Error: cannot write to " .. flags.out .. "\n")
        end
    else
        if not flags.lint or flags.format then
            print(out_str)
        end
    end

elseif cmd == "i18n" then
    local fmt = flags.format or "csv"
    local combined_trans = {}
    for _, fpath in ipairs(files) do
        local m = semantic.parse_file(fpath)
        for _, t in ipairs(m.translatables) do
            combined_trans[#combined_trans + 1] = t
        end
    end
    
    local dummy_model = { translatables = combined_trans }
    local out_str = ""
    if fmt == "csv" then
        out_str = semantic.to_csv(dummy_model)
    elseif fmt == "po" then
        out_str = semantic.to_po(dummy_model, flags.lang)
    end
    
    if flags.out then
        local outf = io.open(flags.out, "w")
        if outf then
            outf:write(out_str)
            outf:close()
            print(string.format("Exported %d translatable strings to %s", #combined_trans, flags.out))
        else
            io.stderr:write("Error: cannot write to " .. flags.out .. "\n")
        end
    else
        print(out_str)
    end

elseif cmd == "lint" then
    local total_errors = 0
    local total_warns = 0
    for _, fpath in ipairs(files) do
        local m = semantic.parse_file(fpath)
        for _, diag in ipairs(m.diagnostics) do
            if diag.severity == "ERROR" then
                total_errors = total_errors + 1
                print(string.format("%s:%d:%d: ERROR [%s] %s", fpath, diag.line, diag.col, diag.code, diag.message))
            else
                total_warns = total_warns + 1
                print(string.format("%s:%d:%d: WARN [%s] %s", fpath, diag.line, diag.col, diag.code, diag.message))
            end
        end
    end
    print(string.format("\nLint complete: %d scenes checked, %d errors, %d warnings.", #files, total_errors, total_warns))
    if total_errors > 0 then
        os.exit(1)
    end
end
