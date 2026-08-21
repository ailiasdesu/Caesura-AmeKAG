// Caesura Editor — recent-projects history (Project Manager, Sprint 2).
// Mirrors the localStorage persistence pattern of lib/settings.ts: load is
// defensive (corrupt/missing storage falls back to []); save is non-fatal on
// failure (storage full / private mode). Recent entries are {path, name,
// openedAt} so the Project Manager can render the list most-recent-first and
// the store can de-duplicate by project path.

export interface RecentProject {
  /** Directory path relative to the engine cwd, e.g. "projects/demo". */
  path: string
  name: string
  /** Epoch-ms when the project was last opened. */
  openedAt: number
}

/** localStorage key for the recent-projects history blob. */
export const RECENT_KEY = 'caesura.editor.recentProjects'

/** Hard cap — keeps the history bounded and the list scrollable. */
export const RECENT_LIMIT = 20

function isRecentProject(x: unknown): x is RecentProject {
  if (typeof x !== 'object' || x === null) return false
  const o = x as Record<string, unknown>
  return (
    typeof o.path === 'string' &&
    typeof o.name === 'string' &&
    typeof o.openedAt === 'number' &&
    Number.isFinite(o.openedAt)
  )
}

/**
 * Validate/sanitize an arbitrary parsed value into a RecentProject[].
 * Invalid entries are dropped individually so a partially corrupt blob still
 * yields a usable, coherent history.
 */
export function sanitizeRecentProjects(x: unknown): RecentProject[] {
  if (!Array.isArray(x)) return []
  return x.filter(isRecentProject).slice(0, RECENT_LIMIT)
}

/** Load persisted recent projects; missing/corrupt storage → []. */
export function loadRecentProjects(key: string = RECENT_KEY): RecentProject[] {
  try {
    const raw = localStorage.getItem(key)
    if (!raw) return []
    return sanitizeRecentProjects(JSON.parse(raw) as unknown)
  } catch {
    return []
  }
}

/** Persist the recent-projects history. Non-fatal on failure. */
export function saveRecentProjects(
  recent: RecentProject[],
  key: string = RECENT_KEY,
): void {
  try {
    localStorage.setItem(key, JSON.stringify(sanitizeRecentProjects(recent)))
  } catch {
    /* storage unavailable — non-fatal */
  }
}

/**
 * Pure: add a project to the front of the history, de-duplicating by path
 * (an existing entry is moved to the front with a fresh openedAt) and capping
 * at RECENT_LIMIT. Returns a new immutable array.
 */
export function pushRecentProject(
  recent: RecentProject[],
  path: string,
  name: string,
  openedAt: number = Date.now(),
): RecentProject[] {
  const entry: RecentProject = { path, name, openedAt }
  const rest = recent.filter((r) => r.path !== path)
  return [entry, ...rest].slice(0, RECENT_LIMIT)
}
