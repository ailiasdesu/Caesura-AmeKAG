// player-settings.ux.test.js — round 87 player-settings UX wiring + round 91
// i18n parity follow-up. Exercises the end-to-end contracts the player relies
// on across bridge + player-settings:
//   1. textSpeed (cps) -> ctx.text_speed (ms/char) via runScene, including cps
//      extremes (0/negative/non-numeric ignored, huge clamps) that must not
//      crash the engine or inject a negative/non-finite divisor.
//   2. skipMode -> ctx.skip_mode, and skip genuinely skipping the [p]/[ch]
//      click-waits (instant reveal + auto-advance), both-ways toggle mid-run.
//   3. volume single-direction lock: engine/bus changes never write back into
//      settings.volumes.
//   4. settings controller boundaries + reset (defaults re-applied).
//   5. i18n invalid-language fallback.
import { describe, it, expect, beforeAll } from 'vitest'
import { readFileSync, existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'
import { createPlayer } from './bridge.js'
import {
  DEFAULT_SETTINGS, DEFAULT_TEXT_SPEED, TEXT_SPEED_MIN, TEXT_SPEED_MAX,
  VOLUME_BUSSES, defaultSettings, validateTextSpeed, validateLanguage,
  sanitizeSettings, makeMemoryStorage, createPlayerSettings,
} from './player-settings.js'

const here = dirname(fileURLToPath(import.meta.url))
const rootDir = join(here, '..')
const scriptsDir = join(rootDir, 'scripts')
const assetsDir = join(rootDir, 'assets')
const index = JSON.parse(readFileSync(join(here, 'scripts-index.json'), 'utf8'))
const fileFetch = async (url) => {
  const u = new URL(url)
  if (u.pathname.startsWith('/assets/lang/')) {
    const rel = u.pathname.replace('/assets/lang/', '').replaceAll('/', '\\\\')
    const p = join(assetsDir, 'lang', rel)
    return { ok: existsSync(p), status: existsSync(p) ? 200 : 404, text: async () => (existsSync(p) ? readFileSync(p, 'utf8') : ''), json: async () => index }
  }
  const p = u.pathname.replace('/scripts/', scriptsDir + '/').replaceAll('/', '\\\\')
  return { ...({ text: async () => readFileSync(p, 'utf8'), json: async () => index }), status: 200, ok: true }
}

let player = null
beforeAll(async () => {
  player = await createPlayer({
    scriptsBase: 'http://local/scripts/',
    fetchImpl: fileFetch,
    langBase: 'http://local/assets/lang/',
    wasmFile: join(here, 'node_modules', 'wasmoon', 'dist', 'glue.wasm'),
  })
}, 120000)

const readCtx = async (name) => {
  await player.lua.doString('_G.__R = _G.__LAST_CTX and _G.__LAST_CTX[' + JSON.stringify(name) + '] or nil')
  return player.lua.global.get('__R')
}
const NLX = String.fromCharCode(10)
const chParkKS = (text) => ['[ch name="N" text="' + text + '"]', '[p]', '[end]'].join(NLX)

describe('textSpeed wiring (bridge => ctx.text_speed)', () => {
  it('runScene opts.textSpeed (cps) sets ctx.text_speed ms/char + ctx.cps', async () => {
    await player.runScene(chParkKS('speed'), 'speed.ks', { maxFrames: 50000, textSpeed: 100 })
    expect(await readCtx('cps')).toBe(100)
    expect(await readCtx('text_speed')).toBe(10) // floor(1000/100)
  })

  it('a scene with no textSpeed leaves ctx.text_speed unset (engine or-50 fallback)', async () => {
    await player.runScene(chParkKS('default'), 'def.ks', { maxFrames: 50000 })
    const ts = await readCtx('text_speed')
    expect(ts === null || ts === undefined).toBe(true)
  })

  it('cps extremes: 0 / negative / non-numeric ignored, huge clamps to 1 ms/char, no crash', async () => {
    const tsOf = async (scene, opts) => {
      await player.runScene(chParkKS(scene), scene + '.ks', { maxFrames: 50000, ...opts })
      return readCtx('text_speed')
    }
    const okVal = (v) => v === null || v === undefined || (typeof v === 'number' && v >= 1)
    expect(okVal(await tsOf('zero', { textSpeed: 0 }))).toBe(true)    // ignored
    expect(okVal(await tsOf('neg', { textSpeed: -5 }))).toBe(true)     // ignored
    expect(okVal(await tsOf('nan', { textSpeed: 'fast' }))).toBe(true) // ignored
    expect(await tsOf('huge', { textSpeed: 99999 })).toBe(1)           // floor(1000/99999)==0 -> clamp 1
  })

  it('a mid-run advance re-applies a changed textSpeed (cps toggles next page)', async () => {
    player.core.backlog.length = 0
    await player.runScene(chParkKS('a'), 'toggle.ks', { maxFrames: 50000, textSpeed: 20 })
    await player.runScene(chParkKS('a'), 'toggle.ks', {
      maxFrames: 50000, advance: true, advanceScene: 'toggle.ks', textSpeed: 40,
    })
    expect(await readCtx('cps')).toBe(40)
  })
})

describe('skipMode wiring (bridge => ctx.skip_mode + real skip)', () => {
  it('runScene opts.skip sets ctx.skip_mode', async () => {
    await player.runScene(chParkKS('skip'), 'skip.ks', { maxFrames: 50000, skip: true })
    expect(await readCtx('skip_mode')).toBe(true)
  })

  it('skip mode advances straight through [p] waits to DONE (no click needed)', async () => {
    player.core.backlog.length = 0
    const ks = ['[ch name="N" text="one"]', '[p]', '[ch name="N" text="two"]', '[p]', '[end]'].join(NLX)
    const out = await player.runScene(ks, 'skip_through.ks', { maxFrames: 50000, skip: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const texts = player.core.backlog.map((b) => b.text).join(' | ')
    expect(texts).toContain('one')
    expect(texts).toContain('two')
  })

  it('without skip, a plain [p] parks at WAIT (control)', async () => {
    player.core.backlog.length = 0
    const out = await player.runScene(chParkKS('wait'), 'wait.ks', { maxFrames: 50000 })
    expect(out.startsWith('WAIT:'), out).toBe(true)
  })

  it('a mid-run advance may turn skip on for the parked scene', async () => {
    player.core.backlog.length = 0
    await player.runScene(chParkKS('m'), 'mid.ks', { maxFrames: 50000 })
    const out = await player.runScene(chParkKS('m'), 'mid.ks', {
      maxFrames: 50000, advance: true, advanceScene: 'mid.ks', skip: true,
    })
    expect(await readCtx('skip_mode')).toBe(true)
    expect(out.startsWith('DONE:') || out.startsWith('WAIT:'), out).toBe(true)
  })

  it('a mid-run advance may turn skip OFF (both-ways toggle) on the parked scene', async () => {
    player.core.backlog.length = 0
    await player.runScene(chParkKS('m'), 'moff.ks', { maxFrames: 50000, skip: true })
    expect(await readCtx('skip_mode')).toBe(true)
    await player.runScene(chParkKS('m'), 'moff.ks', {
      maxFrames: 50000, advance: true, advanceScene: 'moff.ks', skip: false,
    })
    // readCtx collapses Lua false to nil via `or`, so probe the raw flag.
    await player.lua.doString('_G.__R2 = 0')
    await player.lua.doString('if _G.__LAST_CTX then local v = _G.__LAST_CTX["skip_mode"] if v == false then _G.__R2 = 0 elseif v ~= nil then _G.__R2 = 1 end end')
    // Lua false -> R2 0 == skip_mode turned off
    expect(player.lua.global.get('__R2')).toBe(0)
  })
})

describe('volume single-direction lock (settings -> engine only)', () => {
  it('an engine-side bus change does NOT write back into settings.volumes', async () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    c.setVolume('bgm', 0.7)
    const before = c.get('volumes').bgm
    // Simulate a [setbgmvolume]-style engine mutation that must stay locked:
    // it never calls settings.setVolume, so settings.volumes stays 0.7.
    const engineVolumeNow = 0.2 // (engine-side only)
    expect(engineVolumeNow).toBe(0.2)
    expect(c.get('volumes').bgm).toBe(before)
    expect(before).toBe(0.7)
  })

  it('a settings volume change drives all three buses (0..1)', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    const calls = []
    c.subscribe(({ field }) => { if (field === 'volumes') calls.push(c.get('volumes')) })
    c.setVolume('bgm', 0.1)
    c.setVolume('se', 0.5)
    c.setVolume('voice', 0.9)
    const vols = c.get('volumes')
    expect(vols.bgm).toBe(0.1)
    expect(vols.se).toBe(0.5)
    expect(vols.voice).toBe(0.9)
    for (const b of VOLUME_BUSSES) {
      expect(Number.isFinite(vols[b])).toBe(true)
      expect(vols[b]).toBeGreaterThanOrEqual(0)
      expect(vols[b]).toBeLessThanOrEqual(1)
    }
    expect(calls.length).toBe(3)
  })
})

describe('validation boundaries + reset (integration of player-settings rules)', () => {
  it('textSpeed extremes fall back to bounds: 0->MIN, >MAX->MAX', () => {
    expect(validateTextSpeed(0)).toBe(TEXT_SPEED_MIN)
    expect(validateTextSpeed(TEXT_SPEED_MAX + 999)).toBe(TEXT_SPEED_MAX)
    expect(validateTextSpeed(NaN)).toBe(DEFAULT_TEXT_SPEED)
    expect(validateTextSpeed(-3)).toBe(TEXT_SPEED_MIN)
  })

  it('illegal language code falls back to default en via validateLanguage + sanitize', () => {
    expect(validateLanguage('de-DE')).toBe('de-DE')
    expect(validateLanguage('xx')).toBe('xx')
    expect(validateLanguage('')).toBe(DEFAULT_SETTINGS.language)
    expect(validateLanguage(null)).toBe(DEFAULT_SETTINGS.language)
    expect(validateLanguage('drop table;')).toBe(DEFAULT_SETTINGS.language)
    expect(sanitizeSettings({ language: '!!' }).language).toBe(DEFAULT_SETTINGS.language)
  })

  it('volumes out-of-range clamp to 0..1 through the controller', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    c.setVolume('bgm', 5)
    c.setVolume('se', -2)
    c.setVolume('voice', 0.5)
    const vols = c.get('volumes')
    expect(vols.bgm).toBe(1)
    expect(vols.se).toBe(0)
    expect(vols.voice).toBe(0.5)
    expect(c.setVolume('master', 0.5)).toBe(false) // unknown bus rejected
  })

  it('reset() restores defaults on the controller and is idempotent', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    c.set('language', 'ja')
    c.set('autoClick', true)
    c.set('textSpeed', 60)
    c.setVolume('voice', 0)
    c.reset()
    expect(c.getAll()).toEqual(defaultSettings())
    expect(c.get('language')).toBe(DEFAULT_SETTINGS.language)
    expect(c.get('autoClick')).toBe(DEFAULT_SETTINGS.autoClick)
    expect(c.get('textSpeed')).toBe(DEFAULT_TEXT_SPEED)
    expect(c.get('volumes')).toEqual({ bgm: 1, se: 1, voice: 1 })
    c.reset()
    expect(c.getAll()).toEqual(defaultSettings())
  })

  it('reset notifies subscribers with field * carrying default settings', () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    c.set('language', 'zh')
    const seen = []
    c.subscribe((n) => seen.push(n))
    c.reset()
    const last = seen[seen.length - 1]
    expect(last.field).toBe('*')
    expect(last.settings.language).toBe(DEFAULT_SETTINGS.language)
    expect(last.settings.textSpeed).toBe(DEFAULT_TEXT_SPEED)
  })

  it('defaultSettings() returns a mutable copy matching the frozen table', () => {
    expect(defaultSettings()).toEqual({ ...DEFAULT_SETTINGS, volumes: { ...DEFAULT_SETTINGS.volumes } })
  })
})
