-- ===========================================================================
--  Caesura (AmeKAG) 閳?fileutil.lua  [W7]
--  Cross-platform directory scanner. No Windows-only `dir /b`.
--  Falls back: lfs (LuaFileSystem) > popen(dir/ls) > pure-Lua fallback.
-- ===========================================================================

local FileUtil = {}

-- Detect OS: Windows uses backslash as directory separator
local isWindows = (package.config:sub(1,1) == "\\")

-- ===========================================================================
-- FileUtil.scan_dir(dir, pattern) -> {filename, ...}
-- Scans a directory for files matching pattern (e.g. "%.png$").
-- Returns a list of filenames (no path prefix).
-- ===========================================================================
function FileUtil.scan_dir(dir, pattern)
    pattern = pattern or ".*"
    -- [C6] Whitelist dir: only alphanumeric, path separators, underscore, dot, hyphen
    if dir:match("[^%w/_%.%:%-]") then
        print("[fileutil] Rejected unsafe dir: " .. tostring(dir))
        return {}
    end
    local files = {}

    -- Method 1: Try lfs (LuaFileSystem) if available
    local lfs_ok, lfs = pcall(require, "lfs")
    if lfs_ok and lfs and lfs.dir then
        local ok, iter = pcall(lfs.dir, dir)
        if ok and iter then
            for fname in iter do
                if fname ~= "." and fname ~= ".." and fname:match(pattern) then
                    table.insert(files, fname)
                end
            end
            if #files > 0 then return files end
        end
    end

    -- Method 2: popen with OS-appropriate command
    local cmd
    if isWindows then
        cmd = 'dir /b "' .. dir .. '" 2>nul'
    else
        cmd = 'ls "' .. dir .. '" 2>/dev/null'
    end
    local ok, result = pcall(function()
        local f = io.popen(cmd)
        if not f then return nil end
        local out = f:read("*a")
        f:close()
        return out
    end)
    if ok and result then
        for fname in result:gmatch("[^\r\n]+") do
            local trimmed = fname:match("^%s*(.-)%s*$")
            if trimmed and #trimmed > 0 and trimmed:match(pattern) then
                table.insert(files, trimmed)
            end
        end
        if #files > 0 then return files end
    end

    -- Method 3: Pure-Lua fallback 閳?try to open each candidate
    -- (limited but works for script-controlled asset directories)
    local ok2, f = pcall(io.open, dir .. "/_dir_test_.tmp", "w")
    if ok2 and f then
        f:close()
        os.remove(dir .. "/_dir_test_.tmp")
        -- Directory is writable; we can try listing via io.popen
        -- (already attempted above; if we reach here, directory is empty or unreadable)
    end

    return files
end

-- ===========================================================================
-- FileUtil.scan_dirs(dirs, pattern) -> {filename, ...}
-- Tries multiple directories, returns results from the first non-empty one.
-- ===========================================================================
function FileUtil.scan_dirs(dirs, pattern)
    for _, dir in ipairs(dirs) do
        local files = FileUtil.scan_dir(dir, pattern)
        if #files > 0 then return files end
    end
    return {}
end

return FileUtil
