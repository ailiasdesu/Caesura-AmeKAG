// @vitest-environment jsdom
// Save/load slot boundary & lifecycle tests for the Caesar web player.
// Injected in-memory Map storage backend (no localStorage): proves the bridge has
// no hard dependency on localStorage and lets corrupt payloads be seeded for checks.
import { describe, it, expect, beforeAll, beforeEach } from 'vitest'
import { readFileSync, existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'
import { createPlayer } from './bridge.js'

const here = dirname(fileURLToPath(import.meta.url))
const scriptsDir = join(here, '..', 'scripts')
const index = JSON.parse(readFileSync(join(here, 'scripts-index.json'), 'utf8'))
const fileFetch = async (url) => {
  const u = new URL(url)
  const rel = u.pathname.replace('/scripts/', '')
  const p = join(scriptsDir, ...rel.split('/'))
  const ok = existsSync(p)
  return { ok, status: ok ? 200 : 404, text: async () => (ok ? readFileSync(p, 'utf8') : ''), json: async () => index }
}

const store = new Map()
const storageBackend = { get: (k) => (store.has(k) ? store.get(k) : null), set: (k, v) => { store.set(k, v); return true }, del: (k) => { store.delete(k) } }
const SAVE_PREFIX = 'caesura.save.'
const clearSlots = () => { for (const k of [...store.keys()]) if (k.startsWith(SAVE_PREFIX)) store.delete(k) }
let player = null
beforeAll(async () => {
  player = await createPlayer({ scriptsBase: 'http://local/scripts/', fetchImpl: fileFetch, wasmFile: join(here, 'node_modules', 'wasmoon', 'dist', 'glue.wasm'), storageBackend })
})
beforeEach(() => { clearSlots(); player.core.events.length = 0; player.core.backlog.length = 0 })
const pageText = () => (player.core.draws || []).map((d) => d.t || '').join('')
const backlogOf = () => (player.core.backlog || []).map((b) => b.text).join(' | ')
const NL = String.fromCharCode(10)

describe('web save-slot boundary & lifecycle (injected memory backend)', () => {
  it('slot lifecycle: empty list, save reflects, delete removes', async () => {
    expect(player.listSlots()).toEqual([])
    const sceneA = ['[ch name="N" text="L1"]', '[p]', '[ch name="N" text="L2"]', '[set f.c = 5]', '[save slot=1]', '[p]', '[end]'].join(NL)
    await player.runScene(sceneA, 'lifecycle.ks', { maxFrames: 200000, autoClick: true })
    const slots = player.listSlots()
    expect(slots.length).toBe(1)
    const s1 = slots.find((s) => s.slot === 1)
    expect(s1).toBeTruthy()
    expect(s1.scene).toContain('lifecycle.ks')
    expect(s1.token).toBeGreaterThan(0)
    expect(s1.savedAt).toBeGreaterThan(0)
    expect(player.deleteSlot(1)).toBe(true)
    expect(player.deleteSlot(1)).toBe(false)
    expect(player.listSlots()).toEqual([])
  })
  it('save at a [p] park and load does not replay the pages before the park', async () => {
    const sceneA = ['[ch name="N" text="P1"]', '[p]', '[ch name="N" text="P2"]', '[p]', '[ch name="N" text="P3"]', '[p]', '[ch name="N" text="P4"]', '[p]', '[end]'].join(NL)
    let out = await player.runScene(sceneA, 'wait.ks', { maxFrames: 50000 })
    expect(out.startsWith('WAIT:')).toBe(true)
    expect(pageText()).toContain('P1')
    out = await player.runScene(sceneA, 'wait.ks', { maxFrames: 50000, advance: true, advanceScene: 'wait.ks' })
    expect(out.startsWith('WAIT:')).toBe(true)
    expect(pageText()).toContain('P2')
    expect(await player.saveCurrent(3)).toBe(true)
    const s = player.listSlots().find((x) => x.slot === 3)
    expect(s).toBeTruthy()
    expect(s.scene).toContain('wait.ks')
    expect(s.token).toBeGreaterThan(1)
    player.core.backlog.length = 0
    out = await player.loadSlot(3, { sceneSources: { 'wait.ks': sceneA } })
    expect(out.startsWith('DONE:')).toBe(true)
    const texts = backlogOf()
    expect(texts).toContain('P2')
    expect(texts).toContain('P4')
    expect(texts).not.toContain('P1')
  }, 120000)
  it('multiple slots stay isolated: each load restores its own scene and vars', async () => {
    const textA = "A:${f.hero}:${f.level}"
    const sceneA = ['[set f.hero = "sakura"]', '[set f.level = 10]', '[save slot=1]', '[ch name="N" text=' + textA + ']', '[p]', '[end]'].join(NL)
    const textB = "B:${f.hero}:${f.level}"
    const sceneB = ['[set f.hero = "teacher"]', '[set f.level = 99]', '[save slot=2]', '[ch name="N" text=' + textB + ']', '[p]', '[end]'].join(NL)
    await player.runScene(sceneA, 'scene_a.ks', { maxFrames: 200000, autoClick: true })
    player.core.backlog.length = 0
    await player.runScene(sceneB, 'scene_b.ks', { maxFrames: 200000, autoClick: true })
    const slots = player.listSlots()
    expect(slots.find((s) => s.slot === 1)?.scene).toContain('scene_a.ks')
    expect(slots.find((s) => s.slot === 2)?.scene).toContain('scene_b.ks')
    const sources = { 'scene_a.ks': sceneA, 'scene_b.ks': sceneB }
    player.core.backlog.length = 0
    await player.loadSlot(1, { sceneSources: sources })
    expect(backlogOf()).toContain('A:sakura:10')
    player.core.backlog.length = 0
    await player.loadSlot(2, { sceneSources: sources })
    expect(backlogOf()).toContain('B:teacher:99')
    player.core.backlog.length = 0
    await player.loadSlot(1, { sceneSources: sources })
    expect(backlogOf()).toContain('A:sakura:10')
  }, 120000)
  it('advance after a load acts on the restored scene, not the loader', async () => {
    const sceneA = ['[ch name="N" text="Q1"]', '[p]', '[set f.m = 41]', '[save slot=4]', '[ch name="N" text="QX post-save"]', '[p]', '[ch name="N" text="QY"]', '[p]', '[end]'].join(NL)
    await player.runScene(sceneA, 'adv_scene.ks', { maxFrames: 200000, autoClick: true })
    player.core.backlog.length = 0
    const lout = await player.loadSlot(4, { sceneSources: { 'adv_scene.ks': sceneA }, autoClick: false })
    expect(lout.startsWith('WAIT:')).toBe(true)
    expect(pageText()).toContain('Loading slot 4')
    const a1 = await player.runScene(sceneA, 'adv_scene.ks', { maxFrames: 60000, advance: true, advanceScene: 'adv_scene.ks' })
    expect(player._ctx).toBeTruthy()
    const ctxName = String(player._ctx.current_scene || player._ctx.currentScene || '')
    expect(ctxName).not.toContain('loadslot.ks')
    expect(ctxName).toContain('adv_scene.ks')
    expect(typeof a1).toBe('string')
    expect(a1.length).toBeGreaterThan(0)
    const loaderShown = () => backlogOf().split('Loading slot 4').length - 1
    const before = loaderShown()
    const a2 = await player.runScene(sceneA, 'adv_scene.ks', { maxFrames: 60000, advance: true, advanceScene: 'adv_scene.ks' })
    expect(typeof a2).toBe('string')
    expect(String(player._ctx.current_scene || '')).toContain('adv_scene.ks')
    expect(loaderShown()).toBeLessThanOrEqual(before + 1)
  }, 120000)
  it('tolerates corrupt/illegal save payloads (list + load both survive)', async () => {
    store.set(SAVE_PREFIX + '11', '{ this is not valid json !!')
    store.set(SAVE_PREFIX + '12', JSON.stringify({ state: 'just-a-string', scene: 7, token: 'abc', savedAt: null }))
    store.set(SAVE_PREFIX + '13', JSON.stringify({ state: { token_index: 5 } }))
    store.set(SAVE_PREFIX + '14', JSON.stringify({ state: { token_index: 2, scene_path: 'demo/toler.ks', f: { ok: 1 } }, scene: 'demo/toler.ks', token: 2, savedAt: 1 }))
    const slotsSeen = player.listSlots().map((s) => s.slot)
    expect(slotsSeen).not.toContain(11)
    expect(slotsSeen).toContain(12)
    expect(slotsSeen).toContain(13)
    expect(slotsSeen).toContain(14)
    for (const bad of [11, 12, 13]) {
      player.core.events.length = 0
      const out = await player.loadSlot(bad, { sceneSources: { 'toler.ks': '[ch name="N" text="T"]' + NL + '[p]' + NL + '[end]' }, autoClick: true, maxFrames: 60000 })
      expect(typeof out).toBe('string')
      expect(out).not.toBe('')
      const sawRead = player.core.events.some((e) => e.kind === 'save.read' && e.detail?.slot === bad)
      expect(sawRead).toBe(true)
    }
    player.core.backlog.length = 0
    const okOut = await player.loadSlot(14, { sceneSources: { 'toler.ks': '[ch name="N" text="T-OK"]' + NL + '[p]' + NL + '[end]' }, autoClick: true, maxFrames: 60000 })
    expect(okOut).toBeTruthy()
  }, 120000)
  it('runs on an injected Map backend without touching localStorage', async () => {
    const beforeLs = Object.keys(localStorage).filter((k) => k.startsWith(SAVE_PREFIX)).length
    const sceneA = ['[save slot=21]', '[end]'].join(NL)
    await player.runScene(sceneA, 'mem.ks', { maxFrames: 200000, autoClick: true })
    const raw = store.get(SAVE_PREFIX + '21')
    expect(raw).toBeTruthy()
    const payload = JSON.parse(raw)
    expect(String(payload.scene)).toContain('mem.ks')
    const afterLs = Object.keys(localStorage).filter((k) => k.startsWith(SAVE_PREFIX)).length
    expect(afterLs).toBe(beforeLs)
    expect(player.deleteSlot(21)).toBe(true)
    expect(store.has(SAVE_PREFIX + '21')).toBe(false)
  })
})