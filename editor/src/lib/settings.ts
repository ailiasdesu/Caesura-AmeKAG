// Caesura Editor — IDE preference settings (theme / font / engine connection).
// Persistent across page reloads via localStorage, mirroring the chatHistory
// persistence pattern used by the AiPanel. Every field is validated on load so
// a corrupt or stale stored value falls back to the default instead of
// crashing the editor or leaving Monaco in a broken state.

export type EditorTheme = 'dark' | 'light'

/** Editor preference settings persisted to localStorage (layer "settings"). */
export interface EditorSettings {
  /** Color theme. The workbench applies it as `data-theme` on the app root. */
  theme: EditorTheme
  /** Monaco editor font size in px (clamped 8..32 on apply/load). */
  fontSize: number
  /** Monaco editor "line numbers" gutter toggle. */
  showLineNumbers: boolean
  /** Engine HTTP editor server port (informational; the dev proxy fixes 9876). */
  enginePort: number
  /** Engine bearer token (CAESURA_EDITOR_TOKEN) — informational for now. */
  engineToken: string
}

export const DEFAULT_SETTINGS: EditorSettings = {
  theme: 'dark',
  fontSize: 13,
  showLineNumbers: true,
  enginePort: 9876,
  engineToken: '',
}

/** localStorage key for the editor settings blob. */
export const SETTINGS_KEY = 'caesura.editor.settings'

const MIN_FONT = 8
const MAX_FONT = 32
const MIN_PORT = 1
const MAX_PORT = 65535

function isTheme(x: unknown): x is EditorTheme {
  return x === 'dark' || x === 'light'
}

/**
 * Validate an int against [min, max] and round it; an out-of-range or
 * non-numeric value falls back to `dflt` rather than being clamped, so
 * corrupt stored preferences never silently drift to a boundary.
 */
function clampInt(x: unknown, min: number, max: number, dflt: number): number {
  if (typeof x !== 'number' || !Number.isFinite(x)) return dflt
  const n = Math.round(x)
  return n >= min && n <= max ? n : dflt
}

/**
 * Validate/sanitize an arbitrary parsed value into a full EditorSettings.
 * Invalid fields individually fall back to their default — a valid partial
 * value (e.g. theme only) still produces a complete, coherent settings object.
 */
export function sanitizeSettings(x: unknown): EditorSettings {
  const o = (typeof x === 'object' && x !== null) ? (x as Record<string, unknown>) : {}
  return {
    theme: isTheme(o.theme) ? o.theme : DEFAULT_SETTINGS.theme,
    fontSize: clampInt(
      o.fontSize,
      MIN_FONT,
      MAX_FONT,
      DEFAULT_SETTINGS.fontSize,
    ),
    showLineNumbers:
      typeof o.showLineNumbers === 'boolean'
        ? o.showLineNumbers
        : DEFAULT_SETTINGS.showLineNumbers,
    enginePort: clampInt(
      o.enginePort,
      MIN_PORT,
      MAX_PORT,
      DEFAULT_SETTINGS.enginePort,
    ),
    engineToken:
      typeof o.engineToken === 'string'
        ? o.engineToken
        : DEFAULT_SETTINGS.engineToken,
  }
}

/** Load persisted settings; missing/corrupt storage falls back to defaults. */
export function loadSettings(key: string = SETTINGS_KEY): EditorSettings {
  try {
    const raw = localStorage.getItem(key)
    if (!raw) return { ...DEFAULT_SETTINGS }
    return sanitizeSettings(JSON.parse(raw) as unknown)
  } catch {
    return { ...DEFAULT_SETTINGS }
  }
}

/** Persist settings. Non-fatal on failure (storage full / private mode). */
export function saveSettings(settings: EditorSettings, key: string = SETTINGS_KEY): void {
  try {
    localStorage.setItem(key, JSON.stringify(sanitizeSettings(settings)))
  } catch {
    /* storage unavailable — non-fatal */
  }
}

/** Remove persisted settings (used by "reset to defaults"). */
export function clearSettings(key: string = SETTINGS_KEY): void {
  try {
    localStorage.removeItem(key)
  } catch {
    /* non-fatal */
  }
}