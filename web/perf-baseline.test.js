// @vitest-environment jsdom
// Round 109: WEB player (wasmoon) large-scene performance baseline.
//
// Goal: establish a Web-player performance baseline for large scenes —
// frame throughput (scheduler ticks/ms), token throughput (tokens/ms),
// and Lua-heap growth under collectgarbage — so engine/script hot-path
// changes that would regress the browser player surface detectably.
//
// Measurement surface (jsdom-headless, wasmoon): bridge.js drives each
// scene as ONE synchronous lua.doString (the whole scene runs in a tight
// frame loop). Wall-clock around that await is the true scheduler time.
// bridge.js round-109 hook writes _G.__FRAME_COUNT under __PERF_TRACE so
// we can read the exact frame (tick) count — gated off on the normal path.
// Memory uses the round-101 technique: collectgarbage("collect") x3 then
// collectgarbage("count") (KB) before/after — reflects Lua-managed heap
// (tables + strings) inside wasmoon emscripten linear memory.
//
// Measured (probe): story.ks 2.75 frames/ms (2607 frames / 949ms),
// synthetic1000 5.87 frames/ms (4001 frames / 682ms). Budgets below are
// ~2x headroom over those readings (see doc round 109). Is a web test —
// not part of CI (same standing as story.bundle.sweep); run locally via
// `cd web && npx vitest run perf-baseline.test.js`.
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
    const rel = u.pathname.replace('/assets/lang/', '').replaceAll('/', String.fromCharCode(92))
    const p = join(assetsDir, 'lang', rel)
    return {
      ok: existsSync(p), status: existsSync(p) ? 200 : 404,
      text: async () => (existsSync(p) ? readFileSync(p, 'utf8') : ''),
      json: async () => index,
    }
  }
  const p = u.pathname.replace('/scripts/', scriptsDir + '/').replaceAll('/', String.fromCharCode(92))
  return { ...({ text: async () => readFileSync(p, 'utf8'), json: async () => index }), status: 200, ok: true }
}

function sourceFor(key) {
  for (const dir of ['../demo/', '../demo/tutorial/', '../demo/example_game/']) {
    const p = join(here, dir, key)
    if (existsSync(p)) return readFileSync(p, 'utf8')
  }
  return null
}

function makeSynthetic(lines) {
  let ks = ''
  for (let i = 0; i < lines; i++) {
    ks += '[ch name="Nar' + (i % 4) + '"]synthetic stress line ' + i + ' of the large scene\n[p]\n'
  }
  return ks
}

async function heapKB(player) {
  const s = await player.lua.doString(
    'collectgarbage("collect"); collectgarbage("collect"); collectgarbage("collect"); return tostring(collectgarbage("count"))'
  )
  return parseFloat(String(s))
}

async function benchmarkRun(player, src, name) {
  player.core.events.length = 0
  player.core.backlog.length = 0
  player.lua.global.set('__PERF_TRACE', true)
  player.lua.global.set('__FRAME_COUNT', 0)
  const memBefore = await heapKB(player)
  const t0 = performance.now()
  const out = await player.runScene(src, name, { maxFrames: 1000000, autoClick: true })
  const t1 = performance.now()
  const frames = Number((await player.lua.global.get('__FRAME_COUNT')) || 0)
  const memAfter = await heapKB(player)
  const wall = t1 - t0
  const m = /^DONE:(\d+):(\d+)$/.exec(String(out))
  return {
    out: String(out),
    wallMs: wall,
    frames,
    framesPerMs: frames / wall,
    tokens: m ? Number(m[1]) : NaN,
    tokensPerMs: m ? Number(m[1]) / wall : NaN,
    memGrowthKB: memAfter - memBefore,
  }
}

describe('web player performance baseline (round 109)', () => {
  let player = null
  beforeAll(async () => {
    player = await createPlayer({
      scriptsBase: 'http://local/scripts/',
      fetchImpl: fileFetch,
      langBase: 'http://local/assets/lang/',
      wasmFile: join(here, 'node_modules', 'wasmoon', 'dist', 'glue.wasm'),
    })
  }, 60000)

  it('story.ks main path: frame throughput + completes clean', async () => {
    const src = sourceFor('story.ks')
    expect(src, 'demo/example_game/story.ks should exist').toBeTruthy()
    const r = await benchmarkRun(player, src, 'story.ks')
    expect(r.out.startsWith('DONE:'), 'story.ks should complete via runScene: ' + r.out).toBe(true)
    expect(r.frames, '__FRAME_COUNT should be reported by the round-109 hook').toBeGreaterThan(100)
    expect(r.framesPerMs, 'story.ks frame throughput >= 0.8 frames/ms').toBeGreaterThan(0.8)
    expect(r.tokensPerMs, 'story.ks token throughput >= 0.08 tokens/ms').toBeGreaterThan(0.08)
    const errs = player.core.events.filter((e) => String(e.kind).includes('error'))
    expect(errs, 'story.ks main run should have no error events').toEqual([])
  }, 120000)

  it('story.ks Lua heap growth stays bounded (< 1024 KB)', async () => {
    const r = await benchmarkRun(player, sourceFor('story.ks'), 'story.ks-mem')
    expect(r.memGrowthKB, 'story.ks heap growth < 1024 KB (got ' + r.memGrowthKB.toFixed(1) + ' KB)').toBeLessThan(1024)
  }, 120000)

  it('synthetic 1000-line scene: frame throughput + correctness (3000 tokens)', async () => {
    const r = await benchmarkRun(player, makeSynthetic(1000), 'synthetic1000.ks')
    expect(r.out, 'synthetic 1000-line scene should complete: ' + r.out).toMatch(/^DONE:/)
    expect(r.tokens, '1000 ch+p lines produce 3000 tokens').toBe(3000)
    expect(r.framesPerMs, 'synthetic 1000-line frame throughput >= 2.5 frames/ms').toBeGreaterThan(2.5)
    expect(r.tokensPerMs, 'synthetic 1000-line token throughput >= 2.0 tokens/ms').toBeGreaterThan(2.0)
  }, 120000)

  it('synthetic 1000-line Lua heap growth stays bounded (< 2048 KB)', async () => {
    const r = await benchmarkRun(player, makeSynthetic(1000), 'synthetic1000-mem.ks')
    expect(r.memGrowthKB, 'synthetic 1000-line heap growth < 2048 KB (got ' + r.memGrowthKB.toFixed(1) + ' KB)').toBeLessThan(2048)
  }, 120000)

  it('2000-line scene runs within 2.5x of the 1000-line scene (linear-ish)', async () => {
    const r1 = await benchmarkRun(player, makeSynthetic(1000), 'synth1000-scale.ks')
    const r2 = await benchmarkRun(player, makeSynthetic(2000), 'synth2000-scale.ks')
    expect(r2.out, '2000-line scene should complete: ' + r2.out).toMatch(/^DONE:/)
    expect(r2.tokens, '2000 ch+p lines produce 6000 tokens').toBe(6000)
    expect(r2.wallMs, 'doubling scene size should not blow up (wall2000 < 2.5x wall1000)').toBeLessThan(r1.wallMs * 2.5)
  }, 120000)

  it('wasmoon limitation observability: heap window + single-thread coroutines', async () => {
    const hk = await heapKB(player)
    expect(hk, 'collectgarbage("count") should report Lua heap in KB').toBeGreaterThan(0)
    const syncProbe = await player.lua.doString('return tostring(coroutine.running())')
    expect(syncProbe, 'scene frame loop drives coroutines cooperatively on the main thread').toBeTruthy()
  }, 60000)
})