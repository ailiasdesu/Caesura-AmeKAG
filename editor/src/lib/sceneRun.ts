// Scene run helpers for the DebugView (Scene Preview, task book §6.6).
//
// The engine drives a .ks scene through the kag_runner Lua module. To run the
// currently open document we eval a small self-reporting snippet over the
// /api/eval route (EngineClient.evalRaw):
//   local kr=require("kag_runner"); kr.stop();
//   return tostring(kr.start("<path>"))
// kr.start returns true on success; kr.stop ends any running scene first so a
// fresh run restarts cleanly (tostring lets the caller surface the verbatim
// engine result). Pure decision/snippet builders — unit-testable without a DOM
// or engine (mirrors editor/src/lib/engineJump.ts).

// Escape a scene path for a Lua double-quoted literal: reuse the shared
// helper from engineJump.ts (single source of truth for /api/eval bridges).
import { escapeLuaString } from './engineJump'

/** Escape a scene path so it is safe inside a Lua double-quoted string literal. */
export const escapeScenePath = escapeLuaString

/**
 * Resolve the runnable .ks scene path for an editor document path.
 * Returns null when the path is not a KAG scene (.lua, null, undefined, or any
 * other extension) so the caller can disable "Run Current Scene" and lean on
 * the manual scene box instead. Pure — unit-testable.
 */
export function scenePathForDoc(path: string | null | undefined): string | null {
  if (!path) return null
  if (!path.toLowerCase().endsWith('.ks')) return null
  return path
}

/**
 * Build the Lua snippet that stops any running scene and starts `path`.
 * The snippet is self-reporting (tostring) so the caller can surface the
 * engine result verbatim.
 */
export function buildRunSceneSnippet(path: string): string {
  const safe = escapeScenePath(path)
  return (
    'local kr=require("kag_runner");' +
    ' if kr.stop then kr.stop() end;' +
    ' return tostring(kr.start("' +
    safe +
    '"))'
  )
}
