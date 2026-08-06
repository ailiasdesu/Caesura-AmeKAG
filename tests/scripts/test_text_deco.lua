-- test_text_deco.lua — [ruby]/[nameplate]/[textbox] contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local schema = require("kag.schema")

-- textbox schema: w/h clamps, color default, opacity 0..255
local tb = schema.coerce("textbox", { w = "99999", h = "-5", opacity = "999" }, {})
check("textbox w clamped", tb.w == 4096)
check("textbox h clamped", tb.h == 32)
check("textbox opacity clamped", tb.opacity == 255)
check("textbox color default", tb.color == "0,0,0")

-- nameplate schema: defaults + clamps
local np = schema.coerce("nameplate", { w = "1", text_color = "1,2,3" }, {})
check("nameplate w clamped", np.w == 32)
check("nameplate text_color kept", np.text_color == "1,2,3")
check("nameplate color default", np.color == "0,0,0")

-- ruby schema: empty text guard (source)
local f = assert(io.open("scripts/kag/commands/text.lua", "r"))
local src = f:read("*a")
f:close()
check("ruby empty guard", src:find('if text == "" then return end', 1, true) ~= nil)
check("ruby add_ruby", src:find("TextScene.add_ruby(ctx, text, ruby_text", 1, true) ~= nil)

-- nameplate re-render on speaker change
check("nameplate re-render", src:find("TextCommands._renderNameplate(ctx, ctx.current_speaker)", 1, true) ~= nil)

-- textbox style persisted for [cl] rebuild (contract)
check("textbox style persisted", src:find("ctx.textbox_style = {", 1, true) ~= nil)

-- handlers registered
check("ruby registered", type(KAG.ruby) == "function")
check("nameplate registered", type(KAG.nameplate) == "function")
check("textbox registered", type(KAG.textbox) == "function")

if failed > 0 then os.exit(1) end
print("TEXT DECO TESTS DONE")
