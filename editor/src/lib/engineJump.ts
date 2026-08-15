// G4 increment 3 — live engine scene jump.
//
// Drives the RUNNING KAG scene to a *label from the editor outline, not just
// the editor buffer. The editor talks to the engine over the /api/eval HTTP
// route (EngineClient.evalRaw). When a .ks scene is running, the engine
// anchors the live KAG context at the sandbox-whitelisted global _CAESURA_CTX
// (scripts/kag_runner.lua: rawset(_G,'_CAESURA_CTX',ctx)). kag.jump(ctx, t)
// sets ctx._next_index so the running scheduler takes the jump on its next
// resume — a true live jump with no engine-side changes.
//
// The eval snippet is deliberately self-reporting so the caller can tell a
// no-op from a staged jump without relying on side effects:
//   "ok"      → scene is running and the label was found / jump staged
//   "missing" → a scene is running but has no such label (no-op)
//   "no-ctx"  → no KAG scene is currently running (kag.jump would no-op)
//
// Pure string builder — unit-testable without a DOM or engine.

/** Escape a label so it is safe inside a Lua double-quoted string literal. */
export function escapeLuaString(value: string): string {
  return value
    .replace(/\\/g, '\\\\')
    .replace(/"/g, '\\"')
    .replace(/\n/g, '\\n')
    .replace(/\r/g, '\\r')
}

/**
 * Build the Lua snippet that live-jumps the running KAG ctx to *label.
 * The returned snippet is idempotent and never throws:
 *  - no running ctx  → returns "no-ctx"
 *  - label not found → returns "missing"
 *  - jump staged     → returns "ok"
 */
export function buildLabelJumpSnippet(label: string): string {
  const safe = escapeLuaString(label)
  return (
    'local flow = require("flow");' +
    ' local kag = require("kag");' +
    ' local c = _G._CAESURA_CTX;' +
    ' if not c then return "no-ctx" end;' +
    ' if not flow.find_label(c.tokens, "' +
    safe +
    '") then return "missing" end;' +
    ' kag.jump(c, "*' +
    safe +
    '");' +
    ' return "ok"'
  )
}

/** Outcome discriminators a successful evalRaw may return. */
export type LabelJumpStatus = 'ok' | 'missing' | 'no-ctx' | string

/** Parse the engine's eval reply and classify the jump outcome. */
export function parseJumpResult(result: string): LabelJumpStatus {
  const trimmed = (result ?? '').trim()
  if (trimmed === 'ok') return 'ok'
  if (trimmed === 'missing') return 'missing'
  if (trimmed === 'no-ctx') return 'no-ctx'
  return trimmed
}
