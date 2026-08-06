-- test_gallery_bare.lua — [gallery]/[ending] bare-arg coverage (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local tokenizer = require("tokenizer")
local scheduler = require("scheduler")

-- [gallery 2] routes the bare start id into Gallery.show
local shown = {}
local gallery_backup = package.loaded["gallery"]
package.loaded["gallery"] = { show = function(ctx, id) shown[#shown + 1] = id end }
local toks = tokenizer.parse("[gallery 2]")
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = toks, token_index = 1, current_scene = "t.ks",
    label_index = {}, _whileIterByScene = { ["t.ks"] = 0 } }
local co = coroutine.create(function() scheduler.run(ctx, toks, 1) end)
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
check("gallery bare routed", shown[1] == "2")
package.loaded["gallery"] = gallery_backup

-- [ending e1] bare id
local toks2 = tokenizer.parse("[ending e1]")
local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = toks2, token_index = 1, current_scene = "t.ks",
    label_index = {}, _whileIterByScene = { ["t.ks"] = 0 } }
local co2 = coroutine.create(function() scheduler.run(ctx2, toks2, 1) end)
while coroutine.status(co2) ~= "dead" do coroutine.resume(co2) end
check("ending bare id", ctx2.seen_endings and ctx2.seen_endings.e1 ~= nil)

-- [music] no-arg no crash
local music_shown = {}
local mr_backup = package.loaded["music_room"]
package.loaded["music_room"] = { show = function(ctx) music_shown[#music_shown + 1] = true end }
local toks3 = tokenizer.parse("[music]")
local ctx3 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = toks3, token_index = 1, current_scene = "t.ks",
    label_index = {}, _whileIterByScene = { ["t.ks"] = 0 } }
local co3 = coroutine.create(function() scheduler.run(ctx3, toks3, 1) end)
while coroutine.status(co3) ~= "dead" do coroutine.resume(co3) end
check("music no-arg", #music_shown == 1)
package.loaded["music_room"] = mr_backup

if failed > 0 then os.exit(1) end
print("GALLERY BARE TESTS DONE")
