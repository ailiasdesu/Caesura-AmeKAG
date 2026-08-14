-- =============================================================================
--  Caesura (AmeKAG) — ks_bake.lua
--  Mobile bytecode pre-baking (Battle 5a): pre-compile every .ks scene
--  into its .ksc cache file BEFORE shipping, so the runtime (especially
--  mobile) loads pre-compiled bytecode with zero parse/compile at launch.
--
--  Path + freshness algorithms are IDENTICAL to flow.load_scene:
--    cache_root/<resolved path with / and \ -> _>.ksc
--    _srcHash = FNV-1a of the source file
--  A baked scene is a cache hit on first load.
--
--  Usage:
--    lua scripts/ks_bake.lua <scene.ks> [more.ks ...]
--    lua scripts/ks_bake.lua --dir scripts --dir demo --out cache/ksc
--    lua scripts/ks_bake.lua --check   (verify existing .ksc is fresh)
--
--  Exit codes: 0 = all baked, 1 = errors.
-- =============================================================================

local BS = string.char(92)
local here = (arg and arg[0] and arg[0]:match("(.*[/" .. BS .. "])")) or "scripts/"
package.path = here .. "?.lua;" .. here .. "kag/?.lua;" .. package.path

local tokenizer = require("tokenizer")
local compiler = require("kag.compiler")

local OUT_ROOT = "cache/ksc"

-- Same sanitization as flow.load_scene: resolved path -> flat file name.
local function kscPathFor(resolved, outRoot)
    return (outRoot or OUT_ROOT) .. "/"
        .. resolved:gsub("[/\\]+", "_"):gsub("%.ks$", ".ksc")
end

--- bake one scene: parse + compile + write .ksc. Returns true/false, err.
local function bakeScene(path, outRoot)
    local f = io.open(path, "r")
    if not f then return false, "cannot open: " .. path end
    local src = f:read("*a")
    f:close()

    local ok, tokens = pcall(tokenizer.parse, src)
    if not ok or not tokens then
        return false, "tokenize failed: " .. path
    end
    compiler.compile(tokens)
    tokens._srcHash = compiler.hashFile(path)
    local outPath = kscPathFor(path, outRoot)
    if not compiler.writeCache(tokens, outPath) then
        return false, "write failed: " .. outPath
    end
    return true, outPath
end

--- check whether an existing .ksc matches the source (fresh).
local function isFresh(path, outRoot)
    local outPath = kscPathFor(path, outRoot)
    local cached = compiler.readCache(outPath)
    if not cached or not cached._compiled then return false end
    local h = compiler.hashFile(path)
    return h ~= nil and cached._compiled._srcHash == h
end

-- ---------------------------------------------------------------------------
-- CLI
-- ---------------------------------------------------------------------------
local is_script = arg and arg[0]
    and arg[0]:match("([^/\\]+)$") == "ks_bake.lua"

if is_script then
    local inputs, dirs, outRoot, checkOnly = {}, {}, OUT_ROOT, false
    local i = 1
    while i <= #arg do
        local a = arg[i]
        if a == "--dir" then
            i = i + 1
            dirs[#dirs + 1] = arg[i]
        elseif a == "--out" then
            i = i + 1
            outRoot = arg[i]
        elseif a == "--check" then
            checkOnly = true
        elseif a == "-h" or a == "--help" then
            print("Usage: lua scripts/ks_bake.lua <scene.ks> [more.ks ...]")
            print("       lua scripts/ks_bake.lua --dir <dir> [--dir <dir2>] [--out cache/ksc] [--check]")
            print("  bakes scenes into pre-compiled .ksc (mobile zero-parse launch)")
            print("  --out: cache root (default cache/ksc, same as flow.load_scene)")
            print("  --check: verify existing .ksc freshness without rewriting")
            os.exit(0)
        else
            inputs[#inputs + 1] = a
        end
        i = i + 1
    end

    -- gather .ks from --dir args (recursive)
    local collected = {}
    local function add(p)
        if p:match("%.ks$") then collected[#collected + 1] = p end
    end
    for _, p in ipairs(inputs) do add(p) end
    for _, d in ipairs(dirs) do
        local cmd = "dir /s /b \"" .. d .. "\\*.ks\" 2>nul"
        local pf = io.popen(cmd)
        if pf then
            for raw_line in pf:lines() do
                local line = raw_line:gsub("\\", "/")
                add(line)
            end
            pf:close()
        end
    end

    if #collected == 0 then
        print("error: no .ks scenes given")
        os.exit(1)
    end

    local errors = 0
    local baked = 0
    local fresh = 0
    for _, path in ipairs(collected) do
        if checkOnly then
            if isFresh(path, outRoot) then
                fresh = fresh + 1
            else
                print("[stale] " .. path)
                errors = errors + 1
            end
        else
            local ok, res = bakeScene(path, outRoot)
            if ok then
                baked = baked + 1
                print("[baked] " .. res)
            else
                print("[error] " .. tostring(res))
                errors = errors + 1
            end
        end
    end
    print(string.format(
        "%s: %d baked, %d fresh, %d errors",
        checkOnly and "CHECK" or "BAKE", baked, fresh, errors))
    if errors > 0 then os.exit(1) end
    os.exit(0)
end

return {
    bakeScene = bakeScene,
    isFresh = isFresh,
    kscPathFor = kscPathFor,
}
