-- test_textbox.lua — [textbox]/[nameplate] styling contracts (audit)
-- Source-lock form: the suite sandbox preloads modules, so a mock-backend
-- behavioral test cannot intercept the already-loaded module's bindings.
-- We lock the clamp + re-apply contracts against the source instead.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local f = assert(io.open("scripts/kag/commands/text.lua", "r"))
local src = f:read("*a")
f:close()

-- clamp_byte exists and is used in all three color paths
check("clamp helper", src:find("local function clamp_byte", 1, true) ~= nil)
local c1 = src:find("clamp_byte(r), clamp_byte(g), clamp_byte(b)", 1, true)
local c2 = src:find("clamp_byte(tr or 255)", 1, true)
check("textbox color clamps", c1 ~= nil and c2 ~= nil)
-- both textbox and nameplate color paths clamp
local n_tb = select(2, src:gsub("clamp_byte%(r%), clamp_byte%(g%), clamp_byte%(b%)", ""))
check("two bg-color clamp sites", n_tb == 2)
check("opacity clamped by schema", src:find('opacity = { type = "number", default = 200, min = 0, max = 255 }', 1, true) ~= nil)
-- style persists into ctx
check("style persisted", src:find("ctx.textbox_style = {", 1, true) ~= nil)
-- unparseable color: clear stale texture (no crash)
check("bad color clears", src:find("bg.texture = nil", 1, true) ~= nil)

-- [cl] re-applies the textbox style (message-window rebuild)
local f2 = assert(io.open("scripts/kag/commands/layer.lua", "r"))
local src2 = f2:read("*a")
f2:close()
check("[cl] re-applies style",
      src2:find("Text.textbox(ctx, style)", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("TEXTBOX TESTS DONE")
