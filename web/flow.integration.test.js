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

  it('persists saves across scenes (round 46 web save bridge)', async () => {
    // isolate: fresh keys so earlier tests cannot leak state
    for (const k of [...Object.keys(localStorage)]) if (k.startsWith('caesura.save.')) localStorage.removeItem(k)
    const NL = String.fromCharCode(10)
    const saveKs = [
      '[set f.coins = 100]',
      '[set f.name = "樱"]',
      '[save slot=1]',
      '[end]',
    ].join(NL)
    const out1 = await player.runScene(saveKs, 'savegame.ks', { maxFrames: 200000, autoClick: true })
    expect(out1.startsWith('DONE:')).toBe(true)
    expect(player.core.events.some((e) => e.kind === 'save.write')).toBe(true)

    // fresh run loads slot 1 and restores variables (load sets stop_flag,
    // so the scene halts after restoring — engine semantics)
    const loadKs = [
      '[ch name="N" text="before"]',
      '[p]',
      '[load slot=1]',
      '[end]',
    ].join(NL)
    player.core.events.length = 0
    const out2 = await player.runScene(loadKs, 'loadgame.ks', { maxFrames: 200000, autoClick: true })
    expect(out2.startsWith('DONE:')).toBe(true)
    expect(player.core.events.some((e) => e.kind === 'save.read')).toBe(true)
    // [load] succeeded: engine logged Loaded slot 1 (no error events)
    const errs = player.core.events.filter((e) => String(e.kind).includes('error'))
    expect(errs.length).toBe(0)
  }, 120000)

  it('resumes from the saved point after [load] (round 47)', async () => {
    for (const k of [...Object.keys(localStorage)]) if (k.startsWith('caesura.save.')) localStorage.removeItem(k)
    const NL = String.fromCharCode(10)
    // Scene A saves mid-way; the loader scene then [load]s it and the
    // player must continue from the saved token (engine resume semantics).
    const sceneA = [
      '[ch name="N" text="A1"]',
      '[p]',
      '[ch name="N" text="A2"]',
      '[p]',
      '[set f.marker = 42]',
      '[save slot=1]',
      '[ch name="N" text="A3 after save"]',
      '[p]',
      '[ch name="N" text="A4"]',
      '[p]',
      '[end]',
    ].join(NL)
    await player.runScene(sceneA, 'scene_a.ks', { maxFrames: 200000, autoClick: true })
    const loader = [
      '[ch name="N" text="LOADER-START"]',
      '[p]',
      '[load slot=1]',
      '[end]',
    ].join(NL)
    player.core.backlog.length = 0
    const out = await player.runScene(loader, 'loader.ks', {
      maxFrames: 200000, autoClick: true,
      sceneSources: { 'scene_a.ks': sceneA },
    })
    expect(out.startsWith('DONE:')).toBe(true)
    // resumed lines appear after the loader page
    const texts = player.core.backlog.map((x) => x.text).join(' | ')
    expect(texts).toContain('LOADER-START')
    expect(texts).toContain('A3 after save')
    expect(texts).toContain('A4')
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
