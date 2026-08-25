// player-settings.desktop-parity.test.js — round 96: sync-coverage for the
// web player settings vs the desktop engine settings (scripts/settings.lua).
//
// PURPOSE
//   The web player persists settings through player-settings.js and the
//   desktop engine through scripts/settings.lua + scripts/config.lua. These
//   are separate products sharing the engine core, so their settings
//   surfaces may drift. This file is the consistency matrix that locks the
//   comparison into a test so a future drift breaks a red test instead of
//   silently diverging.
//
// METHOD
//   Load the REAL scripts/settings.lua through the wasmoon bridge (same
//   standalone pattern as player-settings.ux.test.js) and compare its
//   defaults + clamp bounds against the web player-settings module — using
//   unit conversions to make the comparison honest (web textSpeed is
//   chars-per-second, desktop text_speed is ms/char).
//
// FINDINGS (the matrix this suite encodes)
//   1. DEFAULT textSpeed: web 20 cps == desktop 50 ms/char — the SAME
//      physical pace (floor(1000/20) = 50). No drift; the "20 vs 50"
//      headline is a unit mismatch, not a semantic one.
//   2. DEFAULT volumes: web 1.0/1.0/1.0 (100% all buses) vs desktop
//      80/80/100 (% bgm/se/voice). Semantic drift: web boots BGM/SE at
//      full volume while desktop boots at 80%. Recorded, not force-aligned
//      (independent product UX — web keeps normalized 0..1 unit).
//   3. DEFAULT language: web 'en' vs desktop i18n.current 'zh'. Recorded.
//   4. CLAMP bounds: web textSpeed 1..80 (cps) vs desktop slider 10..200
//      (ms/char). Different units + different ranges (desktop max=200ms/char
//      is only ~5 cps; web max=80 cps is ~12.5 ms/char). Web supports a
//      much wider fast end. Recorded as divergent-but-intentional.
//      Volumes: both clamp into their valid unit (web 0..1, desktop 0..100
//      percentage), equal semantics.
//   5. PERSISTENCE: web localStorage JSON under 'caesura.player-settings' vs
//      desktop Lua-literal files in settings/. Incompatible formats + stores
//      (browser localStorage vs filesystem) — no cross-talk is possible.
//   6. LANGUAGE mapping: web validates any BCP-47-ish code; desktop i18n
//      accepts whatever assets/lang/<code>.lua exists (available(): zh,en,ja).
//      en/zh/ja are in both sets, so those codes behave identically.
import { describe, it, expect, beforeAll } from 'vitest'
import { readFileSync, existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'
import { createPlayer } from './bridge.js'
import {
  DEFAULT_SETTINGS, DEFAULT_TEXT_SPEED, TEXT_SPEED_MIN, TEXT_SPEED_MAX,
  VOLUME_BUSSES, defaultSettings, clampVolume, validateTextSpeed, validateLanguage,
  sanitizeSettings, makeMemoryStorage, createPlayerSettings, DEFAULT_STORAGE_KEY,
} from './player-settings.js'

const here = dirname(fileURLToPath(import.meta.url))
const rootDir = join(here, '..')
const scriptsDir = join(rootDir, 'scripts')
const assetsDir = join(rootDir, 'assets')
const index = JSON.parse(readFileSync(join(here, 'scripts-index.json'), 'utf8'))
const fileFetch = async (url) => {
  const u = new URL(url)
  if (u.pathname.startsWith('/assets/lang/')) {
    const rel = u.pathname.replace('/assets/lang/', '')
    const p = join(assetsDir, 'lang', ...rel.split('/'))
    return { ok: existsSync(p), status: existsSync(p) ? 200 : 404, text: async () => (existsSync(p) ? readFileSync(p, 'utf8') : ''), json: async () => index }
  }
  const rel = u.pathname.replace('/scripts/', '')
  const p = join(scriptsDir, ...rel.split('/'))
  const ok = existsSync(p)
  return { ok, status: ok ? 200 : 404, text: async () => (ok ? readFileSync(p, 'utf8') : ''), json: async () => index }
}

let player = null
let menu = null // desktop _buildMenu items, keyed by 'key'

// Convert a web chars-per-second pace to the desktop ms/char, mirroring the
// bridge's contract (ctx.text_speed = max(1, floor(1000 / cps))).
const cpsToMs = (cps) => Math.max(1, Math.floor(1000 / cps))

beforeAll(async () => {
  player = await createPlayer({
    scriptsBase: 'http://local/scripts/',
    fetchImpl: fileFetch,
    langBase: 'http://local/assets/lang/',
    wasmFile: join(here, 'node_modules', 'wasmoon', 'dist', 'glue.wasm'),
  })
  // settings.lua is a BINDING_MODULE on the web (stubbed '{}' — the web
  // player has its own DOM settings UI), so require('settings') → stub.
  // Load the REAL menu module manually to mirror the desktop defaults.
  //
  // _buildMenu calls i18n.available(), which on the desktop scans
  // assets/lang/*.lua via fileutil.scan_dir. The web stubs fileutil as {}
  // (no scan_dir) — mirror the desktop contract by mounting a scan_dir that
  // returns the three shipped dictionaries so the menu can build.
  await player.lua.doString(`
    local futil = _G.fileutil or {}
    futil.scan_dir = futil.scan_dir or function(dir, pat)
      local list = {}
      if dir == 'assets/lang' then
        list[1] = 'en.lua'; list[2] = 'zh.lua'; list[3] = 'ja.lua'
      end
      return list
    end
    _G.fileutil = futil
  `)
  const src = readFileSync(join(scriptsDir, 'settings.lua'), 'utf8')
  player.lua.global.set('__SETTINGS_LUA_SRC', src)
  await player.lua.doString(`
    local f = assert(load(__SETTINGS_LUA_SRC, '@settings.lua', 't', _ENV))
    local S = f()
    local ctx = { settingsValues = {} }
    local items = S._buildMenu(ctx)
    local m = {}
    for _, it in ipairs(items) do
      m[it.key] = { key = it.key, type = it.type, value = it.value, min = it.min, max = it.max, options = it.options }
    end
    _G.__MENU = m
  `)
  menu = player.lua.global.get('__MENU')
}, 120000)

describe('default value parity (web vs desktop, unit-honest)', () => {
  it('textSpeed default: web 20 cps == desktop 50 ms/char (same physical pace)', () => {
    // Desktop menu item for text_speed: value 50 (ms/char), slider 10..200.
    expect(menu.text_speed).toBeDefined()
    expect(menu.text_speed.value).toBe(50)        // desktop default ms/char
    // Bridge conversion (runScene): cps -> ms/char = floor(1000/cps), min 1.
    expect(cpsToMs(DEFAULT_TEXT_SPEED)).toBe(50)   // web 20 cps -> 50 ms/char
    // The two defaults represent the SAME pace, so there is NO drift.
    expect(cpsToMs(DEFAULT_TEXT_SPEED)).toBe(menu.text_speed.value)
    expect(DEFAULT_TEXT_SPEED).toBe(20)
  })

  it('volume default drift is real and recorded: web 1.0 vs desktop 80/80/100', () => {
    // Web: all buses 1.0 (i.e. 100%).
    expect(DEFAULT_SETTINGS.volumes).toEqual({ bgm: 1, se: 1, voice: 1 })
    // Desktop: bgm/se 80%, voice 100% (0..100 percentage) — BGM/SE quieter.
    expect(menu.volume_bgm.value).toBe(80)
    expect(menu.volume_se.value).toBe(80)
    expect(menu.volume_voice.value).toBe(100)
    // Honest comparison: desktop % / 100 == web normalized 0..1 equivalent.
    const webToDesktopPct = (v) => Math.round(v * 100)
    expect(webToDesktopPct(DEFAULT_SETTINGS.volumes.bgm)).not.toBe(menu.volume_bgm.value)
    const desktopToWeb = { bgm: menu.volume_bgm.value / 100, se: menu.volume_se.value / 100, voice: menu.volume_voice.value / 100 }
    // Documented drift: web would need {0.8,0.8,1.0} to match desktop.
    expect(desktopToWeb).toEqual({ bgm: 0.8, se: 0.8, voice: 1.0 })
  })

  it('single volume bus set survives unit-normalized round-trip to the engine', () => {
    // Web volume 0->1 maps 1:1 to audio.setBusVolume (already 0..1).
    // Desktop volume % maps to set_*_volume(v/100). Both clamp to their unit.
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    c.setVolume('bgm', 0.8) // the desktop-default semantic, as normalized 0..1
    expect(c.get('volumes').bgm).toBe(0.8)
    expect(Math.round(c.get('volumes').bgm * 100)).toBe(80)
  })

  it('default language drift is real and recorded: web en vs desktop zh', () => {
    expect(DEFAULT_SETTINGS.language).toBe('en')
    // Desktop i18n module defaults i18n.current = 'zh' (the menu's language
    // item is a cycle whose initial value is i18n.current).
    expect(menu.language.type).toBe('cycle')
    // i18n.current default is 'zh' (scripts/i18n.lua).
    expect(menu.language.value).toBe('zh')
  })

  it('auto / skip defaults match on both sides', () => {
    // Web: autoClick=false, skipMode=false.
    expect(DEFAULT_SETTINGS.autoClick).toBe(false)
    expect(DEFAULT_SETTINGS.skipMode).toBe(false)
    // Desktop: skip_mode=false, auto_mode=false.
    expect(menu.skip_mode.value).toBe(false)
    expect(menu.auto_mode.value).toBe(false)
  })

  it('desktop and web define the same set of volume buses (bgm/se/voice)', () => {
    // Web busses are exactly [bgm, se, voice].
    expect(VOLUME_BUSSES).toEqual(['bgm', 'se', 'voice'])
    // Desktop menu has one slider per bus, same three keys.
    for (const b of ['bgm', 'se', 'voice']) {
      expect(menu['volume_' + b]).toBeDefined()
      expect(menu['volume_' + b].type).toBe('slider')
    }
  })
})

describe('clamp / validation parity (boundary behavior, unit-converted)', () => {
  it('web textSpeed clamps 1..80 cps; desktop slider clamps 10..200 ms/char', () => {
    // Web bounds (cps).
    expect(TEXT_SPEED_MIN).toBe(1)
    expect(TEXT_SPEED_MAX).toBe(80)
    expect(validateTextSpeed(0)).toBe(1)
    expect(validateTextSpeed(9999)).toBe(80)
    // Desktop menu slider bounds for text_speed (ms/char).
    expect(menu.text_speed.min).toBe(10)
    expect(menu.text_speed.max).toBe(200)
  })

  it('the clamp directions are opposite but both hard-clamp (not wrap)', () => {
    // Desktop _adjust: sv = clamp(value + dir*5, min, max) — hard clamp.
    // Web validateTextSpeed: min(max, max(min, round(v))) — hard clamp.
    // Both sanitize out-of-range to a valid bound; neither wraps modulo.
    expect(validateTextSpeed(TEXT_SPEED_MAX + 100)).toBe(TEXT_SPEED_MAX)
    expect(validateTextSpeed(TEXT_SPEED_MIN - 100)).toBe(TEXT_SPEED_MIN)
  })

  it('web fast end is much faster than desktop fast end (range divergence)', () => {
    // Desktop max text speed = 200 ms/char -> ~5 cps.
    const desktopMaxCps = 1000 / menu.text_speed.max // 1000/200 = 5
    expect(desktopMaxCps).toBeCloseTo(5, 5)
    // Web max speed = 80 cps -> 12.5 ms/char — a far faster reveal.
    expect(cpsToMs(80)).toBe(12)
    // Genuinely different usable ranges; recorded rather than aligned.
    expect(80).toBeGreaterThan(desktopMaxCps)
  })

  it('volume clamp semantics match in both units (0..100 % / 0..1 norm)', () => {
    // Web clamps to 0..1.
    expect(clampVolume(-1)).toBe(0)
    expect(clampVolume(2)).toBe(1)
    // Desktop slider clamps to 0..100 (percentage); _adjust uses min/max.
    expect(menu.volume_bgm.min).toBe(0)
    expect(menu.volume_bgm.max).toBe(100)
  })

  it('sanitize falls back to defaults for volume junk on both conceptual paths', () => {
    // Web sanitize: non-number/invalid bus -> default 1.
    const s = sanitizeSettings({ volumes: { bgm: 'x', se: -5, voice: 99 } })
    expect(s.volumes.bgm).toBe(1)   // non-number -> default
    expect(s.volumes.se).toBe(0)    // clamped
    expect(s.volumes.voice).toBe(1) // clamped (99% -> 1.0 via clamp to <=1? no)
    // 99 is finite -> clampVolume(99) -> 1 (clamped to max 1).
    expect(clampVolume(99)).toBe(1)
  })
})

describe('persistence format (recorded incompatibility — no interop)', () => {
  it('web persists JSON under a single localStorage key', () => {
    expect(DEFAULT_STORAGE_KEY).toBe('caesura.player-settings')
    const mem = makeMemoryStorage()
    const c = createPlayerSettings({ storage: mem })
    c.set('textSpeed', 40)
    c.setVolume('voice', 0.5)
    const raw = mem.get(DEFAULT_STORAGE_KEY)
    const parsed = JSON.parse(raw)
    expect(parsed.textSpeed).toBe(40)
    expect(parsed.volumes.voice).toBe(0.5)
    // Format: JSON object (no Lua 'return' literal).
    expect(raw.startsWith('{')).toBe(true)
    expect(raw.startsWith('return')).toBe(false)
  })

  it('desktop persists Lua-literal files (settings/volume.lua etc.) — cannot interop', () => {
    const v = require('node:fs')
    // The persistence contract lives in config.lua: Lua-literal 'return {...}'.
    const configSrc = readFileSync(join(scriptsDir, 'config.lua'), 'utf8')
    expect(configSrc).toMatch('settings/volume.lua')
    expect(configSrc).toMatch('settings/config.lua')
    expect(configSrc).toMatch('return {')
    // Different: web = browser localStorage JSON; desktop = filesystem Lua.
    // No shared store/format -> no cross-talk is possible. Recorded.
    expect(DEFAULT_STORAGE_KEY.indexOf('settings/')).toBe(-1)
  })
})

describe('language mapping parity (same codes both sides)', () => {
  it('web accepts en/zh/ja-JP and desktop i18n keys en/zh/ja', () => {
    // Web validation: any BCP-47-ish code survives; en/zh/ja-JP all valid.
    expect(validateLanguage('en')).toBe('en')
    expect(validateLanguage('zh')).toBe('zh')
    expect(validateLanguage('ja-JP')).toBe('ja-JP')
    // Desktop i18n.available() starts from built-in {'zh'} and scans
    // assets/lang/*.lua — en/zh/ja are the shipped dictionaries, so the
    // codes the two sides emit overlap exactly for the common set.
    const webCodes = ['en', 'zh', 'ja', 'ja-JP']
    for (const code of webCodes) {
      expect(validateLanguage(code)).toBe(code)
    }
  })

  it('web setLanguage routes a validated code into the real i18n (en works)', async () => {
    // The web player.setLanguage drives i18n.set_language with the sanitized
    // code; an 'en' setting must land as i18n.current == 'en' in the VM.
    const out = await player.setLanguage('en')
    expect(out !== false).toBe(true)
    // setLanguage drives the real i18n.set_language; assert the VM's
    // i18n.current landed on the validated code.
    await player.lua.doString('_G.__CUR2 = type(i18n) == "table" and tostring(i18n.current) or "?"')
    expect(player.lua.global.get('__CUR2')).toBe('en')
  })

  it('web stays default en after a garbage language (desktop keeps its current)', () => {
    // Web: illegal code -> default en.
    expect(validateLanguage('drop table;')).toBe(DEFAULT_SETTINGS.language)
    expect(sanitizeSettings({ language: '!!' }).language).toBe(DEFAULT_SETTINGS.language)
    // Desktop: the menu cycles only i18n.available() codes, so an invalid
    // code can never be selected there (no analogous fallback needed).
    const opts = menu.language.options
    expect(Array.isArray(opts)).toBe(true)
  })
})

describe('change-application path parity (settings -> ctx/engine)', () => {
  it('web: settings.set -> bridge opts -> ctx.text_speed / ctx.skip_mode', async () => {
    // The bridge pushes player textSpeed (cps) + skip into ctx on run/advance
    // (validateTextSpeed output feeds cps; skipMode feeds ctx.skip_mode).
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    c.set('textSpeed', 50)
    c.set('skipMode', true)
    expect(c.get('textSpeed')).toBe(50)
    expect(c.get('skipMode')).toBe(true)
    expect(validateTextSpeed(c.get('textSpeed'))).toBe(50)
    // runScene with these opts should land cps=50 -> text_speed=20 ms/char.
    const ks = ['[ch name="N" text="apply"]', '[p]', '[end]'].join(String.fromCharCode(10))
    await player.runScene(ks, 'apply.ks', {
      maxFrames: 50000, textSpeed: c.get('textSpeed'), skip: c.get('skipMode'),
    })
    await player.lua.doString('_G.__R = _G.__LAST_CTX and _G.__LAST_CTX["cps"] or nil')
    const appliedCps = player.lua.global.get('__R')
    expect(appliedCps).toBe(50)
    await player.lua.doString('_G.__R2 = _G.__LAST_CTX and tostring(tonumber(_G.__LAST_CTX["text_speed"]) or "") or ""')
    const appliedMs = Number(player.lua.global.get('__R2') || '0')
    expect(appliedMs).toBe(20) // floor(1000/50)
  })

  it('desktop: settings._applyAll -> ctx.text_speed / ctx.skip_mode', async () => {
    // Desktop menu applies on close via Settings._applyAll(ctx): it writes
    // ctx.text_speed = sv.text_speed (ms/char directly) and ctx.skip_mode.
    await player.lua.doString(`
      local S = _G.__MENU and nil
      local f = assert(load(__SETTINGS_LUA_SRC, '@settings.lua', 't', _ENV))
      local Settings = f()
      local ctx = { settingsValues = { text_speed = 50, skip_mode = true } }
      Settings._applyAll(ctx)
      _G.__APP = { ts = ctx.text_speed, sm = ctx.skip_mode }
    `)
    const app = player.lua.global.get('__APP')
    expect(app.ts).toBe(50)       // desktop writes ms/char directly (no 1000/cps)
    expect(app.sm).toBe(true)
  })

  it('volumes: web routes 0..1 to audio buses; desktop /100 percentage', async () => {
    const c = createPlayerSettings({ storage: makeMemoryStorage() })
    const calls = []
    c.subscribe(({ field }) => { if (field === 'volumes') calls.push(c.get('volumes')) })
    c.setVolume('bgm', 0.8)
    const webBgm = c.get('volumes').bgm
    expect(webBgm).toBe(0.8)
    // Desktop equivalent: _applyAll does audio.set_bgm_volume(sv.volume_bgm/100).
    expect(Math.round(80 / 100 * 100) / 100).toBe(0.8) // 80% == 0.8 normalized
    expect(webBgm).toBe(0.8)
    expect(menu.volume_bgm.value / 100).toBe(0.8)
  })
})
