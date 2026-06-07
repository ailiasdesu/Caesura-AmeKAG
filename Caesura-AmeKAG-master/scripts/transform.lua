-- ═══════════════════════════════════════════════════════════════════════════
--  Caesura (AmeKAG) — transform.lua
--  Affine transformations: scale, rotate, translate, matrix operations,
--  stretch blit, affine blit, and animated interpolation (move command).
--  Spec [2.4]: 10 resampling filters from krkrz WeightFunctor.h:
--    stNearest, stLinear, stCubic, stLanczos3, stSpline16, stGaussian,
--    stBlackmanSinc, stMitchell, stAreaAverage, stKaiser.
--  Krkrz reference: WeightFunctor.h, CharacterData.cpp (blur/transform).
-- ═══════════════════════════════════════════════════════════════════════════

local backend = require("backend")
local rtt     = require("rtt")

local Transform = {}

-- ═══════════════════════════════════════════════════════════════════════════
--  Resampling filter enum — matches krkrz stType (spec [2.4] Table 2.4-1)
-- ═══════════════════════════════════════════════════════════════════════════

Transform.Filter = {
    NEAREST        = 0,   -- stNearest      — nearest-neighbour
    LINEAR         = 1,   -- stLinear       — bilinear interpolation (default)
    CUBIC          = 2,   -- stCubic        — bicubic (Mitchell-Netravali)
    LANCZOS3       = 3,   -- stLanczos3     — Lanczos window, radius 3
    SPLINE16       = 4,   -- stSpline16     — 4×4 spline kernel
    GAUSSIAN       = 5,   -- stGaussian     — Gaussian blur kernel
    BLACKMAN_SINC  = 6,   -- stBlackmanSinc — Blackman-windowed sinc
    MITCHELL       = 7,   -- stMitchell     — Mitchell-Netravali (B=C=1/3)
    AREA_AVERAGE   = 8,   -- stAreaAverage  — box area average
    KAISER         = 9,   -- stKaiser       — Kaiser-windowed sinc
}

Transform.FilterName = {
    [0]="nearest",  [1]="linear",   [2]="cubic",
    [3]="lanczos3", [4]="spline16", [5]="gaussian",
    [6]="blackman_sinc", [7]="mitchell", [8]="area_average",
    [9]="kaiser",
}

-- Resolve filter name or id → filter id
function Transform.resolve_filter(f)
    if type(f) == "number" then return f end
    if not f then return Transform.Filter.LINEAR end
    local map = {
        nearest=0, linear=1, cubic=2, lanczos3=3, lanczos=3,
        spline16=4, spline=4, gaussian=5, blackman_sinc=6, blackmansinc=6,
        blackman=6, mitchell=7, area_average=8, area=8, kaiser=9,
    }
    return map[tostring(f):lower()] or Transform.Filter.LINEAR
end

-- ═══════════════════════════════════════════════════════════════════════════
--  2D Affine Matrix (row-major, 2×3 packed in Lua table)
--    [ a  c  tx ]    x'' = a·x + c·y + tx
--    [ b  d  ty ]    y'' = b·x + d·y + ty
--    [ 0  0  1  ]
-- ═══════════════════════════════════════════════════════════════════════════

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.newMatrix() → identity matrix {a,c,tx, b,d,ty}
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.newMatrix()
    return { a = 1, c = 0, tx = 0,
             b = 0, d = 1, ty = 0 }
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.identity(m) → reset to identity
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.identity(m)
    m.a, m.c, m.tx = 1, 0, 0
    m.b, m.d, m.ty = 0, 1, 0
    return m
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.copy(src, dst) — copy matrix values
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.copy(src, dst)
    dst = dst or Transform.newMatrix()
    dst.a, dst.c, dst.tx = src.a, src.c, src.tx
    dst.b, dst.d, dst.ty = src.b, src.d, src.ty
    return dst
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.clone(m) → new matrix with same values
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.clone(m)
    return { a = m.a, c = m.c, tx = m.tx,
             b = m.b, d = m.d, ty = m.ty }
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.multiply(a, b, out) → out = a × b
--    Combines two affine transforms. out defaults to a new matrix.
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.multiply(a, b, out)
    out = out or Transform.newMatrix()
    local na = a.a * b.a + a.c * b.b
    local nc = a.a * b.c + a.c * b.d
    local ntx = a.a * b.tx + a.c * b.ty + a.tx
    local nb = a.b * b.a + a.d * b.b
    local nd = a.b * b.c + a.d * b.d
    local nty = a.b * b.tx + a.d * b.ty + a.ty
    out.a, out.c, out.tx = na, nc, ntx
    out.b, out.d, out.ty = nb, nd, nty
    return out
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.translate(m, dx, dy)
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.translate(m, dx, dy)
    m.tx = m.tx + (dx or 0)
    m.ty = m.ty + (dy or 0)
    return m
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.scale(m, sx, sy, cx, cy) — scale around center (cx, cy)
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.scale(m, sx, sy, cx, cy)
    cx = cx or 0; cy = cy or 0
    local s = sy or sx
    -- translate to origin, scale, translate back
    m.tx = m.tx - cx
    m.ty = m.ty - cy
    m.a, m.c = m.a * sx, m.c * s
    m.b, m.d = m.b * s,  m.d * s
    m.tx = m.tx + cx
    m.ty = m.ty + cy
    return m
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.rotate(m, angle_deg, cx, cy) — rotate around center
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.rotate(m, angle_deg, cx, cy)
    cx = cx or 0; cy = cy or 0
    local rad = (angle_deg or 0) * math.pi / 180
    local cosA, sinA = math.cos(rad), math.sin(rad)
    m.tx = m.tx - cx; m.ty = m.ty - cy
    local na = m.a * cosA + m.c * sinA
    local nc = -m.a * sinA + m.c * cosA
    local nb = m.b * cosA + m.d * sinA
    local nd = -m.b * sinA + m.d * cosA
    m.a, m.c, m.b, m.d = na, nc, nb, nd
    m.tx = m.tx + cx; m.ty = m.ty + cy
    return m
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.skew(m, skewX_deg, skewY_deg) — shear transform
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.skew(m, skewX_deg, skewY_deg)
    local sx = math.tan((skewX_deg or 0) * math.pi / 180)
    local sy = math.tan((skewY_deg or 0) * math.pi / 180)
    local na = m.a + m.c * sy
    local nc = m.a * sx + m.c
    local nb = m.b + m.d * sy
    local nd = m.b * sx + m.d
    m.a, m.c, m.b, m.d = na, nc, nb, nd
    return m
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.invert(m, out) → invertible matrix | nil, error
--    Computes the inverse of an affine 2D matrix.
--    Returns out (or a new matrix) on success, nil + error on singular.
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.invert(m, out)
    local det = m.a * m.d - m.c * m.b
    if math.abs(det) < 1e-12 then
        return nil, "singular matrix"
    end
    local invDet = 1.0 / det
    out = out or Transform.newMatrix()
    out.a  =  m.d * invDet
    out.c  = -m.c * invDet
    out.tx = (m.c * m.ty - m.d * m.tx) * invDet
    out.b  = -m.b * invDet
    out.d  =  m.a * invDet
    out.ty = (m.b * m.tx - m.a * m.ty) * invDet
    return out
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.transform_point(m, x, y) → x'', y''
--    Apply the affine matrix to a point.
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.transform_point(m, x, y)
    return m.a * x + m.c * y + m.tx,
           m.b * x + m.d * y + m.ty
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.transform_rect(m, x, y, w, h) → x', y', w', h'
--    Transform a rectangle through the affine matrix (axis-aligned bbox).
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.transform_rect(m, x, y, w, h)
    local p1x, p1y = Transform.transform_point(m, x, y)
    local p2x, p2y = Transform.transform_point(m, x + w, y)
    local p3x, p3y = Transform.transform_point(m, x, y + h)
    local p4x, p4y = Transform.transform_point(m, x + w, y + h)
    local minX = math.min(p1x, p2x, p3x, p4x)
    local minY = math.min(p1y, p2y, p3y, p4y)
    local maxX = math.max(p1x, p2x, p3x, p4x)
    local maxY = math.max(p1y, p2y, p3y, p4y)
    return minX, minY, maxX - minX, maxY - minY
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.interpolate(a, b, t, out) → linear matrix interpolation
--    t in [0, 1]: 0 → a, 1 → b
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.interpolate(a, b, t, out)
    out = out or Transform.newMatrix()
    local t1 = 1 - t
    out.a  = a.a  * t1 + b.a  * t
    out.c  = a.c  * t1 + b.c  * t
    out.tx = a.tx * t1 + b.tx * t
    out.b  = a.b  * t1 + b.b  * t
    out.d  = a.d  * t1 + b.d  * t
    out.ty = a.ty * t1 + b.ty * t
    return out
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.stretch_blt(dst_rt, dst_rect, src_rt, src_rect, filter_type)
--    Spec [2.4]: GPU stretch blit with resampling.
--    dst/src: RTT handles; rects: {x,y,w,h}; filter_type: name or id.
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.stretch_blt(dst_rt, dst_rect, src_rt, src_rect, filter_type)
    local filterId = Transform.resolve_filter(filter_type)
    backend.submit_stretch_blt(dst_rt, dst_rect, src_rt, src_rect, filterId)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.affine_blt(dst_rt, dst_rect, src_rt, src_rect, matrix, filter_type)
--    Spec [2.4]: GPU affine transform blit.
--    matrix: {a,c,tx, b,d,ty} from Transform.newMatrix()
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.affine_blt(dst_rt, dst_rect, src_rt, src_rect, matrix, filter_type)
    local filterId = Transform.resolve_filter(filter_type)
    backend.submit_affine_blt(dst_rt, dst_rect, src_rt, src_rect, matrix, filterId)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Easing functions — used by animate() for non-linear interpolation
-- ═══════════════════════════════════════════════════════════════════════════

Transform.Easing = {}

-- Linear (no easing)
function Transform.Easing.linear(t) return t end

-- Quad
function Transform.Easing.easeInQuad(t)  return t * t end
function Transform.Easing.easeOutQuad(t) return t * (2 - t) end
function Transform.Easing.easeInOutQuad(t)
    t = t * 2
    if t < 1 then return 0.5 * t * t end
    t = t - 1
    return -0.5 * (t * (t - 2) - 1)
end

-- Cubic
function Transform.Easing.easeInCubic(t)  return t * t * t end
function Transform.Easing.easeOutCubic(t)
    t = t - 1
    return t * t * t + 1
end
function Transform.Easing.easeInOutCubic(t)
    t = t * 2
    if t < 1 then return 0.5 * t * t * t end
    t = t - 2
    return 0.5 * (t * t * t + 2)
end

-- Sinusoidal
function Transform.Easing.easeInSine(t)   return 1 - math.cos(t * math.pi / 2) end
function Transform.Easing.easeOutSine(t)  return math.sin(t * math.pi / 2) end
function Transform.Easing.easeInOutSine(t) return -0.5 * (math.cos(math.pi * t) - 1) end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.easing_by_name(name) → easing function
--    Names: "linear", "easeInQuad", "easeOutQuad", "easeInOutQuad",
--           "easeInCubic", "easeOutCubic", "easeInOutCubic",
--           "easeInSine", "easeOutSine", "easeInOutSine"
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.easing_by_name(name)
    local fn = Transform.Easing[name or "linear"]
    return fn or Transform.Easing.linear
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.animate(layer, params, coro) — blocking coroutine animation
--    params: {x, y, scaleX, scaleY, rotation, duration, easing, filter,
--             on_update=fn(layer, t)}
--    Backs the KAG [move] command per spec [2.4].
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.animate(layer, params, coro)
    local dur        = tonumber(params.duration) or 1000
    local from_x     = layer.x or 0
    local from_y     = layer.y or 0
    local from_sx    = layer.scaleX or 1
    local from_sy    = layer.scaleY or 1
    local from_rot   = layer.rotation or 0

    local to_x       = tonumber(params.x) or from_x
    local to_y       = tonumber(params.y) or from_y
    local to_sx      = tonumber(params.scaleX or params.scale) or from_sx
    local to_sy      = tonumber(params.scaleY or params.scale) or from_sy
    local to_rot     = tonumber(params.rotation or params.rotate) or from_rot

    local easing_fn  = Transform.easing_by_name(params.easing)
    local elapsed    = 0

    while elapsed < dur do
        local dt = 16  -- ~60fps default
        if coro and coro.yield then
            dt = coro.yield() or 16
        end
        elapsed = elapsed + dt
        local raw_t = math.min(1.0, elapsed / dur)
        local t = easing_fn(raw_t)

        layer.x        = from_x  + (to_x  - from_x)  * t
        layer.y        = from_y  + (to_y  - from_y)  * t
        layer.scaleX   = from_sx + (to_sx - from_sx) * t
        layer.scaleY   = from_sy + (to_sy - from_sy) * t
        layer.rotation = from_rot + (to_rot - from_rot) * t
        layer.dirty    = true

        if params.on_update then
            params.on_update(layer, raw_t)
        end

        if not coro then break end
    end

    -- snap to final values
    layer.x        = to_x
    layer.y        = to_y
    layer.scaleX   = to_sx
    layer.scaleY   = to_sy
    layer.rotation = to_rot
    layer.dirty    = true
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.apply(layer, params) — immediate affine transform (no animation)
--    params: scaleX, scaleY, rotate, originX, originY, filter
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.apply(layer, params)
    if not layer or not params then return end

    if params.scaleX or params.scaleY then
        layer.scaleX = tonumber(params.scaleX) or layer.scaleX or 1
        layer.scaleY = tonumber(params.scaleY) or layer.scaleY or 1
    end
    if params.rotate or params.rotation then
        layer.rotation = tonumber(params.rotate or params.rotation) or layer.rotation or 0
    end
    if params.originX or params.originY then
        layer.originX = tonumber(params.originX) or 0
        layer.originY = tonumber(params.originY) or 0
    end
    layer.dirty = true
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform Stack — push/pop for nested transform compositing
--    Used when rendering: push transforms before drawing children,
--    pop after. Each layer can maintain its own local stack.
-- ═══════════════════════════════════════════════════════════════════════════

local transformStack = {}

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.push(m) — push a matrix onto the global stack
--    Returns the new top-of-stack (combined) matrix.
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.push(m)
    local top = transformStack[#transformStack]
    if top then
        local combined = Transform.multiply(top, m)
        transformStack[#transformStack + 1] = combined
        return combined
    else
        local copy = Transform.clone(m)
        transformStack[#transformStack + 1] = copy
        return copy
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.pop() → matrix — pop the top of the stack, return previous
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.pop()
    if #transformStack == 0 then return Transform.newMatrix() end
    table.remove(transformStack)
    return transformStack[#transformStack] or Transform.newMatrix()
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.top() → current top-of-stack matrix (or identity)
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.top()
    return transformStack[#transformStack] or Transform.newMatrix()
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.stack_depth() → integer
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.stack_depth()
    return #transformStack
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.clear_stack() — reset the global transform stack
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.clear_stack()
    transformStack = {}
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Transform.layer_matrix(layer) → matrix built from layer properties
--    Converts a layer node's scaleX, scaleY, rotation, origin into an
--    affine matrix suitable for affine_blt.
-- ═══════════════════════════════════════════════════════════════════════════

function Transform.layer_matrix(layer)
    if not layer then return Transform.newMatrix() end
    local m = Transform.newMatrix()
    local ox = layer.originX or 0
    local oy = layer.originY or 0
    local sx = layer.scaleX or 1
    local sy = layer.scaleY or 1
    local rot = layer.rotation or 0

    -- translate to origin
    Transform.translate(m, -ox, -oy)
    -- scale
    Transform.scale(m, sx, sy, 0, 0)
    -- rotate
    Transform.rotate(m, rot, 0, 0)
    -- translate back
    Transform.translate(m, ox, oy)

    return m
end

return Transform