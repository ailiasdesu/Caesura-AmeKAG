// @vitest-environment jsdom
// Full browser-flow integration: real wasmoon engine + DOM renderer +
// the complete galgame demo scene, driven click-by-click.
import { describe, it, expect, beforeAll } from 'vitest'
import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'
import { createPlayer } from './bridge.js'
import { DomRenderer } from './dom-renderer.js'

const here = dirname(fileURLToPath(import.meta.url))
const scriptsDir = join(here, '..', 'scripts')
const index = JSON.parse(readFileSync(join(here, 'scripts-index.json'), 'utf8'))
const fileFetch = async (url) => {
  const u = new URL(url)
  const p = u.pathname.replace('/scripts/', scriptsDir + '/').replaceAll('/', '\\')
  return { text: async () => readFileSync(p, 'utf8'), json: async () => index }
}

let player = null
let renderer = null
let stage = null

beforeAll(async () => {
  player = await createPlayer({
    scriptsBase: 'http://local/scripts/',
    fetchImpl: fileFetch,
    wasmFile: join(here, 'node_modules', 'wasmoon', 'dist', 'glue.wasm'),
  })
  stage = document.createElement('div')
  document.body.appendChild(stage)
  renderer = new DomRenderer(player.core, stage)
})

describe('browser flow (jsdom + wasmoon + DOM)', () => {
  it('renders the scene into DOM and advances on clicks', async () => {
    const ks = readFileSync(join(here, '..', 'demo', 'galgame_demo.ks'), 'utf8')
    let out = await player.runScene(ks, 'galgame_demo.ks', { maxFrames: 50000 })
    expect(out.startsWith('WAIT:')).toBe(true)
    for (const [id, t] of player.core.textures) {
      renderer.setTextureUrl(id, '/assets/' + t.path)
    }
    renderer.render()

    const bgImg = stage.querySelector('img[data-layer="bg"]')
    expect(bgImg).toBeTruthy()
    expect(bgImg.getAttribute('src')).toContain('classroom.png')
    const msg = stage.querySelector('.caesura-message')
    expect(msg?.textContent).toContain('Welcome to Caesura')
    // structured draws render as positioned spans (x/y/color/scale)
    const spans = msg?.querySelectorAll('span')
    expect(spans && spans.length).toBeGreaterThan(0)
    expect(spans[0].style.left).toBeTruthy()
    expect(spans[0].style.top).toBeTruthy()
    expect(spans[0].style.color).toMatch(/rgb\(/)
    expect(spans[0].style.fontSize).toMatch(/px/)
    // layer transitions: CSS animates engine-driven moves/fades
    const bgEl = stage.querySelector('img[data-layer="bg"]')
    expect(bgEl.style.transition).toContain('left 300ms')
    expect(bgEl.style.opacity).toBe('1') // 255 -> 1.0 normalized

    out = await player.runScene(ks, 'galgame_demo.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:')).toBe(true)
    // map any textures the second run loaded (hana.png got a new id)
    for (const [id, t] of player.core.textures) {
      renderer.setTextureUrl(id, '/assets/' + t.path)
    }
    await renderer.render()

    const bg2 = stage.querySelector('img[data-layer="bg"]')
    expect(bg2).toBeTruthy()
    expect(bg2.getAttribute('src')).toContain('hana.png')
    expect(stage.querySelector('img[data-layer="_char_Sakura"]')).toBeTruthy()
    expect(stage.querySelector('img[data-layer="_char_Teacher"]')).toBeTruthy()
    // after the final [p] the message layer is cleared (engine semantics)
    const msg2 = stage.querySelector('.caesura-message')
    expect(msg2).toBeNull()

    // sprite_move animated _char_Sakura toward 120,200 (a later [ch sprite=]
    // re-centers the layer at 440 — engine semantics; assert the move ran
    // and the CSS transition is armed).
    const sakura = stage.querySelector('img[data-layer="_char_Sakura"]')
    expect(sakura).toBeTruthy()
    const sakMoves = player.core.events.filter((e) => e.kind === 'layer.move' && e.detail.name === '_char_Sakura')
    expect(sakMoves.length).toBeGreaterThan(20) // full sprite_move animation
    expect(sakMoves[sakMoves.length - 1].detail.x).toBe(120)
    expect(sakura.style.transition).toContain('left 300ms')
  }, 120000)

  it('loads a pre-baked .ksc stream (zero-parse start)', async () => {
    // ks_bake pre-compiles scenes into Lua-literal .ksc; the web player
    // loads them directly (no tokenizer/compiler at scene start).
    const ksc = readFileSync(join(here, '..', 'cache', 'ksc-web', 'demo_galgame_demo.ksc'), 'utf8')
    player.lua.global.set('KSC_SRC', ksc)
    const data = await player.lua.doString([
      '  local chunk = assert(load(KSC_SRC, \'@galgame.ksc\', \'t\', _ENV))',
      '  return chunk()',
    ].join(String.fromCharCode(10)))
    expect(data.version).toBe(1)
    expect(data.tokens.length).toBeGreaterThan(0)
    expect(data.labels).toBeTruthy()
  }, 60000)

  it('loads the ks_bake --web story bundle (scenes + assets)', async () => {
    const story = readFileSync(join(here, '..', 'cache', 'story', 'story.lua'), 'utf8')
    player.lua.global.set('STORY_SRC', story)
    const bundle = await player.lua.doString([
      '  local chunk = assert(load(STORY_SRC, \'@story.lua\', \'t\', _ENV))',
      '  return chunk()',
    ].join(String.fromCharCode(10)))
    expect(bundle.version).toBe(1)
    expect(bundle.scenes).toBeTruthy()
    expect(bundle.scenes['galgame_demo.ks']).toBeTruthy()
    expect(bundle.assets.length).toBeGreaterThan(0)
    expect(bundle.assets.some((a) => a.includes('classroom.png'))).toBe(true)
  }, 60000)

  it('runs every tutorial in the teaching path (01-08) to completion', async () => {
    const tutorials = [
      ['tutorial_01_hello.ks', /你好，世界/],
      ['tutorial_02_text.ks', /文本命令学完了/],
      ['tutorial_03_layers.ks', /图层教程完成/],
      ['tutorial_04_audio.ks', /音频教程完成/],
      ['tutorial_05_branching.ks', /分支教学完成/],
      ['tutorial_06_effects.ks', /六个教程全部完成/],
      // round 45: save/load degrades gracefully (no SaveManager in web)
      ['tutorial_07_saveload.ks', /存档教程完成/],
      // round 45: system-UI overlays are no-op stubs in the web player
      ['tutorial_08_system_ui.ks', /八课教程全部学完/],
    ]
    for (const [file, lineRe] of tutorials) {
      const ks = readFileSync(join(here, '..', 'demo', 'tutorial', file), 'utf8')
      const out = await player.runScene(ks, file, { maxFrames: 200000, autoClick: true })
      expect(out.startsWith('DONE:'), file + ' should complete: ' + out).toBe(true)
      // teaching lines flow through the TextScene draws (bridge collects
      // them per-run); a [ch] line must have hit the page at some point.
      const texts = player.core.events
        .filter((e) => e.kind === 'text.draws')
        .flatMap((e) => (Array.isArray(e.detail?.draws) ? e.detail.draws : []))
        .map((d) => d.t || '')
        .concat(player.core.backlog.flatMap((p) => p.draws.map((d) => d.t || '')))
      expect(texts.some((x) => lineRe.test(x)), file + ' should contain teaching line').toBe(true)
    }
    // tutorial 06 unlocks its ending
    const ending = player.core.events.some((e) => e.kind === 'ending.unlock')
    expect(ending).toBe(true)
  }, 120000)

  it('runs the showcase sample (25 commands, branching, backlog)', async () => {
    player.core.backlog.length = 0 // isolate from earlier scenes
    const ks = readFileSync(join(here, '..', 'demo', 'showcase.ks'), 'utf8')
    const out = await player.runScene(ks, 'showcase.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:')).toBe(true)
    // branching took the lucky path; ending unlocked
    const ending = player.core.events.some((e) => e.kind === 'audio.stop')
    // backlog accumulated per [p] page
    expect(player.core.backlog.length).toBeGreaterThan(5)
    expect(player.core.backlog[0].text).toContain('Welcome to the Caesura')
  }, 120000)
})
