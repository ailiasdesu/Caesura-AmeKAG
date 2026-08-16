// Scene Builder — pure helpers for visual scene construction.
//
// Generates KAG .ks statement lines from builder intents (background, sprite,
// dialogue, page break) and exposes the static demo asset manifest that the
// Scene Builder panel renders. Kept free of DOM/store so these can be unit
// tested in isolation and reused by both the panel and future drag-drop.

export type BuilderAssetKind = 'bg' | 'sprite'

/** One selectable visual asset in the Scene Builder palette. */
export interface SceneBuilderAsset {
  kind: BuilderAssetKind
  /** Display label (the file basename). */
  label: string
  /** Asset path used for the command's storage= parameter. */
  storage: string
}

/**
 * Canonical demo asset manifest. These are the visual assets the demo scenes
 * reference (bg/classroom.png, hana.png, fg/girl_uniform.png ...). The engine's
 * /api/assets route can be consulted for the full live set, but the panel keeps
 * a static palette so scene construction works even while the engine is
 * disconnected / not yet populated.
 */
export const DEMO_ASSETS: SceneBuilderAsset[] = [
  { kind: 'bg', label: 'classroom.png', storage: 'bg/classroom.png' },
  { kind: 'bg', label: 'room.png', storage: 'bg/room.png' },
  { kind: 'bg', label: 'stage.png', storage: 'bg/stage.png' },
  { kind: 'sprite', label: 'girl_uniform.png', storage: 'fg/girl_uniform.png' },
  { kind: 'sprite', label: 'hana.png', storage: 'hana.png' },
]

/** Compose a background statement, e.g. [bg storage="bg/classroom.png"]. */
export function buildBgLine(storage: string): string {
  return '[bg storage="' + storage + '"]'
}

/**
 * Compose a sprite (立绘) statement. Uses [csp] with an explicit on-screen
 * position; [image] is the alias form for a plain full-screen insert, kept
 * available for callers that prefer it.
 */
export function buildSpriteLine(storage: string, x: number, y: number, name = 'Hero'): string {
  // [csp] contract requires `name` (character id / asset stem, positional 1).
  return '[csp name="' + name + '" storage="' + storage + '" x=' + x + ' y=' + y + ']'
}

/** Plain [image] form of an on-screen sprite (no position). */
export function buildImageLine(storage: string): string {
  return '[image storage="' + storage + '"]'
}

/**
 * Compose a dialogue statement, e.g. [ch name="Hero" text="Hello"]. When the
 * name is empty (anonymous narration) the name= parameter is omitted.
 */
export function buildDialogueLine(name: string, text: string): string {
  const namePart = name.trim() ? 'name="' + name + '" ' : ''
  return '[ch ' + namePart + 'text="' + text + '"]'
}

/** The page-break statement that flushes accumulated text onto the screen. */
export const PAGE_BREAK_LINE = '[p]'

/**
 * True when there is nothing meaningful to build dialogue from (no text) - used
 * to disable the compose action and keep malformed empty tags out of the doc.
 */
export function isBlankDialogue(text: string): boolean {
  return text.trim() === ''
}
