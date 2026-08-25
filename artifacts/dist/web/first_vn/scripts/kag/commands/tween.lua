-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/tween.lua
--  Declarative [tween] command family (Ren'Py ATL minimal subset).
--
--  One-line "in N ms, transition attr A → B on a layer/sprite" with an
--  easing function, replacing hand-written multi-command sequences like
--  [sprite_fade]+[sprite_move]+[wait].
--
--  Design: a NEW tween manager lives in ctx.tweens. Each started tween is
--  pushed there and advanced per-frame by kag_runner.update (which calls
--  TweenCommands.update(ctx, dt_ms)); completed tweens clear themselves.
--  For wait=true the handler ALSO drives the tween inline through the
--  same code path inside a blocking Operation loop (isomorphic to [wait] /
--  [sprite_fade]), so the per-frame hook is only required for the
--  fire-and-forget (wait=false) case.
-- =============================================================================

local layers = require("layers")
local Operation = require("kag.operation")

-- ═══════════════════════════════════════════════════════════════════════════
--  Easing functions (matches the engine LUT easing vocabulary in
--  transition.lua: linear / ease_in / ease_out / ease_in_out / back_out).
--  Every easing maps the normalized time t ∈ [0,1] onto [0,1]; endpoints
--  are exact (e(0)=0, e(1)=1) EXCEPT back_out which overshoots past 1.
-- ═══════════════════════════════════════════════════════════════════════════

local Ease = {
  linear = function(t) return t end,
  ease_in = function(t) return t * t end,
  ease_out = function(t) return t * (2 - t) end,
  ease_in_out = function(t)
    t = t * 2
    if t < 1 then return 0.5 * t * t end
    t = t - 1
    return -0.5 * (t * (t - 2) - 1)
  end,
  back_out = function(t)
    local c1 = 1.70158
    local c3 = c1 + 1
    t = t - 1
    return 1 + c3 * t * t * t + c1 * t * t
  end,
}

local function ease_by_name(name)
  return Ease[name] or Ease.linear
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layer resolution. target is a KAG speaker OR a layer name:
--    * speaker  → the character sprite layer '_char_<speaker>'
--    * layer    → an explicit layer name (bg/fg/image layers, etc.)
--  A bare layer name wins if it exists; otherwise the _char_ convention
--  is tried. Returns the layer node or nil.
-- ═══════════════════════════════════════════════════════════════════════════

local function resolve_layer(target)
  if type(target) ~= "string" or #target == 0 then return nil end
  local node = layers.get(target) or layers.find(target)
  if node then return node end
  return layers.get("_char_" .. target) or layers.find("_char_" .. target)
end

-- Apply the interpolated value for one tween to its layer node.
--   attr x/y   : layers.move_layer (marks dirty)
--   attr alpha : layers.set_layer_opacity (keeps node.opacity + node.alpha in sync)
--   attr scale : uniform scaleX/scaleY
local function apply_step(node, attr, value)
  if not node then return end
  if attr == "x" or attr == "y" then
    if attr == "x" then
      layers.move_layer(node, value, nil)
    else
      layers.move_layer(node, nil, value)
    end
  elseif attr == "alpha" then
    layers.set_layer_opacity(node, value)
  elseif attr == "scale" then
    if node.scaleX ~= nil and node.scaleY ~= nil then
      node.scaleX = value
      node.scaleY = value
      layers.mark_dirty(node)
    end
  end
end

-- Read the current value of an attribute on a layer node (used as the
-- implicit `from` when from is omitted).
local function read_attr(node, attr)
  if not node then return 0 end
  if attr == "x" then return tonumber(node.x) or 0 end
  if attr == "y" then return tonumber(node.y) or 0 end
  if attr == "alpha" then return tonumber(node.opacity or (node.alpha and node.alpha * 255) or 255) or 255 end
  if attr == "scale" then return tonumber(node.scaleX or node.scale or 1.0) or 1.0 end
  return 0
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Shared per-tween step: advance a tween by dt_ms and write its value.
--  Returns 'done' when the tween is finished (delay consumed + duration
--  elapsed), else 'running'. Finished tweens snap to the exact value.
-- ═══════════════════════════════════════════════════════════════════════════

local function step_tween(tw, dt_ms)
  if tw.done then return "done" end
  -- Single timeline: delay and duration are sequential phases of one
  -- clock. Movement begins only once total elapsed exceeds the delay.
  tw.t = (tw.t or 0) + dt_ms
  if tw.t <= tw.delay then
    return "running"  -- still in the delay phase
  end
  local motion_t = (tw.t - tw.delay) / tw.dur
  local raw_t = math.min(1.0, motion_t)
  local t = tw.ease_fn(raw_t)
  local value = tw.from + (tw.to - tw.from) * t
  apply_step(tw.node, tw.attr, value)
  if motion_t >= 1.0 then
    apply_step(tw.node, tw.attr, tw.to)  -- exact endpoint
    tw.done = true
    return "done"
  end
  return "running"
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Schema contract → LSP / ks_check / docs all benefit automatically.
--  from/to support ${expr} interpolation (declared string + interpolate,
--  then tonumber'd here — mirrors schema.interpolate on text params).
-- ═══════════════════════════════════════════════════════════════════════════

local schema = require("kag.schema")
schema.define("tween", {
  _meta = {
    category = "layer",
    blocking = true,  -- wait defaults true; false (fire-and-forget) set by the param
    desc = "declaratively tween a layer/sprite attribute from A to B in N ms",
  },
  target = { type = "string", required = true },
  attr = {
    type = "enum",
    required = true,
    values = { "x", "y", "alpha", "scale" },
  },
  from = { type = "string", interpolate = true },   -- tonumber'd; omit → current value
  to = { type = "string", interpolate = true, required = true },
  dur = { type = "number", required = true, min = 100, max = 30000 },
  delay = { type = "number", default = 0, min = 0, max = 30000 },
  ease = {
    type = "enum",
    default = "linear",
    values = { "linear", "ease_in", "ease_out", "ease_in_out", "back_out" },
  },
  wait = { type = "boolean", default = true },
})

local TweenCommands = {}

-- ═══════════════════════════════════════════════════════════════════════════
--  Per-frame driver for fire-and-forget tweens. kag_runner.update calls
--  this every frame with the frame delta (ms). Advances ctx.tweens,
--  removes finished entries, returns the number still active.
-- ═══════════════════════════════════════════════════════════════════════════

function TweenCommands.update(ctx, dt_ms)
  if type(ctx) ~= "table" or type(ctx.tweens) ~= "table" then return 0 end
  dt_ms = tonumber(dt_ms) or 16
  local active = 0
  local i = 1
  local list = ctx.tweens
  while i <= #list do
    local tw = list[i]
    if tw.cancelled then
      table.remove(list, i)
    else
      local st = step_tween(tw, dt_ms)
      if st == "done" then
        table.remove(list, i)  -- clear finished tween THIS frame
      else
        active = active + 1
        i = i + 1
      end
    end
  end
  return active
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [tween target= speaker|layer attr= x|y|alpha|scale
--        from= A to= B dur= N delay= M ease= ease_in|... wait= false]
--    target  (req) speaker or layer name (sprite _char_/<name> fallback)
--    attr    (req) x | y | alpha | scale
--    from    (opt) start value; default = current value. ${expr} allowed.
--    to      (req) target value. ${expr} allowed.
--    dur     (req) duration in ms (100–30000)
--    delay   (opt) delay before motion starts (0–30000)
--    ease    (opt) linear|ease_in|ease_out|ease_in_out|back_out (default linear)
--    wait    (opt) true=block until done (default); false=fire-and-forget
-- ═══════════════════════════════════════════════════════════════════════════

function TweenCommands.tween(ctx, params)
  local target = params.target
  local node = resolve_layer(target)
  if not node then
    print("[tween] no sprite/layer found for target: " .. tostring(target))
    return
  end

  local attr = params.attr
  local to = tonumber(params.to)
  if to == nil then
    print("[tween] attr '" .. attr .. "' target 'to' is not a number: " .. tostring(params.to))
    return
  end
  local from = params.from ~= nil and params.from ~= ""
    and tonumber(params.from) or read_attr(node, attr)
  if from == nil then from = read_attr(node, attr) end

  local dur = tonumber(params.dur) or 1000
  if dur < 100 then dur = 100 end
  if dur > 30000 then dur = 30000 end
  local delay = tonumber(params.delay) or 0
  local ease_fn = ease_by_name(params.ease)

  local tw = {
    node = node, attr = attr,
    from = from, to = to, dur = dur, delay = delay,
    ease_fn = ease_fn, t = 0, done = false, cancelled = false,
  }

  -- Fire-and-forget: push + return immediately; the per-frame hook advances it.
  if params.wait == false then
    ctx.tweens = ctx.tweens or {}
    table.insert(ctx.tweens, tw)
    return
  end

  -- Blocking: drive the SAME step logic inside an Operation loop (isomorphic
  -- to [wait] / [sprite_fade]): yield each frame, scheduler feeds real dt.
  local operation <close> = Operation.start(ctx)
  local ct = operation.token
  while not ct.cancelled and not ctx.stop_flag and not ctx._next_index
        and tw.done == false do
    local dt = coroutine.yield() or 16
    if ct.cancelled then break end
    step_tween(tw, dt)
  end
  if tw.done then
    apply_step(node, attr, to)  -- snap exact endpoint
  end
  if not ct.cancelled then
    operation:complete()
  end
end

return TweenCommands