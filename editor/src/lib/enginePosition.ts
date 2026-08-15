// G4 final increment — live engine position probe + outline mapping.
//
// Cross-references the RUNNING KAG scene position (scene + token_index) back
// into the editor's scene outline. The editor reads the position over the
// /api/eval route (EngineClient.evalRaw), reusing the sandbox-whitelisted
// _CAESURA_CTX global that kag_runner.lua anchors at the running ctx
// (rawset(_G,'_CAESURA_CTX',ctx); scripts/kag_runner.lua). The ctx carries
// ctx.current_scene (the scene path, e.g. "assets/script/main.ks") and
// ctx.token_index (the scheduler's 1-based token index).
//
// The engine reply is self-describing so a probe can never throw:
//   "{scene, token}" → a scene is running at that token
//   "no-ctx"         → no KAG scene is running (no _CAESURA_CTX)
//   "no-scene"       → ctx exists but carries no scene/token yet
//
// Mapping token -> outline row: the runtime token stream and the parsed
// outline derive from the same source in document order, so in the common
// (no-macro) case the Nth token corresponds 1:1 to the Nth outline item
// (label / command / text). Each OutlineItem carries its source line, so a
// token index resolves to a source line that the panel can highlight.
//
// Pure string/array helpers — unit-testable without a DOM or engine.

import type { OutlineSection } from '../ide/sceneOutlineTypes'

/**
 * Build the eval snippet that reports the live position.
 * Returns a single outer string (the snippet); the Lua side returns either
 * "no-ctx" / "no-scene" or a JSON object text "{scene, token}".
 * The scene path is JSON-escaped (backslash + quote) before embedding, so a
 * path containing unusual characters still yields well-formed JSON.
 */
export function buildPositionProbeSnippet(): string {
  return (
    'local c = _G._CAESURA_CTX;' +
    ' if not c then return "no-ctx" end;' +
    ' local scene = tostring(c.current_scene or c.currentScene or "");' +
    ' local token = tonumber(c.token_index or c.tokenIndex);' +
    ' if scene == "" or not token then return "no-scene" end;' +
    ' scene = scene:gsub("\\\\", "\\\\"):gsub([["]], [[\\"]]);' +
    " return '{\"scene\":\"' .. scene .. '\",\"token\":' .. tostring(token) .. '}'"
  )
}

/** Parsed live position: the scene path plus the 1-based token index. */
export interface EnginePosition {
  scene: string
  token: number
}

/**
 * Parse the engine's probe reply. Returns the resolved position, or null when
 * the engine reports no running scene ("no-ctx" / "no-scene") or the reply is
 * unparseable. Never throws.
 */
export function parsePositionProbe(result: string): EnginePosition | null {
  const trimmed = (result ?? '').trim()
  if (!trimmed || trimmed === 'no-ctx' || trimmed === 'no-scene') return null
  try {
    const parsed = JSON.parse(trimmed) as { scene?: unknown; token?: unknown }
    if (typeof parsed.scene !== 'string' || typeof parsed.token !== 'number') {
      return null
    }
    return { scene: parsed.scene, token: parsed.token }
  } catch {
    return null
  }
}

/**
 * True when the engine scene path corresponds to an open document path.
 * Exact normalized match wins, then a basename match (strips directory and a
 * trailing .ks), so "assets/script/main.ks" matches an open doc at either the
 * same full path or the bare basename. Returns false on empty inputs.
 */
export function sceneMatchesDoc(scene: string, docPath: string): boolean {
  if (!scene || !docPath) return false
  const norm = (p: string) => p.replace(/\\/g, '/').replace(/^\/?/, '')
  const s = norm(scene)
  const d = norm(docPath)
  if (s === d) return true
  const baseOf = (p: string) => {
    const base = p.split('/').pop() ?? ''
    const noExt = base.replace(/\.ks$/i, '')
    return noExt !== base ? noExt.toLowerCase() : base.toLowerCase()
  }
  const sb = baseOf(s)
  const db = baseOf(d)
  return sb !== '' && sb === db
}

/**
 * Resolve a token index to the source line of the outline item it points at.
 * The token stream and the outline items are both document-ordered, so the
 * Nth token maps to the Nth outline item (label headings plus command/text
 * items in source order). Label items resolve to the label's own line (the
 * section heading row); command/text items resolve to their line. Returns
 * null when the token is out of range or the outline is empty.
 */
export function tokenToOutlineLine(
  sections: OutlineSection[],
  tokenIndex: number,
): number | null {
  if (!sections || sections.length === 0) return null
  // Flatten heading + items in document order, matching the token stream.
  const rows: number[] = []
  for (const s of sections) {
    rows.push(s.line) // section heading line (label, or first item for prologue)
    for (const item of s.items) rows.push(item.line)
  }
  const idx = tokenIndex - 1 // engine token index is 1-based
  if (idx < 0 || idx >= rows.length) return null
  return rows[idx]
}