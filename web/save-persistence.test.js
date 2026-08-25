// @vitest-environment jsdom
// Plan W2 — Web Storage / Save Provider persistence & edge contract.
//
// Unlike slots.boundary.test.js (injected Map backend), this suite drives the
// bridge with its DEFAULT backend = the real jsdom localStorage, and creates a
// SECOND player instance on the same origin: the plan W2 requirement "browser
// reload 后数据仍存在" is verified at the bridge level (a reload == a fresh
// engine over the same storage), plus overwrite / invalid slot / empty save /
// corrupt payload / quota-failure observability.
import { describe, it, expect, beforeAll, beforeEach } from 'vitest'
import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'
import { createPlayer } from './bridge.js'

const here = dirname(fileURLToPath(import.meta.url))
const scriptsDir = join(here, '..', 'scripts')
const wasmFile = join(here, 'node_modules', 'wasmoon', 'dist', 'glue.wasm')
const index = JSON.parse(readFileSync(join(here, 'scripts-index.json'), 'utf8'))
const fileFetch = async (url) => {
  const u = new URL(url)
  const rel = u.pathname.replace('/scripts/', '')
  const p = join(scriptsDir, ...rel.split('/'))
  const ok = existsSync(p)
  return { ok, status: ok ? 200 : 404, text: async () => (ok ? readFileSync(p, 'utf8') : ''), json: async () => index }
}

const SAVE_PREFIX = 'caesura.save.'
const NL = String.fromCharCode(10)
const clearSlots = () => {
  for (const k of Object.keys(localStorage)) if (k.startsWith(SAVE_PREFIX)) localStorage.removeItem(k)
}
const mk = () => createPlayer({ scriptsBase: 'http://local/scripts/', fetchImpl: fileFetch, wasmFile })

describe('W2 web storage persistence (real localStorage + fresh instance)', () => {
  beforeAll(async () => { await mk() })
  beforeEach(() => { clearSlots() })

  it('round trip + overwrite: a slot re-saved stays a single entry, updated', async () => {
    const p = await mk()
    const sceneA = ['[ch name="N" text="L1"]', '[p]', '[set f.c = 1]', '[save slot=5]', '[p]', '[end]'].join(NL)
    await p.runScene(sceneA, 'w2.ks', { maxFrames: 200000, autoClick: true })
    expect(await p.saveCurrent(5)).toBe(true)
    let slots = p.listSlots()
    expect(slots.length).toBe(1)
    const first = slots[0]
    expect(first.slot).toBe(5)
    expect(first.scene).toContain('w2.ks')
    // overwrite: same slot under a DIFFERENT scene -> still ONE entry, updated
    const sceneB = ['[ch name="N" text="L2"]', '[p]', '[set f.c = 2]', '[save slot=5]', '[p]', '[end]'].join(NL)
    await p.runScene(sceneB, 'other.ks', { maxFrames: 200000, autoClick: true })
    expect(await p.saveCurrent(5)).toBe(true)
    slots = p.listSlots()
    expect(slots.length).toBe(1)
    expect(slots[0].scene).toContain('other.ks')
  }, 120000)

  it('a fresh player instance sees the slot (browser reload persistence)', async () => {
    const p1 = await mk()
    const scene = ['[ch name="N" text="persist"]', '[save slot=8]', '[end]'].join(NL)
    await p1.runScene(scene, 'w2.ks', { maxFrames: 200000, autoClick: true })
    expect(p1.listSlots().some((s) => s.slot === 8)).toBe(true)
    // "reload": brand-new engine over the same localStorage
    const p2 = await mk()
    const seen = p2.listSlots()
    expect(seen.some((s) => s.slot === 8)).toBe(true)
    expect(seen.find((s) => s.slot === 8).scene).toContain('w2.ks')
    // and loadSlot against the fresh instance restores it
    const lout = await p2.loadSlot(8, { sceneSources: { 'w2.ks': scene } })
    expect(typeof lout).toBe('string')
    expect(lout.length).toBeGreaterThan(0)
  }, 120000)

  it('storageStats reports slots + payload bytes for the UI', async () => {
    const p = await mk()
    expect(p.storageStats()).toEqual({ slots: 0, bytesUsed: 0 })
    const scene = ['[ch name="N" text="stat"]', '[save slot=2]', '[end]'].join(NL)
    await p.runScene(scene, 'w2.ks', { maxFrames: 200000, autoClick: true })
    const st = p.storageStats()
    expect(st.slots).toBe(1)
    expect(st.bytesUsed).toBeGreaterThan(0)
  }, 120000)

  it('invalid slots are rejected honestly (saveCurrent/deleteSlot)', async () => {
    const p = await mk()
    for (const bad of [-1, 1.5, 100, 1000, 'x', null]) {
      expect(await p.saveCurrent(bad)).toBe(false)
      expect(p.deleteSlot(bad)).toBe(false)
    }
    // no storage written for any invalid slot
    expect(Object.keys(localStorage).filter((k) => k.startsWith(SAVE_PREFIX)).length).toBe(0)
  })

  it('empty save (no scene has run) returns false without touching storage', async () => {
    const p = await mk()
    expect(await p.saveCurrent(9)).toBe(false)
    expect(p.listSlots().length).toBe(0)
  })

  it('corrupt payloads in REAL localStorage are skipped without crashing', async () => {
    const p = await mk()
    localStorage.setItem(SAVE_PREFIX + '40', '{ not json !!')
    localStorage.setItem(SAVE_PREFIX + '41', JSON.stringify({ scene: 7, token: 'x' }))
    const slots = p.listSlots()
    expect(slots.some((s) => s.slot === 40)).toBe(false)
    expect(slots.some((s) => s.slot === 41)).toBe(true) // parseable, lenient metadata
    const out = await p.loadSlot(40, { sceneSources: {}, autoClick: true, maxFrames: 60000 })
    expect(typeof out).toBe('string')
    expect(p.listSlots().some((s) => s.slot === 40)).toBe(false)
  }, 120000)

  it('quota / backend failure is observable: save.write ok:false + saveCurrent false', async () => {
    const failing = { get: () => null, set: () => false, del: () => {} }
    const p = await createPlayer({ scriptsBase: 'http://local/scripts/', fetchImpl: fileFetch, wasmFile, storageBackend: failing })
    // engine [save] path: Lua reports error, we log save.write ok:false
    const scene = ['[ch name="N" text="q"]', '[save slot=7]', '[end]'].join(NL)
    await p.runScene(scene, 'w2.ks', { maxFrames: 200000, autoClick: true })
    const write = p.core.events.find((e) => e.kind === 'save.write' && Number(e.detail?.slot) === 7)
    expect(write?.detail?.ok).toBe(false)
    // saveCurrent degrades to false (UI then surfaces the failure)
    expect(await p.saveCurrent(7)).toBe(false)
  }, 120000)
})
