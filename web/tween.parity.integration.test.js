// @vitest-environment jsdom
// R106-A parity: the [tween] command family must behave identically under the
// web runner (wasmoon) as on desktop.
//
// The [tween] contract (scripts/kag/commands/tween.lua) advances in two modes:
//   * wait=true  (default, blocking)  — the handler drives the tween inline
//     through coroutine.yield(); the web frame loop resumes it with a real
//     per-frame dt (16 ms), exactly like [wait]/[move]/[trans]. NATURALLY
//     shared: zero bridge support needed.
//   * wait=false (fire-and-forget)    — the handler pushes the tween into
//     ctx.tweens and returns; desktop advances it from kag_runner.update
//     (-> TweenCommands.update(ctx, dt_ms)). Both Web entry points now use
//     that same runner hook. These tests lock parity for BOTH modes, plus easing-
//     function numeric correctness at a mid-tween sample.
//
// Because R106-A has not yet registered tween in kag.lua / regenerated
// scripts-index.json (shared coupling points), this suite mounts the REAL
// scripts/kag/commands/tween.lua source into the live VM and registers
// kag.tween — a test-local registration that exercises the actual shipped
// implementation without touching the repo registration files.
import { describe, it, expect, beforeAll } from 'vitest'
import { readFileSync, existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'
import { createPlayer } from './bridge.js'

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

// Mount the REAL tween module + register kag.tween (test-local, exercises the
// shipped implementation without touching kag.lua / scripts-index.json).
async function mountTween(player) {
  const src = readFileSync(join(rootDir, 'scripts', 'kag', 'commands', 'tween.lua'), 'utf8')
  player.lua.global.set('__TWEEN_SRC', src)
  await player.lua.doString([
    "local chunk = assert(load(_G.__TWEEN_SRC, '@kag/commands/tween.lua', 't', _ENV))",
    "package.preload['kag.commands.tween'] = function() return chunk() end",
    "local tw = require('kag.commands.tween')",
    "require('kag').tween = tw.tween",
    "_G.__TWEEN_MOUNTED = (type(tw) == 'table' and type(tw.tween) == 'function') and true or false",
  ].join(String.fromCharCode(10)))
  return await player.lua.global.get('__TWEEN_MOUNTED') === true
}

beforeAll(async () => {
  player = await createPlayer({
    scriptsBase: 'http://local/scripts/',
    fetchImpl: fileFetch,
    langBase: 'http://local/assets/lang/',
    wasmFile: join(here, 'node_modules', 'wasmoon', 'dist', 'glue.wasm'),
  })
  expect(await mountTween(player)).toBe(true)
})

describe('web [tween] parity (wasmoon + real tween.lua)', () => {
  it('blocking [tween wait=true] runs to completion and snaps the endpoint (R106)', async () => {
    player.core.events.length = 0
    const ks = [
      '[image layer="mover" storage="assets/bg/classroom.png" x=0 y=0]',
      '[tween target="mover" attr=x from=0 to=100 dur=1000]',
      '[ch name="N" text="after-tween"]',
      '[p]',
      '[end]',
    ].join('\n')
    const out = await player.runScene(ks, 'tw_block.ks', { maxFrames: 200000, autoClick: true })
    // 1000ms / 16ms ≈ 63 frames of blocking yields -> DONE, not ERR/frame-limit
    expect(out.startsWith('DONE:'), 'blocking tween should complete: ' + out).toBe(true)
    // no command errors surfaced
    const errs = player.core.events.filter((e) => String(e.kind).includes('error'))
    expect(errs, 'no error events for blocking tween').toEqual([])
    // endpoint snapped exactly
    const n = player.core.layers.get('mover')
    expect(n.x).toBe(100)
  }, 60000)

  it('ease_in decelerates relative to linear at a mid-tween sample (R106 easing math)', async () => {
    // Fire-and-forget tweens are advanced by the web bridge driver once per
    // frame (16 ms) in lockstep with the [wait] blocking clock. The exact
    // parked frame count is not a stable contract (it depends on the wait/
    // park frame accounting), so we assert the EASING SHAPE, not an absolute
    // value: at the same half-duration sample, ease_in(t)=t^2 must sit clearly
    // BEHIND linear(t)=t, and linear must be near the range midpoint while
    // ease_in stays far below it. Using the SAME scene/params for both makes
    const scene = (ease) => [
      '[image layer="mid" storage="assets/bg/classroom.png" x=0 y=0]',
      '[tween target="mid" attr=x from=0 to=100 dur=1000 ease=' + ease + ' wait=false]',
      '[wait ms=500]',
      '[p]',
      '[end]',
    ].join('\n')
    const outIn = await player.runScene(scene('ease_in'), 'tw_mid_in.ks', { maxFrames: 200000, autoClick: false })
    expect(outIn.startsWith('WAIT:'), outIn).toBe(true)
    const xIn = player.core.layers.get('mid').x

    const outLin = await player.runScene(scene('linear'), 'tw_mid_lin.ks', { maxFrames: 200000, autoClick: false })
    expect(outLin.startsWith('WAIT:'), outLin).toBe(true)
    const xLin = player.core.layers.get('mid').x

    // linear at ~half-duration should be near the 0..100 midpoint (frame-
    // quantized, so allow a wide band around 50); ease_in must sit well below
    // both the linear sample and 50 — the squared sweep is just getting going.
    expect(xLin).toBeGreaterThan(35)
    expect(xLin).toBeLessThan(65)
    expect(xIn).toBeLessThan(xLin)
    expect(xIn).toBeGreaterThan(15)
    expect(xIn).toBeLessThan(45)
  }, 60000)
  it('fire-and-forget [tween wait=false] advances to the exact endpoint (R106 driver)', async () => {
    const NL = String.fromCharCode(10)
    const ks = [
      '[image layer="ff" storage="assets/bg/classroom.png" x=0 y=0]',
      '[tween target="ff" attr=x from=0 to=100 dur=400 wait=false]',
      '[wait ms=700]',
      '[ch name="N" text="after-framedrive"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'tw_ff.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    // driver advanced the fire-and-forget tween past its 400ms dur during the
    // [wait ms=700] (700/16 ≈ 43 frames) -> endpoint snapped.
    expect(player.core.layers.get('ff').x).toBe(100)
  }, 60000)
})
