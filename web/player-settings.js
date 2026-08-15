// player-settings.js — centralized, persistent player settings for the
// Caesura web player (round 86). Owns language / auto-click / text speed /
// skip-mode / per-bus volumes, with a default table, per-field validation
// (illegal values fall back to defaults), an injectable storage backend
// (browser localStorage by default, in-memory Map in tests/headless), and
// subscribe/notify so the player UI and engine wiring stay in sync.
//
// The module is a pure factory (no shared singleton state) so tests can
// create isolated instances per case; the player calls createPlayerSettings
// once and keeps the returned controller.

// ---- setting schema ---------------------------------------------------
export const VOLUME_BUSSES = ['bgm', 'se', 'voice']

/** Bounds used by field validators. Clamped — illegal values fall back to
 *  the default (not silently accepted). */
export const TEXT_SPEED_MIN = 1
export const TEXT_SPEED_MAX = 80
export const DEFAULT_TEXT_SPEED = 20

/** The settings a fresh install gets when storage is empty. */
export const DEFAULT_SETTINGS = Object.freeze({
  language: 'en',
  autoClick: false,
  textSpeed: DEFAULT_TEXT_SPEED, // displayed characters per second
  skipMode: false,
  volumes: Object.freeze({ bgm: 1, se: 1, voice: 1 }),
})

export const DEFAULT_STORAGE_KEY = 'caesura.player-settings'

/** @returns a fresh copy of the default settings (drops the shared
 *  frozen object so callers may mutate the result safely). */
export function defaultSettings() {
  return {
    language: DEFAULT_SETTINGS.language,
    autoClick: DEFAULT_SETTINGS.autoClick,
    textSpeed: DEFAULT_SETTINGS.textSpeed,
    skipMode: DEFAULT_SETTINGS.skipMode,
    volumes: { ...DEFAULT_SETTINGS.volumes },
  }
}

// ---- individual field validators (exported for unit coverage) ----------
/** Clamp a number into 0..1; non-finites fall back to 1. */
export function clampVolume(v) {
  const n = Number(v)
  if (!Number.isFinite(n)) return 1
  return Math.min(1, Math.max(0, n))
}

/** Coerce a non-finite, or out-of-range, text speed to a bounded int. */
export function validateTextSpeed(v) {
  const n = Number(v)
  if (!Number.isFinite(n)) return DEFAULT_TEXT_SPEED
  return Math.min(TEXT_SPEED_MAX, Math.max(TEXT_SPEED_MIN, Math.round(n)))
}

/** Accept a BCP-47-ish language code (en, zh, ja-JP …); anything else
 *  falls back to the default language. */
export function validateLanguage(v) {
  if (typeof v === 'string' && /^[a-z]{2,3}(-[A-Za-z0-9]{2,})?$/.test(v)) return v
  return DEFAULT_SETTINGS.language
}

/** Non-issues: autoClick / skipMode are plain booleans. Coerce only when
 *  the value is exactly a boolean; anything else keeps the fallback. */
function boolOr(fallback) { return (v) => (typeof v === 'boolean' ? v : fallback) }

/** Sanitize an arbitrary (possibly corrupt/partial) settings object into a
 *  complete, valid settings object. Every missing or malformed field is
 *  replaced with its default. */
export function sanitizeSettings(raw) {
  const src = raw && typeof raw === 'object' ? raw : {}
  const rawVolumes = src.volumes && typeof src.volumes === 'object' ? src.volumes : {}
  const volumes = {}
  for (const b of VOLUME_BUSSES) {
    volumes[b] = typeof rawVolumes[b] === 'number' ? clampVolume(rawVolumes[b]) : DEFAULT_SETTINGS.volumes[b]
  }
  const language = typeof src.language === 'string' ? validateLanguage(src.language) : DEFAULT_SETTINGS.language
  const autoClick = boolOr(DEFAULT_SETTINGS.autoClick)(src.autoClick)
  const textSpeed = typeof src.textSpeed === 'number' ? validateTextSpeed(src.textSpeed) : DEFAULT_SETTINGS.textSpeed
  const skipMode = boolOr(DEFAULT_SETTINGS.skipMode)(src.skipMode)
  return { language, autoClick, textSpeed, skipMode, volumes }
}

/** A storage backend with the minimal KV contract the settings controller
 *  needs. Returns a process-lifetime in-memory Map store (used by tests and
 *  headless environments that lack localStorage). */
export function makeMemoryStorage() {
  const mem = new Map()
  return {
    get: (k) => (mem.has(k) ? mem.get(k) : null),
    set: (k, v) => { mem.set(k, String(v)); return true },
    del: (k) => { mem.delete(k) },
  }
}

function defaultStorage() {
  if (typeof localStorage !== 'undefined' && localStorage) {
    return {
      get: (k) => { try { return localStorage.getItem(k) } catch { return null } },
      set: (k, v) => { try { localStorage.setItem(k, String(v)); return true } catch { return false } },
      del: (k) => { try { localStorage.removeItem(k) } catch { /* noop */ } },
    }
  }
  return makeMemoryStorage()
}

// ---- controller ----------------------------------------------------------
/**
 * createPlayerSettings({ storage?, storageKey? })
 * Returns a settings controller bound to one storage backend + key:
 *   .get(key)              — current validated value for one field
 *   .getAll()              — a copy of the full settings object
 *   .set(key, value)       — update one field (validated), notify, persist
 *   .setVolume(bus, v)     — update one volume bus, notify, persist
 *   .load()                — (re)read + sanitize from storage, notify subscribers
 *   .save()                — persist the current settings to storage
 *   .subscribe(fn)         — register a listener; returns an unsubscribe fn
 *   .unsubscribe(fn)       — remove a previously registered listener
 *   .reset()               — restore defaults (no storage write by default)
 */
export function createPlayerSettings(opts = {}) {
  const storage = opts.storage ?? defaultStorage()
  const key = opts.storageKey ?? DEFAULT_STORAGE_KEY
  // current state (always valid — sanitized on every write/load)
  let state = defaultSettings()
  const listeners = new Set()

  const notify = (field, value, prev) => {
    for (const fn of [...listeners]) {
      try { fn({ field, value, prev, settings: { ...state } }) } catch { /* listener errors are non-fatal */ }
    }
  }
  // persist current state as JSON (never throws; storage may reject)
  const persist = () => {
    const raw = JSON.stringify(state)
    storage.set(key, raw)
  }

  const controller = {
    get(key) {
      return key === 'volumes' ? { ...state.volumes } : state[key]
    },
    getAll() {
      return { language: state.language, autoClick: state.autoClick, textSpeed: state.textSpeed, skipMode: state.skipMode, volumes: { ...state.volumes } }
    },
    set(field, value) {
      if (!(field in DEFAULT_SETTINGS)) return false
      const next = sanitizeSettings({ ...state, [field]: value })
      const prev = field === 'volumes' ? { ...state.volumes } : state[field]
      state = next
      notify(field, next[field], prev)
      persist()
      return true
    },
    setVolume(bus, v) {
      if (!VOLUME_BUSSES.includes(bus)) return false
      const clamped = clampVolume(v)
      const prev = state.volumes[bus]
      state = { ...state, volumes: { ...state.volumes, [bus]: clamped } }
      notify('volumes', { ...state.volumes }, prev)
      persist()
      return true
    },
    load() {
      const raw = storage.get(key)
      if (!raw) { state = defaultSettings(); return this.getAll() }
      let parsed = null
      try { parsed = JSON.parse(raw) } catch { /* corrupt payload -> defaults */ }
      state = sanitizeSettings(parsed)
      return this.getAll()
    },
    save() { persist(); return true },
    subscribe(fn) {
      listeners.add(fn)
      return () => listeners.delete(fn)
    },
    unsubscribe(fn) { listeners.delete(fn) },
    reset() {
      const prev = state
      state = defaultSettings()
      notify('*', this.getAll(), prev)
      return this.getAll()
    },
  }
  return controller
}
