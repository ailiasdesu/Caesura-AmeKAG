-- U11: dictionary preparation must not alter the currently visible language.
package.path = "scripts/?.lua;" .. package.path
local locale = require("i18n")
locale.load("en")

local passed, failed = 0, 0
local function check(name, ok)
    if ok then passed = passed + 1
    else failed = failed + 1; print("FAIL: " .. name) end
end

local files = {
    ["assets/lang/u11_good.lua"] = "return {hello='prepared', lines={line_a='translated'}, count={one='one',other='many'}}",
    ["assets/lang/u11_bad.lua"] = "return { broken = ",
    ["assets/lang/u11_function.lua"] = "return {hello=function() return 'unsafe' end}",
    ["assets/lang/u11_bad_lines.lua"] = "return {lines='not a line dictionary'}",
    ["assets/lang/u11_nested_line.lua"] = "return {lines={line_a={other='not a string'}}}",
    ["assets/lang/u11_fallback.lua"] = "return {fallback_only='saved default'}",
}
local original_open, reads = io.open, 0
io.open = function(path, mode)
    if files[path] then
        reads = reads + 1
        local text = files[path]
        return { read = function() return text end, close = function() return true end }
    end
    return original_open(path, mode)
end

local old_code, old_strings, old_lines, old_fallback, old_default =
    locale.current, locale.strings, locale.lines, locale.fallback, locale.default_language
local ok, prepared = pcall(function() return locale.prepare("u11_good") end)
check("valid dictionary prepares", ok and type(prepared) == "table")
check("prepare leaves language code unchanged", locale.current == old_code)
check("prepare leaves dictionary identity unchanged", locale.strings == old_strings)
check("prepare leaves line dictionary unchanged", locale.lines == old_lines)
check("prepare leaves fallback unchanged", locale.fallback == old_fallback)
files["assets/lang/u11_good.lua"] = "return {hello='changed after prepare'}"
local reads_before_commit = reads
local committed = ok and pcall(function() locale.commit(prepared) end)
check("prepared dictionary commits", committed)
check("commit publishes the prepared language", locale.current == "u11_good")
check("commit uses the captured bytes", locale.strings.hello == "prepared")
check("commit includes prepared line translations", locale.lines.line_a == "translated")
check("commit retains plural forms", type(locale.strings.count) == "table"
    and locale.strings.count.other == "many")
check("commit performs no second file read", reads == reads_before_commit)

for _, code in ipairs({"u11_bad", "u11_function", "u11_bad_lines", "u11_nested_line", "../outside"}) do
    local before = {locale.current, locale.strings, locale.lines, locale.fallback}
    local accepted = pcall(function() return locale.prepare(code) end)
    check(code .. " is rejected", not accepted)
    check(code .. " preserves all visible locale state",
        locale.current == before[1] and locale.strings == before[2]
        and locale.lines == before[3] and locale.fallback == before[4])
end

local default_before=locale.default_language
local explicit=locale.prepare("u11_good","u11_fallback")
check("explicit fallback prepares its dictionary",explicit.fallback.fallback_only=="saved default")
check("preparing fallback does not change active default",locale.default_language==default_before)
locale.commit(explicit)
check("commit restores the matching fallback selection",locale.default_language=="u11_fallback"
    and locale.fallback.fallback_only=="saved default")
check("malformed fallback lines reject before commit",
    not pcall(locale.prepare,"u11_good","u11_bad_lines"))
io.open = original_open
locale.current, locale.strings, locale.lines, locale.fallback =
    old_code, old_strings, old_lines, old_fallback
locale.default_language=old_default
print(string.format("U11 I18N RESTORE: %d passed, %d failed", passed, failed))
os.exit(failed == 0 and 0 or 1)
