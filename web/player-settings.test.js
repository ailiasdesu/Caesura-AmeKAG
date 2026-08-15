// player-settings.test.js — round 86: centralized player settings module.
// Covers the default table, per-field validation (illegal -> fallback),
// storage round-trip, subscribe/notify, volume wiring helpers, and the
// integration-path plumbing that main.mjs/bridge use (autoClick -> opts,
// volumes -> setBusVolume, language -> validateLanguage).
import { describe, it, expect, vi } from 'vitest'
import {
  DEFAULT_SETTINGS, DEFAULT_STORAGE_KEY, VOLUME_BUSSES,
  TEXT_SPEED_MIN, TEXT_SPEED_MAX, DEFAULT_TEXT_SPEED,
  defaultSettings, clampVolume, validateTextSpeed, validateLanguage,
  sanitizeSettings, makeMemoryStorage, createPlayerSettings,
} from './player-settings.js'

describe('defaults & schema', () => {
  it('exposes the frozen default table (en / no auto / cps 20 / skip off / all 1.0)', () => {
    expect(DEFAULT_SETTINGS.language).toBe('en')
    expect(DEFAULT_SETTINGS.autoClick).toBe(false)
    expect(DEFAULT_SETTINGS.textSpeed).toBe(DEFAULT_TEXT_SPEED)
    expect(DEFAULT_SETTINGS.skipMode).toBe(false)
    expect(DEFAULT_SETTINGS.volumes).toEqual({ bgm: 1, se: 1, voice: 1 })
    expect(VOLUME_BUSSES).toEqual(['bgm', 'se', 'voice'])
  })

  it('defaultSettings() returns a fresh mutable copy (not the frozen table)', () => {
    const a = defaultSettings()
    const b = defaultSettings()
    expect(a).not.toBe(DEFAULT_SETTINGS)
    expect(a.volumes).not.toBe(DEFAULT_SETTINGS.volumes)
    a.volumes.bgm = 0
    expect(b.volumes.bgm).toBe(1)
    expect(a.volumes.bgm).toBe(0)
    expect(b.volumes.bgm).toBe(1)
    a.volumes.bgm = 1
    expect(a).toEqual(defaultSettings())
  })

  it('bounds are sane: text speed 1..80, volumes clamped 0..1', () => {
    expect(TEXT_SPEED_MIN).toBe(1)
    expect(TEXT_SPEED_MAX).toBe(80)
    expect(clampVolume(-1)).toBe(0)
    expect(clampVolume(2)).toBe(1)
    expect(clampVolume(0.5)).toBe(0.5)
  })

  it('a fresh controller starts from defaults when storage is empty', () => {
    const s = makeMemoryStorage()
    const c = createPlayerSettings({ storage: s })
    expect(c.get('language')).toBe('en')
    expect(c.getAll()).toEqual(defaultSettings())
    expect(s.get(DEFAULT_STORAGE_KEY)).toBeNull()
  })
})

describe('validation — illegal values fall back to defaults', () => {
  it('rejects an unknown/garbage language, accepts BCP-47-ish codes', () => {
    expect(validateLanguage('zh')).toBe('zh')
    expect(validateLanguage('ja-JP')).toBe('ja-JP')
    expect(validateLanguage('fr-CA')).toBe('fr-CA')
    expect(validateLanguage('')).toBe(DEFAULT_SETTINGS.language)
    expect(validateLanguage(42)).toBe(DEFAULT_SETTINGS.language)
    expect(validateLanguage('en!')).toBe(DEFAULT_SETTINGS.language)
  })

  it('clamps out-of-range text speed and clamps negative/oversized vol', () => {
    expect(validateTextSpeed(0)).toBe(TEXT_SPEED_MIN)
    expect(validateTextSpeed(9999)).toBe(TEXT_SPEED_MAX)
    expect(validateTextSpeed(30.6)).toBe(31)
    expect(validateTextSpeed('fast')).toBe(DEFAULT_TEXT_SPEED)
    expect(sanitizeSettings({ textSpeed: 5, volumes: { bgm: 3, se: -2, voice: 'x' } }))
      .toMatchObject({ textSpeed: 5, volumes: { bgm: 1, se: 0, voice: 1 } })
  })

  it('sanitizeSettings fills every missing field with a default', () => {
    const s = sanitizeSettings({})
    expect(s).toEqual(defaultSettings())
    expect(sanitizeSettings('junk')).toEqual(defaultSettings())
    expect(sanitizeSettings(null)).toEqual(defaultSettings())
  })

  it('setting an invalid value via controller keeps current valid state', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    c.set('language', 123)
    expect(c.get('language')).toBe('en')
    c.set('language', 'zh')
    expect(c.get('language')).toBe('zh')
  })
})

describe('persistence — save/load round trip', () => {
  it('load() re-hydrates a previously saved settings object', () => {
    const s = makeMemoryStorage()
    const c = createPlayerSettings({ storage: s })
    c.set('language', 'ja')
    c.setVolume('bgm', 0.4)
    c.set('textSpeed', 45)
    const expected = c.getAll()

    const c2 = createPlayerSettings({ storage: s })
    const loaded = c2.load()
    expect(loaded).toEqual(expected)
    expect(c2.get('language')).toBe('ja')
    expect(c2.get('volumes').bgm).toBe(0.4)
    expect(c2.get('textSpeed')).toBe(45)
  })

  it('survives JSON corrupt / non-object payloads (falls back to defaults)', () => {
    const s = makeMemoryStorage()
    s.set(DEFAULT_STORAGE_KEY, '{not json')
    const c = createPlayerSettings({ storage: s })
    expect(c.load()).toEqual(defaultSettings())
  })

  it('unknown keys are stripped on load (sanitize drops junk fields)', () => {
    const s = makeMemoryStorage()
    s.set(DEFAULT_STORAGE_KEY, JSON.stringify({ language: 'en', bogus: true, nested: { x: 1 } }))
    const c = createPlayerSettings({ storage: s })
    const loaded = c.load()
    expect(loaded).not.toHaveProperty('bogus')
    expect(loaded).not.toHaveProperty('nested')
    expect(loaded).toEqual(defaultSettings())
  })

  it('set() persists immediately; a fresh controller reads it back', () => {
    const s = makeMemoryStorage()
    const c = createPlayerSettings({ storage: s })
    c.set('skipMode', true)
    expect(JSON.parse(s.get(DEFAULT_STORAGE_KEY)).skipMode).toBe(true)
  })
})

describe('subscribe / unsubscribe notifications', () => {
  it('fires listeners on set with field/value/prev and latest settings', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    const fn = vi.fn()
    c.subscribe(fn)
    c.set('textSpeed', 60)
    expect(fn).toHaveBeenCalledTimes(1)
    const arg = fn.mock.calls[0][0]
    expect(arg.field).toBe('textSpeed')
    expect(arg.value).toBe(60)
    expect(arg.prev).toBe(DEFAULT_TEXT_SPEED)
    expect(arg.settings.textSpeed).toBe(60)
  })

  it('setVolume notifies with the bus as field and the new volume', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    const fn = vi.fn()
    c.subscribe(fn)
    c.setVolume('voice', 0.7)
    const arg = fn.mock.calls[0][0]
    expect(arg.field).toBe('volumes')
    expect(arg.settings.volumes.voice).toBe(0.7)
  })

  it('unsubscribe stops further notifications', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    const fn = vi.fn()
    c.subscribe(fn)
    c.unsubscribe(fn)
    c.set('language', 'zh')
    expect(fn).not.toHaveBeenCalled()
  })

  it('subscribe returns an unsubscribe handle that works too', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    const fn = vi.fn()
    const off = c.subscribe(fn)
    off()
    c.setVolume('bgm', 0.5)
    expect(fn).not.toHaveBeenCalled()
  })

  it('an invalid field name is rejected without crashing or notifying', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    const fn = vi.fn()
    c.subscribe(fn)
    expect(c.set('nope', true)).toBe(false)
    expect(fn).not.toHaveBeenCalled()
  })

  it('a throwing listener does not break subsequent listeners', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    const bad = vi.fn(() => { throw new Error('boom') })
    const good = vi.fn()
    c.subscribe(bad)
    c.subscribe(good)
    expect(() => c.set('skipMode', true)).not.toThrow()
    expect(good).toHaveBeenCalledTimes(1)
  })
})

describe('integration-path plumbing (what main/bridge read)', () => {
  it('autoClick setting is a boolean and maps 1:1 to run opts.autoClick', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    expect(typeof c.get('autoClick')).toBe('boolean')
    c.set('autoClick', true)
    expect(c.get('autoClick')).toBe(true)
    expect(sanitizeSettings({ autoClick: 1 }).autoClick).toBe(false)
  })

  it('volumes wiring: each bus value routes to audio setBusVolume (0..1)', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    const setBusVolume = vi.fn()
    c.setVolume('bgm', 0.8)
    c.setVolume('se', 0.2)
    c.setVolume('voice', 0.5)
    const vols = c.get('volumes')
    for (const b of VOLUME_BUSSES) {
      setBusVolume(b, vols[b])
      expect(setBusVolume).toHaveBeenCalledWith(b, expect.any(Number))
    }
    expect(vols.bgm).toBe(0.8)
    expect(vols.se).toBe(0.2)
    expect(vols.voice).toBe(0.5)
  })

  it('unknown volume bus is rejected', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    expect(c.setVolume('master', 0.5)).toBe(false)
  })

  it('language validation output feeds i18n.set_language (BCP-47 safe)', () => {
    expect(validateLanguage('zh')).toBe('zh')
    expect(validateLanguage('ja-JP')).toBe('ja-JP')
    expect(validateLanguage(';; drop table')).toBe(DEFAULT_SETTINGS.language)
  })

  it('makeMemoryStorage implements the full KV contract', () => {
    const s = makeMemoryStorage()
    expect(s.get('k')).toBeNull()
    expect(s.set('k', 'v')).toBe(true)
    expect(s.get('k')).toBe('v')
    s.del('k')
    expect(s.get('k')).toBeNull()
  })
})

describe('meta', () => {
  it('keeps a stable storage key and ships deep coverage', () => {
    expect(DEFAULT_STORAGE_KEY).toBe('caesura.player-settings')
    expect(String(DEFAULT_STORAGE_KEY)).toContain('player-settings')
    expect(VOLUME_BUSSES.length).toBeGreaterThanOrEqual(3)
  })
})
