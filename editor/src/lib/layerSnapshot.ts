// Visual preview — layer render-state snapshot (round 90).
//
// Reads the engine's live layer tree over the /api/eval channel
// (EngineClient.evalRaw), reusing the sandbox-whitelisted `layers` module
// (scripts/layers.lua) that the engine loads. The snippet walks the tree via
// layers.forEach(id, node) and reports each node's id/name/z/visible/texture
// handle as a JSON array, self-describing so it can never throw on the engine
// side (every access is guarded; missing nodes yield an empty array).
//
// The reply is one of:
//   "no-layers"  → the layers module is not loaded / no tree yet
//   "[...]"      → a JSON array of node snapshots (possibly empty)
//
// Pure string/array helpers — unit-testable without a DOM or engine.

/** One layer node in the render-state snapshot. */
export interface LayerSnapshot {
  /** Node id as reported by the layer tree (e.g. 1,2,3 or a string). */
  id: string
  /** Layer name, e.g. "bg", "fg", "msg" or a custom scene name. */
  name: string
  /** Z-order within the tree compositor. */
  z: number
  /** Whether the layer is currently rendered. */
  visible: boolean
  /** Raw bgfx texture handle index (0 / null when no texture is bound). */
  handle: number
  /** Composition opacity 0..1. */
  opacity: number
  /** Tree membership group fallback: 'bg' | 'fg' | 'msg'. */
  slot: string
}

/** Order of the 3 low-level compositor slots for row grouping. */
const SLOT_ORDER = ['bg', 'fg', 'msg']

/**
 * Build the eval snippet that reports the layer tree snapshot.
 * Returns a single Lua source string. The engine evaluates it via /api/eval;
 * the Lua side returns either "no-layers" or a JSON array text of node
 * snapshots {id, name, z, visible, handle, opacity}.
 * Fully defensive: every global/access guarded so it cannot throw.
 */
export function buildLayerSnapshotSnippet(): string {
  return [
    'local L = layers',
    'if not L or type(L.forEach) ~= "function" then return "no-layers" end',
    'local out = {}',
    'local function esc(s)',
    '  return (tostring(s):gsub("\\\\", "\\\\"):gsub([["]], [[\\"]]))',
    'end',
    'L.forEach(function(id, node)',
    '  local n = node or {}',
    '  local visible = n.visible == nil and true or (n.visible and true or false)',
    '  local z = tonumber(n.z) or 0',
    '  local handle = tonumber(n.texture) or 0',
    '  local opacity = tonumber(n.opacity) or 1',
    '  table.insert(out, "{\"id\":\"" .. esc(id) .. "\",\"name\":\"" .. esc(n.name or "")',
    '    .. "\",\"z\":\"" .. tostring(z) .. "\",\"visible\":\"" .. (visible and "1" or "0")',
    '    .. "\",\"handle\":\"" .. tostring(handle) .. "\",\"opacity\":\"" .. tostring(opacity) .. "\"}")',
    'end)',
    'return "[" .. table.concat(out, ",") .. "]"',
  ].join(' ')
}

/**
 * True when a layer name falls into a low-level compositor slot. Guards the
 * common "bg"/"fg"/"msg" names (and any name containing them); scene-graph
 * user layers (e.g. "_gallery") fall through to the generic slot.
 */
export function layerSlot(name: string): string {
  const n = (name || '').toLowerCase()
  for (const s of SLOT_ORDER) {
    if (n === s || n.indexOf(s) === 0) return s
  }
  return 'other'
}

/**
 * Parse the engine's layer-snapshot reply into a structured list.
 * Returns an empty array on "no-layers" / empty replies; never throws.
 * A malformed entry is dropped; a fully unparseable reply yields [].
 */
export function parseLayerSnapshot(result: string): LayerSnapshot[] {
  const trimmed = ((result ?? '') as string).trim()
  if (!trimmed || trimmed === 'no-layers') return []
  let raw: unknown
  try {
    raw = JSON.parse(trimmed)
  } catch {
    return []
  }
  if (!Array.isArray(raw)) return []
  const out: LayerSnapshot[] = []
  for (const item of raw) {
    if (!item || typeof item !== 'object') continue
    const rec = item as Record<string, unknown>
    const slot = layerSlot(typeof rec.name === 'string' ? rec.name : '')
    out.push({
      id: typeof rec.id === 'string' ? rec.id : String(rec.id ?? out.length),
      name: typeof rec.name === 'string' ? rec.name : `layer${out.length}`,
      z: typeof rec.z === 'number' ? rec.z : 0,
      visible: rec.visible !== false,
      handle: typeof rec.handle === 'number' ? rec.handle : 0,
      opacity: typeof rec.opacity === 'number' ? rec.opacity : 1,
      slot,
    })
  }
  // Stable ordering: compositor slots first (bg, fg, msg), then z asc.
  const rank = (l: LayerSnapshot) => {
    const s = SLOT_ORDER.indexOf(l.slot)
    return { s: s === -1 ? SLOT_ORDER.length : s, z: l.z }
  }
  out.sort((a, b) => {
    const ra = rank(a)
    const rb = rank(b)
    return ra.s !== rb.s ? ra.s - rb.s : ra.z - rb.z
  })
  return out
}
