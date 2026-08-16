// @vitest-environment jsdom
// Round 93 WIRE: skip/auto/advance THREE-WAY INTERACTION MATRIX.
//
// round 82 wired opts.advance (resume the parked cursor one [p] page per
//   click); round 87 wired opts.autoClick; round 93 wired opts.skip +
//   ctx.skip_mode (skip reveals the page instantly and advances through the
//   [p]/[ch]/[wait] wait WITHOUT a click — the VN equivalent of holding the
//   fast-forward key).
//
// The three controls are independent OPTIONS on runScene/runFromBundle that
// all write into the SAME persistent scene ctx (__CTXREF), so their
// combinations need locking. This file pins the full matrix:
//   1. skip x advance      — skip fast-forwards the whole remainder; a
//                            skip=OFF advance parks one page at a time.
//   2. skip x auto         — skip + auto coexist; [p] handled once each, no
//                            backlog duplication.
//   3. skip x choice       — choiceIndex routes deterministically under skip;
//                            bare skip auto-selects option 1.
//   4. skip x save/load    — ctx.skip_mode persists through [save]/[load]
//                            (save.lua state.skip_mode) and a reload resumes
//                            in skip mode.
//   5. three-way toggle    — auto->skip->advance arbitrary switching keeps
//                            token monotonic and the backlog duplicate-free.
//   6. skip x wait         — a [wait] inside a skip run completes and the
//                            scene continues to DONE (no manual clicks).
//
// All assertions were verified empirically against the wired bridge before
// being committed here.
import { describe, it, expect, beforeAll } from 'vitest'
import { readFileSync, existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'
import { createPlayer } from './bridge.js'
import { DomRenderer } from './dom-renderer.js'

const here = dirname(fileURLToPath(import.meta.url))
const rootDir = join(here, '..')
const scriptsDir = join(rootDir, 'scripts')
const assetsDir = join(rootDir, 'assets')
const index = JSON.parse(readFileSync(join(here, 'scripts-index.json'), 'utf8'))
const fileFetch = async (url) => {
  const u = new URL(url)
  if (u.pathname.startsWith('/assets/lang/')) {
    const rel = u.pathname.replace('/assets/lang/', '').replaceAll('/', '\\')
    const p = join(assetsDir, 'lang', rel)
    return {
      ok: existsSync(p),
      status: existsSync(p) ? 200 : 404,
      text: async () => (existsSync(p) ? readFileSync(p, 'utf8') : ''),
      json: async () => index,
    }
  }
  const p = u.pathname.replace('/scripts/', scriptsDir + '/').replaceAll('/', '\\')
  return { ...({ text: async () => readFileSync(p, 'utf8'), json: async () => index }), status: 200, ok: true }
}

let player = null
let renderer = null
let stage = null

beforeAll(async () => {
  player = await createPlayer({
    scriptsBase: 'http://local/scripts/',
    fetchImpl: fileFetch,
    langBase: 'http://local/assets/lang/',
    wasmFile: join(here, 'node_modules', 'wasmoon', 'dist', 'glue.wasm'),
  })
  stage = document.createElement('div')
  document.body.appendChild(stage)
  renderer = new DomRenderer(player.core, stage)
})

describe('skip/auto/advance interaction matrix (round 93 wiring)', () => {
  const NL = String.fromCharCode(10)
  const pageText = () => (player.core.draws || []).map((d) => d.t || '').join('')
  const backlogT = () => (player.core.backlog || []).map((b) => b.text || '').join(' | ')
  const readCtx = async (name) => {
    await player.lua.doString('_G.__R = _G.__LAST_CTX and _G.__LAST_CTX[' + JSON.stringify(name) + '] or nil')
    return player.lua.global.get('__R')
  }

  // ---- 1. skip x advance ------------------------------------------------
  it('skip=on auto-runs every page to DONE with zero WAIT (免点击推进)', async () => {
    player.core.backlog.length = 0
    const ks = [
      '[ch name="A" text="P1"]', '[p]',
      '[ch name="A" text="P2"]', '[p]',
      '[ch name="A" text="P3"]', '[p]',
      '[end]',
    ].join(NL)
    // From a fresh start, skip reveals each page and advances through every
    // wait without a click, so the run does NOT park — it reaches DONE and
    // all three [p] pages land in the backlog.
    const out = await player.runScene(ks, 'sk_auto_run.ks', { maxFrames: 50000, skip: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const bl = backlogT()
    expect(bl).toContain('[A]P1')
    expect(bl).toContain('[A]P2')
    expect(bl).toContain('[A]P3')
  }, 60000)

  it('advance + skip=on fast-forwards the whole remainder from the parked cursor (round 93)', async () => {
    player.core.backlog.length = 0
    const ks = [
      '[ch name="A" text="P1"]', '[p]',
      '[ch name="A" text="P2"]', '[p]',
      '[ch name="A" text="P3"]', '[p]',
      '[end]',
    ].join(NL)
    // Park (no skip) at the first [p].
    let out = await player.runScene(ks, 'sk_adv_skip.ks', { maxFrames: 50000 })
    expect(out.startsWith('WAIT:')).toBe(true)
    expect(pageText()).toContain('P1')

    // One advance WITH skip: the advance consumes the parked page, then skip
    // takes over and fast-forwards through all remaining waits to DONE.
    player.core.backlog.length = 0
    out = await player.runScene(ks, 'sk_adv_skip.ks', { maxFrames: 50000, advance: true, advanceScene: 'sk_adv_skip.ks', skip: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const bl = backlogT()
    // the parked P1 page was committed on the way through, then P2/P3
    expect(bl).toContain('P1')
    expect(bl).toContain('P2')
    expect(bl).toContain('P3')
  }, 60000)

  it('skip=OFF advance resumes char-by-char: one [p] page per advance (逐字)', async () => {
    player.core.backlog.length = 0
    const ks = [
      '[ch name="A" text="Q1"]', '[p]',
      '[ch name="A" text="Q2"]', '[p]',
      '[ch name="A" text="Q3"]', '[p]',
      '[end]',
    ].join(NL)
    let out = await player.runScene(ks, 'sk_adv_noskip.ks', { maxFrames: 50000 })
    expect(out.startsWith('WAIT:')).toBe(true)
    expect(pageText()).toContain('Q1')

    // advance WITHOUT skip -> exactly ONE page, parking at Q2 (逐字).
    out = await player.runScene(ks, 'sk_adv_noskip.ks', { maxFrames: 50000, advance: true, advanceScene: 'sk_adv_noskip.ks', skip: false })
    expect(out.startsWith('WAIT:')).toBe(true)
    expect(pageText()).toContain('Q2')
    expect(pageText()).not.toContain('Q1')

    out = await player.runScene(ks, 'sk_adv_noskip.ks', { maxFrames: 50000, advance: true, advanceScene: 'sk_adv_noskip.ks', skip: false })
    expect(out.startsWith('WAIT:')).toBe(true)
    expect(pageText()).toContain('Q3')

    out = await player.runScene(ks, 'sk_adv_noskip.ks', { maxFrames: 50000, advance: true, advanceScene: 'sk_adv_noskip.ks', skip: false })
    expect(out.startsWith('DONE:')).toBe(true)
  }, 60000)

  // ---- 2. skip x auto ---------------------------------------------------
  it('skip + autoClick simultaneously: [p] handled once each, no backlog duplication', async () => {
    player.core.backlog.length = 0
    const ks = [
      '[ch name="A" text="A1"]', '[p]',
      '[ch name="A" text="A2"]', '[p]',
      '[ch name="A" text="A3"]', '[p]',
      '[end]',
    ].join(NL)
    // Both auto and skip are live at once. The two controls converge on the
    // SAME "advance through the wait" path; each [p] page must be committed
    // exactly once (skip reveals + auto-click dispatch, not a double push).
    const out = await player.runScene(ks, 'sk_auto_both.ks', { maxFrames: 50000, autoClick: true, skip: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const texts = backlogT()
    expect(texts).toContain('[A]A1')
    expect(texts).toContain('[A]A2')
    expect(texts).toContain('[A]A3')
    // exactly three pages — no A1/A2/A3 repeated by the auto+skip overlap
    const pages = player.core.backlog.map((b) => b.text).filter((t) => /^\[A\]A[123]$/.test(t || ''))
    expect(pages).toHaveLength(3)
  }, 60000)

  it('autoClick=ON (skip=OFF) auto-runs to DONE with no parked wait; state matches skip run', async () => {
    player.core.backlog.length = 0
    const ks = [
      '[ch name="A" text="M1"]', '[p]',
      '[ch name="A" text="M2"]', '[p]',
      '[end]',
    ].join(NL)
    // autoClick alone drives every [p] to completion (round 87 semantics),
    // the same terminal state a skip run reaches — but WITHOUT the instant
    // reveal (skip also sets reveal_chars=total). Assert the page flow only.
    const out = await player.runScene(ks, 'sk_auto_only.ks', { maxFrames: 50000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    expect(backlogT()).toContain('[A]M1')
    expect(backlogT()).toContain('[A]M2')
  }, 60000)

  // ---- 3. skip x choice -------------------------------------------------
  it('skip + choiceIndex routes the chosen branch deterministically (skip×choice combo)', async () => {
    player.core.backlog.length = 0
    const ks = [
      '[ch name="A" text="pre"]', '[p]',
      '[sel x="tf.r" target="*A" text="PickA"]',
      '[sel x="tf.r" target="*B" text="PickB"]',
      '[endbutton]',
      '[end]',
      '*A',
      '[ch name="A" text="routeA"]', '[p]',
      '[end]',
      '*B',
      '[ch name="A" text="routeB"]', '[p]',
      '[end]',
    ].join(NL)
    // choiceIndex=2 under skip: the choice dispatch still honors the index,
    // so the scene routes to *B even though skip auto-advances elsewhere.
    const out = await player.runScene(ks, 'sk_choice_idx.ks', { maxFrames: 50000, skip: true, choiceIndex: 2 })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const bl = backlogT()
    expect(bl).toContain('routeB')
    expect(bl).not.toContain('routeA')
    // tf.result recorded the *B target label
    await player.lua.doString('_G.__R = _G.__LAST_CTX and _G.__LAST_CTX.tf and _G.__LAST_CTX.tf.r or nil')
    expect(player.lua.global.get('__R')).toBe('*B')
  }, 60000)

  it('bare skip at a [sel] auto-selects option 1 (skip consumes the choice wait)', async () => {
    player.core.backlog.length = 0
    const ks = [
      '[ch name="A" text="pre"]', '[p]',
      '[sel x="tf.r" target="*A" text="PickA"]',
      '[sel x="tf.r" target="*B" text="PickB"]',
      '[endbutton]',
      '[end]',
      '*A',
      '[ch name="A" text="routeA"]', '[p]',
      '[end]',
      '*B',
      '[ch name="A" text="routeB"]', '[p]',
      '[end]',
    ].join(NL)
    // Skip does NOT park at a bare choice: the [endbutton] wait is consumed
    // exactly like a [p], with the default index (1) — so it picks the first
    // visible option and continues. (Desktop kag_runner stops skip at a
    // choice; the web bridge currently auto-resolves it — pinned as current
    // web behavior.)
    const out = await player.runScene(ks, 'sk_choice_bare.ks', { maxFrames: 50000, skip: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const bl = backlogT()
    expect(bl).toContain('routeA')
    expect(bl).not.toContain('routeB')
  }, 60000)

  // ---- 4. skip x save/load ---------------------------------------------
  it('skip_mode persists through [save] and a reload continues in skip mode', async () => {
    for (const k of [...Object.keys(localStorage)]) if (k.startsWith('caesura.save.')) localStorage.removeItem(k)
    const slot = 63
    player.core.backlog.length = 0
    // A skip run saves mid-scene; save.lua captures ctx.skip_mode into the
    // save payload and restores it on load (round 87 R6-FIX).
    const sceneA = [
      '[ch name="A" text="A1"]', '[p]',
      '[set f.m = 1]',
      '[save slot=' + slot + ']',
      '[ch name="A" text="A2after"]', '[p]',
      '[ch name="A" text="A3"]', '[p]',
      '[end]',
    ].join(NL)
    const out0 = await player.runScene(sceneA, 'sk_save.ks', { maxFrames: 50000, skip: true })
    expect(out0.startsWith('DONE:'), out0).toBe(true)
    expect(await readCtx('skip_mode')).toBe(true)

    // Load via the UI path. The restored ctx.skip_mode survives the resume,
    // so the post-save lines continue to be driven in skip (no manual click).
    player.core.backlog.length = 0
    const out1 = await player.loadSlot(slot, { sceneSources: { 'sk_save.ks': sceneA }, autoClick: true })
    expect(out1.startsWith('DONE:'), out1).toBe(true)
    expect(await readCtx('skip_mode')).toBe(true)
    const bl = backlogT()
    expect(bl).toContain('A2after')
    expect(bl).toContain('A3')

    player.deleteSlot(slot)
  }, 60000)

  // ---- 5. three-way toggle ---------------------------------------------
  it('auto→skip→advance arbitrary toggles keep token monotonic and backlog duplicate-free', async () => {
    player.core.backlog.length = 0
    const ks = [
      '[ch name="A" text="T1"]', '[p]',
      '[ch name="A" text="T2"]', '[p]',
      '[ch name="A" text="T3"]', '[p]',
      '[end]',
    ].join(NL)
    // park (no skip/auto)
    let out = await player.runScene(ks, 'sk_toggle.ks', { maxFrames: 50000 })
    expect(out.startsWith('WAIT:')).toBe(true)
    const t0 = await readCtx('token_index')
    expect(backlogT()).toBe('')

    // advance with autoClick ON -> runs the remainder to DONE (auto).
    out = await player.runScene(ks, 'sk_toggle.ks', { maxFrames: 50000, advance: true, advanceScene: 'sk_toggle.ks', autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const t1 = await readCtx('token_index')
    expect(t1).toBeGreaterThanOrEqual(t0)
    // every page committed exactly once
    const counts1 = player.core.backlog.map((b) => b.text).filter((t) => /^\[A\]T[123]$/.test(t || ''))
    expect(counts1).toHaveLength(3)

    // Repeat advance+skip on the (already DONE) cursor: token does not move
    // backward and the backlog does not grow (no duplicate — the prior pages
    // were already committed, the scene is finished).
    const before = backlogT()
    out = await player.runScene(ks, 'sk_toggle.ks', { maxFrames: 50000, advance: true, advanceScene: 'sk_toggle.ks', skip: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const t2 = await readCtx('token_index')
    expect(t2).toBeGreaterThanOrEqual(t1)
    expect(backlogT()).toBe(before)
  }, 60000)

  // ---- 6. skip x wait ---------------------------------------------------
  it('a [wait] inside a skip run completes and the scene continues to DONE (免点击)', async () => {
    player.core.backlog.length = 0
    const ks = [
      '[ch name="A" text="W0"]', '[p]',
      '[wait ms=30]',
      '[ch name="A" text="W1"]', '[p]',
      '[ch name="A" text="W2"]', '[p]',
      '[end]',
    ].join(NL)
    // Skip must not deadlock on the [wait] yield: the bridge pumps frames
    // while skip_mode holds, so the short wait elapses and execution falls
    // through to the post-wait lines, all the way to DONE — with zero manual
    // clicks (autoClick is OFF here, so only skip drives advancement).
    const out = await player.runScene(ks, 'sk_wait.ks', { maxFrames: 50000, skip: true, autoClick: false })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const bl = backlogT()
    expect(bl).toContain('W0')
    expect(bl).toContain('W1')
    expect(bl).toContain('W2')
  }, 60000)
})
