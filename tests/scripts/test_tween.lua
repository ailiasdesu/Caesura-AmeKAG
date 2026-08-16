-- test_tween.lua — [tween] declarative tween command family
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
  if cond then print("PASS " .. name) passed = passed + 1
  else print("FAIL " .. name) failed = failed + 1 end
end

-- Mock the layers module: capture move/opacity/scale writes on nodes.
local moves, opacities, dirty = 0, 0, 0
local layers_backup = package.loaded["layers"]
package.loaded["layers"] = {
  get = function(name)
    if name == "_char_Aoi" or name == "sprite" or name == "bg" then
      return { name = name, x = 100, y = 100, opacity = 255, alpha = 1.0, scaleX = 1.0, scaleY = 1.0 }
    end
    return nil
  end,
  find = function() return nil end,
  move_layer = function(node, x, y)
    if x ~= nil then node.x = x end
    if y ~= nil then node.y = y end
    moves = moves + 1
  end,
  set_layer_opacity = function(node, o) node.opacity = o; node.alpha = o / 255 end,
  mark_dirty = function() dirty = dirty + 1 end,
}

local KAG = require("kag")
local schema = require("kag.schema")
local Tween = require("kag.commands.tween")
-- dispatch helper: coerce params through the schema (as the scheduler does),
-- then invoke the handler. Returns the handler result / fired tween ctx.
local function run_tween(ctx, params)
  local p = schema.coerce("tween", params, ctx)
  return Tween.tween(ctx, p)
end

-- 1. Schema contract
local s = schema.coerce("tween", { target="bg", attr="alpha", to="0", dur="500" }, {})
check("tween to kept", s.to == "0" and tonumber(s.to) == 0)
check("tween dur kept", s.dur == 500)
check("tween ease default linear", s.ease == "linear")
check("tween wait default true", s.wait == true)
check("tween delay default 0", s.delay == 0)
local ok_e = pcall(schema.coerce, "tween", { target="bg", attr="bad", to="0", dur="500" }, {})
check("tween attr enum rejected", not ok_e)
local co = schema.coerce("tween", { target="bg", attr="x", to="9", dur="50" }, {})
check("tween dur clamped min", co.dur == 100)
local ok_req = pcall(schema.coerce, "tween", { attr="x", to="5", dur="300" }, {})
check("tween target required", not ok_req)
local ok_req2 = pcall(schema.coerce, "tween", { target="bg", attr="x", dur="300" }, {})
check("tween to required", not ok_req2)

-- 2. Non-blocking wait=false: registers, advances, clears
do
  local node = { name="sprite", x=0, y=0, opacity=255, alpha=1.0, scaleX=1.0, scaleY=1.0 }
  local ctx = { f={}, sf={}, tf={}, tweens={} }
  package.loaded["layers"].get = function() return node end
  run_tween(ctx, { target="sprite", attr="x", from="0", to="100", dur="100", wait="false" })
  check("wait=false returns immediately", #ctx.tweens == 1)
  local n = Tween.update(ctx, 50)
  check("non-block x after 50ms ~=50", math.abs(node.x - 50) < 0.001 and n == 1)
  local n2 = Tween.update(ctx, 50)
  check("non-block x reaches 100", math.abs(node.x - 100) < 0.001)
  check("non-block cleared from tweens", n2 == 0 and #ctx.tweens == 0)
end

-- 3. Ease matrix: every ease lands exactly on `to` at completion
local eases = { "linear", "ease_in", "ease_out", "ease_in_out", "back_out" }
for _, en in ipairs(eases) do
  local node = { name="sprite", x=0, y=0, opacity=255, alpha=1.0, scaleX=1.0, scaleY=1.0 }
  local ctx = { tweens={} }
  package.loaded["layers"].get = function() return node end
  run_tween(ctx, { target="sprite", attr="x", from="10", to="210", dur="200", ease=en, wait="false" })
  Tween.update(ctx, 100); Tween.update(ctx, 100); Tween.update(ctx, 200);
  check("ease " .. en .. " endpoint exact", math.abs(node.x - 210) < 0.001 and #ctx.tweens == 0)
end

-- 4. from omitted -> current value (read_attr)
do
  local node = { name="sprite", x=40, y=0, opacity=255, alpha=1.0, scaleX=1.0, scaleY=1.0 }
  local ctx = { tweens={} }
  package.loaded["layers"].get = function() return node end
  run_tween(ctx, { target="sprite", attr="x", to="90", dur="100", wait="false" })
  Tween.update(ctx, 100);
  check("from defaults to current", math.abs(node.x - 90) < 0.001)
end

-- 5. delay: no motion before delay expires
do
  local node = { name="sprite", x=0, y=0, opacity=255, alpha=1.0, scaleX=1.0, scaleY=1.0 }
  local ctx = { tweens={} }
  package.loaded["layers"].get = function() return node end
  run_tween(ctx, { target="sprite", attr="x", from="0", to="100", dur="100", delay="300", ease="linear", wait="false" })
  Tween.update(ctx, 200);
  check("delay no motion before 300ms", math.abs(node.x - 0) < 0.001 and #ctx.tweens == 1)
  Tween.update(ctx, 50);
  check("delay still no motion at 250ms", math.abs(node.x - 0) < 0.001)
  Tween.update(ctx, 100);
  check("delay motion after delay x=50", math.abs(node.x - 50) < 0.001)
  Tween.update(ctx, 300);
  check("delay completes", math.abs(node.x - 100) < 0.001 and #ctx.tweens == 0)
end

-- 6. Blocking wait=true (default): coroutine yields until done
do
  local node = { name="bg", x=0, y=0, opacity=255, alpha=1.0, scaleX=1.0, scaleY=1.0 }
  package.loaded["layers"].get = function() return node end
  package.loaded["layers"].find = function() return nil end
  local ctx = { f={}, sf={}, tf={}, active_operations={}, stop_flag=false, _next_index=nil }
  local co = coroutine.create(function() run_tween(ctx, { target="bg", attr="alpha", from="255", to="0", dur="200", ease="linear" }) end)
  local ok1 = coroutine.resume(co)
  check("blocking yields once running", ok1 and coroutine.status(co) == "suspended")
  local ok2 = coroutine.resume(co, 100)
  check("blocking ~127.5 at 100ms", math.abs(node.opacity - 127.5) < 1.0 and ok2)
  local ok3 = coroutine.resume(co, 100)
  check("blocking completes opacity 0", ok3 and coroutine.status(co) == "dead" and math.abs(node.opacity - 0) < 0.001)
end

-- 7. ${expr} from/to interpolation
do
  local node = { name="sprite", x=0, y=0, opacity=255, alpha=1.0, scaleX=1.0, scaleY=1.0 }
  package.loaded["layers"].get = function() return node end
  local ctx = { f={ startX=5 }, tf={ endX=305 }, tweens={} }
  local fe = "${f.startX}"
  local te = "${tf.endX}"
  run_tween(ctx, { target="sprite", attr="x", from=fe, to=te, dur="200", wait="false" })
  Tween.update(ctx, 200);
  check("interp from/to applied", math.abs(node.x - 305) < 0.001 and #ctx.tweens == 0)
end

-- 8. scale tween
do
  local node = { name="sprite", x=0, y=0, opacity=255, alpha=1.0, scaleX=1.0, scaleY=1.0 }
  local ctx = { tweens={} }
  package.loaded["layers"].get = function() return node end
  run_tween(ctx, { target="sprite", attr="scale", from="1.0", to="2.0", dur="100", wait="false" })
  Tween.update(ctx, 100);
  check("scale tween reaches 2.0", math.abs(node.scaleX - 2.0) < 0.001 and math.abs(node.scaleY - 2.0) < 0.001)
end

package.loaded["layers"] = layers_backup
if failed > 0 then print("TWEEN TESTS: " .. failed .. " failed"); os.exit(1) end
print("TWEEN TESTS DONE (" .. passed .. " passed)")