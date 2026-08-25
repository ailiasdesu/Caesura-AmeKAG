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

--- Web export (round 35): bake every scene into a story.json bundle for
--  the web player: { version, scenes: { path = serialized }, assets: [...] }.
--  Assets are discovered from [bg]/[fg]/[playbgm]/[playse]/[playvoice]
--  storage/file params (best-effort; the player falls back to the .ksc
--  stream when an asset is missing).
local function bakeWeb(scenes, outDir)
    local bundle = { version = 1, scenes = {}, assets = {} }
    local seen = {}
    local assetCount = 0
    local function addAsset(p)
        if type(p) ~= "string" or #p == 0 or seen[p] then return end
        seen[p] = true
        assetCount = assetCount + 1
        bundle.assets[assetCount] = p
    end
    for _, path in ipairs(scenes) do
        local f = io.open(path, "r")
        if not f then return nil, "cannot open: " .. path end
        local src = f:read("*a")
        f:close()
        local ok, tokens = pcall(tokenizer.parse, src)
        if not ok or not tokens then return nil, "tokenize failed: " .. path end
        compiler.compile(tokens)
        local serialized = compiler.serialize(tokens)
        if not serialized then return nil, "serialize failed: " .. path end
        -- scene key: player uses the basename (demo/galgame_demo.ks)
        local key = path:match("([^/\\]+)$") or path
        bundle.scenes[key] = serialized
        -- asset discovery: scan token params for storage/file values
        for _, tok in ipairs(tokens) do
            local p2 = tok[2]
            if type(p2) == "table" then
                for _, k in ipairs({ "storage", "file" }) do
                    local v = p2[k]
                    if type(v) == "string" and v:find("%.%a%a%a?$") then
                        addAsset(v)
                    end
                end
            end
        end
    end
    return bundle, assetCount
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
    local inputs, dirs, outRoot, checkOnly, webOut = {}, {}, OUT_ROOT, false, nil
    local i = 1
    while i <= #arg do
        local a = arg[i]
        if a == "--dir" then
            i = i + 1
            dirs[#dirs + 1] = arg[i]
        elseif a == "--out" then
            i = i + 1
            outRoot = arg[i]
        elseif a == "--web" then
            i = i + 1
            webOut = arg[i]
        elseif a == "--check" then
            checkOnly = true
        elseif a == "-h" or a == "--help" then
            print("Usage: lua scripts/ks_bake.lua <scene.ks> [more.ks ...]")
            print("       lua scripts/ks_bake.lua --dir <dir> [--dir <dir2>] [--out cache/ksc] [--check]")
            print("       lua scripts/ks_bake.lua --dir demo --web cache/story  (web player bundle)")
            print("  bakes scenes into pre-compiled .ksc (mobile zero-parse launch)")
            print("  --out: cache root (default cache/ksc, same as flow.load_scene)")
            print("  --web <dir>: emit cache/story.lua bundle (scenes + assets) for the web player")
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
                -- dir /b on absolute inputs yields full paths; strip the CWD
                -- prefix so io.open works even when the repo path is non-ASCII
                -- (Lua 5.4 io.open is byte-oriented on Windows).
                local cwd = io.popen("cd"):read("*a") or ""
                cwd = cwd:gsub("^%s*(.-)%s*$", "%1"):gsub("\\", "/")
                if cwd ~= "" and line:sub(1, #cwd) == cwd then
                    line = line:sub(#cwd + 2)
                end
                add(line)
            end
            pf:close()
        end
    end

    if #collected == 0 then
        print("error: no .ks scenes given")
        os.exit(1)
    end

    -- Web bundle mode (round 35): one Lua-literal file the web player loads
    -- with zero parse/compile (scenes serialized via compiler.serialize).
    if webOut then
        local bundle, assetCount = bakeWeb(collected)
        if not bundle then
            print("[error] " .. tostring(assetCount))
            os.exit(1)
        end
        local encoded = compiler.encode_lua_literal(bundle)
        local outPath = webOut .. "/story.lua"
        local dirCmd = "mkdir \"" .. webOut .. "\" 2>nul"
        pcall(os.execute, dirCmd)
        local w = io.open(outPath, "w")
        if not w then
            print("[error] cannot write " .. outPath)
            os.exit(1)
        end
        w:write("return " .. encoded .. "\n")
        w:close()
        print(string.format("[web] %s: %d scenes, %d assets", outPath,
            #collected, assetCount or 0))
        os.exit(0)
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
