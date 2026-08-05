-- ks_check.lua — static .ks contract checker (next-gen standard tool)
-- Usage: lua scripts/ks_check.lua <scene.ks> [more.ks ...]
-- Tokenizes each scene, runs every migrated command's contract over its
-- params, and reports contract violations with scene:line locations.
-- Exit code 0 = clean, 1 = violations found (CI gate).
--
-- Static-validation counterpart of the runtime schema: developers catch
-- bad params before the game ever runs them.

-- Resolve scripts/ from this file's location (works from any CWD).
local BS = string.char(92)  -- backslash
local here = (arg and arg[0] and arg[0]:match("(.*[/" .. BS .. "])")) or "scripts/"
package.path = here .. "?.lua;" .. package.path

local tokenizer = require("tokenizer")
local schema = require("kag.schema")

-- register every command module so all contracts load
pcall(require, "kag.commands.text")
pcall(require, "kag.commands.system")
pcall(require, "kag.commands.audio")
pcall(require, "kag.commands.transition")
pcall(require, "kag.commands.layer")
pcall(require, "kag.commands.vfx")
pcall(require, "kag.commands.video")
pcall(require, "kag.commands.save")
pcall(require, "kag")

local issues = 0

local function report(scene, line, msg)
    issues = issues + 1
    print(string.format("%s:%d: %s", scene, line, msg))
end

local function checkScene(path)
    local f = io.open(path, "r")
    if not f then
        report(path, 0, "cannot open file")
        return
    end
    local text = f:read("*a")
    f:close()
    local tokens = tokenizer.parse(text)
    if not tokens then
        report(path, 0, "tokenize failed")
        return
    end
    -- Pre-compute newline offsets for accurate line numbers.
    local LF = string.char(10)
    local offsets = {}
    local n = 0
    for part in (text .. LF):gmatch("(.-)" .. LF) do
        offsets[#offsets + 1] = n
        n = n + #part + 1
    end
    local pos = 1
    for _, tok in ipairs(tokens) do
        local line = 1
        if tok.type == "command" then
            for li = #offsets, 1, -1 do
                if offsets[li] <= pos then line = li break end
            end
            local cmd = tok.cmd
            if type(cmd) == "string" and schema.isMigrated(cmd) then
                local params = {}
                for _, pair in ipairs(tok.params or {}) do
                    if type(pair) == "table" and pair[1] then
                        local k = pair[1]  -- param name (already the string)
                        params[k] = pair[2]
                    end
                end
                local ok, err2 = pcall(function()
                    schema.coerce(cmd, params, { current_scene = path, token_index = line })
                end)
                if not ok then
                    report(path, line, tostring(err2))
                end
            end
        end
        pos = pos + #(tok.content or tok.cmd or "") + 1
    end
end

if #arg == 0 then
    print("usage: lua scripts/ks_check.lua <scene.ks> [more ...]")
    os.exit(2)
end
for _, p in ipairs(arg) do
    checkScene(p)
end
if issues > 0 then
    print(string.format("%d contract violation(s) found", issues))
    os.exit(1)
end
print("OK: all scenes pass contract checks")
os.exit(0)
