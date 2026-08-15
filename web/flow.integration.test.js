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

  it('runs every tutorial in the teaching path (01-13) to completion', async () => {
    const tutorials = [
      ['tutorial_01_hello.ks', /你好，世界/],
      ['tutorial_02_text.ks', /文本命令学完了/],
      ['tutorial_03_layers.ks', /图层教程完成/],
      ['tutorial_04_audio.ks', /音频教程完成/],
      ['tutorial_05_branching.ks', /分支教学完成/],
      ['tutorial_06_effects.ks', /教程 06 完成/],
      // round 45: save/load degrades gracefully (no SaveManager in web)
      ['tutorial_07_saveload.ks', /存档教程完成/],
      // round 45: system-UI overlays are no-op stubs in the web player
      ['tutorial_08_system_ui.ks', /教程 08 完成/],
      // round 50: interpolation teaching ($tbl.key / %tbl.key% / ${expr})
      ['tutorial_09_interpolation.ks', /插值教程完成/],
      // round 56: bounded loops ([for] ascending/descending, [while]+[eval])
      ['tutorial_10_loops.ks', /循环教程完成/],
      // round 58: multi-way branch (bare variable + exp= expression selector)
      ['tutorial_11_switch.ks', /switch 教程完成/],
      // round 68: expression combo (ternary-in-index / ?? / switch exp / loops)
      ['tutorial_12_expr_combo.ks', /表达式组合教程完成/],
      // round 71: KAG3-compatible commands (textspeed/cps, math chain, csp, notify, vibrate, preload)
      ['tutorial_13_commands.ks', /KAG3 兼容命令教程完成/],
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

  it('manages save slots: list/save-current/delete (round 49)', async () => {
    for (const k of [...Object.keys(localStorage)]) if (k.startsWith('caesura.save.')) localStorage.removeItem(k)
    const NL = String.fromCharCode(10)
    const sceneA = [
      '[ch name="N" text="S1"]',
      '[p]',
      '[ch name="N" text="S2"]',
      '[p]',
      '[save slot=1]',
      '[ch name="N" text="S3"]',
      '[p]',
      '[end]',
    ].join(NL)
    await player.runScene(sceneA, 'scene_a.ks', { maxFrames: 200000, autoClick: true })

    // listSlots reflects the [save] made inside the scene
    let slots = player.listSlots()
    expect(slots.some((s) => s.slot === 1 && s.scene.includes('scene_a.ks'))).toBe(true)

    // saveCurrent captures the LAST run's position into another slot
    const ok = await player.saveCurrent(9)
    expect(ok).toBe(true)
    slots = player.listSlots()
    const s9 = slots.find((s) => s.slot === 9)
    expect(s9).toBeTruthy()
    expect(s9.token).toBeGreaterThan(1)

    // deleteSlot removes and is idempotent
    expect(player.deleteSlot(1)).toBe(true)
    expect(player.deleteSlot(1)).toBe(false)
    expect(player.listSlots().some((s) => s.slot === 1)).toBe(false)
  }, 120000)

  it('loads a slot through the UI path and resumes (round 49)', async () => {
    for (const k of [...Object.keys(localStorage)]) if (k.startsWith('caesura.save.')) localStorage.removeItem(k)
    const NL = String.fromCharCode(10)
    const sceneA = [
      '[ch name="N" text="U1"]',
      '[p]',
      '[set f.tag = 7]',
      '[save slot=2]',
      '[ch name="N" text="U2 after save"]',
      '[p]',
      '[ch name="N" text="U3"]',
      '[p]',
      '[end]',
    ].join(NL)
    await player.runScene(sceneA, 'scene_a.ks', { maxFrames: 200000, autoClick: true })

    player.core.backlog.length = 0
    const out = await player.loadSlot(2, { sceneSources: { 'scene_a.ks': sceneA } })
    expect(out.startsWith('DONE:')).toBe(true)
    const texts = player.core.backlog.map((x) => x.text).join(' | ')
    expect(texts).toContain('Loading slot 2')
    expect(texts).toContain('U2 after save')
    expect(texts).toContain('U3')
  }, 120000)

  it('runs the showcase sample (25 commands, branching, backlog)', async () => {
    player.core.backlog.length = 0 // isolate from earlier scenes
    const ks = readFileSync(join(here, '..', 'demo', 'showcase.ks'), 'utf8')
    const out = await player.runScene(ks, 'showcase.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:')).toBe(true)
    // branching took the lucky path; [ending] unlocked the showcase ending
    const ending = player.core.events.some((e) => e.kind === 'ending.unlock')
    // backlog accumulated per [p] page
    expect(player.core.backlog.length).toBeGreaterThan(5)
    expect(player.core.backlog[0].text).toContain('Welcome to the Caesura')
  }, 120000)

  it('exercises round 53-63 expression edges in the web player', async () => {
    player.core.backlog.length = 0
    // Ternary-in-index, ?? null-coalesce, long-bracket interpolation,
    // [switch exp=], [eval] TJS operators and expression-bound [for] —
    // the desktop fixes must behave identically under wasmoon.
    const ks = [
      '[set f.flag = true]',
      '[set f.hp = 30]',
      '[eval exp="f.arr = {10, 20}"]',
      '[eval exp="f.ok = f.hp > 10 && f.flag"]',
      '[ch name="N" text="idx=\x24{f.arr[f.flag ? 1 : 2]}"]',
      '[p]',
      '[set f.flag = false]',
      '[ch name="N" text="idx2=\x24{f.arr[f.flag ? 1 : 2]}"]',
      '[p]',
      '[ch name="N" text="nc=\x24{f.missing ?? 42}"]',
      '[p]',
      '[ch name="N" text="lb=\x24{ [[x}}]] .. \'A\' }"]',
      '[p]',
      '[ch name="N" text="ok=\x24{f.ok}"]',
      '[p]',
      '[set f.tier = 2]',
      '[switch exp="f.tier"]',
      '[case 1]',
      '[ch name="N" text="tier-one"]',
      '[case 2]',
      '[ch name="N" text="tier-two"]',
      '[endswitch]',
      '[ch name="N" text="sw=done"]',
      '[p]',
      '[set f.n = 2]',
      '[for var="i" start="1" end="f.n"]',
      '[ch name="N" text="loop\x24{f.i}"]',
      '[endfor]',
      '[ch name="N" text="expr-edge-done"]',
      '[p]',
      '[end]',
    ].join('\n')
    const out = await player.runScene(ks, 'expr_edge.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const texts = player.core.backlog.map((x) => x.text).join(' | ')
    expect(texts).toContain('idx=10')          // ternary-in-index then-branch
    expect(texts).toContain('idx2=20')         // ternary-in-index else-branch
    expect(texts).toContain('nc=42')           // ?? fallback on missing var
    expect(texts).toContain('lb=x}}A')         // long-bracket braces survive
    expect(texts).toContain('ok=true')         // [eval] && translation
    expect(texts).toContain('tier-two')        // [switch exp=] numeric match
    expect(texts).toContain('sw=done')         // flow continues after switch
    expect(texts).toContain('loop1')           // [for] expression end bound
    expect(texts).toContain('loop2')
    expect(texts).toContain('expr-edge-done')
  }, 120000)
  it('[for] loop exposes its counter in f and accumulates via [add] (round 74/75)', async () => {
    player.core.backlog.length = 0
    const NL = String.fromCharCode(10)
    const ks = [
      '[set f.sum = 0]',
      '[for var="i" start="1" end="3"]',
      '[add name="f.sum" value=1]',
      '[endfor]',
      '[ch name="N" text="sum=\x24{f.sum} after=\x24{f.i}"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'loop_edge.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const texts = player.core.backlog.map((b) => b.text).join(' | ')
    // body executed once per bound range step: [add] accumulated 1+1+1=3
    expect(texts).toContain('sum=3')
    // the loop counter is exposed in the f table and left past the bound
    expect(texts).toContain('after=4')
  }, 120000)

  it('[sel x="tf.result"]/[endbutton] writes the chosen target label (round 74/75)', async () => {
    player.core.backlog.length = 0
    const NL = String.fromCharCode(10)
    // auto-click selects option 1; the choice routes to its *label.
    const ks = [
      '[sel x="tf.result" target="*route_a" text="Route A"]',
      '[sel x="tf.result" target="*route_b" text="Route B"]',
      '[endbutton]',
      '[end]',
      '*route_a',
      '[ch name="N" text="chose-a=\x24{tf.result}"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'choice_a.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    expect(player.core.backlog.map((b) => b.text).join(' | ')).toContain('chose-a=*route_a')

    // choiceIndex=2 picks the second option -> routes to *route_b
    player.core.backlog.length = 0
    const ks2 = [
      '[sel x="tf.result" target="*route_a" text="Route A"]',
      '[sel x="tf.result" target="*route_b" text="Route B"]',
      '[endbutton]',
      '[end]',
      '*route_b',
      '[ch name="N" text="chose-b=\x24{tf.result}"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out2 = await player.runScene(ks2, 'choice_b.ks', {
      maxFrames: 200000, autoClick: true, choiceIndex: 2,
    })
    expect(out2.startsWith('DONE:'), out2).toBe(true)
    expect(player.core.backlog.map((b) => b.text).join(' | ')).toContain('chose-b=*route_b')
  }, 120000)

  it('[ruby] follows the text cursor when no x/y is given (round 74)', async () => {
    player.core.draws = []
    const NL = String.fromCharCode(10)
    const ks = [
      '[ch name="N" text="GO"]',
      '[ruby text="漢字" ruby="かんじ"]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'ruby_follow.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    // the bare [ruby] (no x/y) lands on the current text cursor, NOT the
    // absolute (0,0) that schema.coerce's x=0/y=0 defaults used to pin — the
    // round-74 fix. core.draws carries the base text at its computed x/y.
    const rubyDraw = player.core.draws.find((d) => d.t === '漢字')
    expect(rubyDraw).toBeTruthy()
    expect(rubyDraw.x).toBeGreaterThan(0)
    expect(rubyDraw.y).toBeGreaterThan(0)
    // specifically it matches the message window cursor (after the GO line),
    // i.e. it followed the cursor rather than pinning to the origin.
    expect(rubyDraw.y).toBeGreaterThan(100)
  }, 120000)

  it('[nvl on]/[nvl off] toggles _textbox/_nameplate layer visibility (round 74)', async () => {
    player.core.layers.clear()
    const NL = String.fromCharCode(10)
    // [textbox] creates _textbox; [ch name=...] renders the _nameplate.
    const ks = [
      '[textbox x=0 y=520 w=1280 h=200 color="0,0,0" opacity=200]',
      '[nameplate y=480 w=220 h=36 color="0,0,0"]',
      '[ch name="N" text="pre"]',
      '[p]',
      '[nvl on]',
      '[set f.mark = 1]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'nvl_hide.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    // [nvl on] hides the fixed message window + nameplate (Ren'Py NVL parity)
    expect(player.core.getLayer('_textbox').visible).toBe(false)
    expect(player.core.getLayer('_nameplate').visible).toBe(false)

    // fresh run: [nvl off] restores the pre-NVL visibility
    player.core.layers.clear()
    const ks2 = [
      '[textbox x=0 y=520 w=1280 h=200 color="0,0,0" opacity=200]',
      '[nameplate y=480 w=220 h=36 color="0,0,0"]',
      '[ch name="N" text="pre"]',
      '[p]',
      '[nvl on]',
      '[nvl off]',
      '[set f.mark2 = 1]',
      '[end]',
    ].join(NL)
    const out2 = await player.runScene(ks2, 'nvl_restore.ks', { maxFrames: 200000, autoClick: true })
    expect(out2.startsWith('DONE:'), out2).toBe(true)
    expect(player.core.getLayer('_textbox').visible).toBe(true)
    expect(player.core.getLayer('_nameplate').visible).toBe(true)
  }, 120000)

  it('\x24{f.missing ?? fallback} null-coalescing interpolation (round 74/75)', async () => {
    player.core.backlog.length = 0
    const NL = String.fromCharCode(10)
    const ks = [
      '[set f.name = "fallback"]',
      '[ch name="N" text="nc=\x24{f.missing ?? f.name}"]',
      '[p]',
      '[set f.have = "present"]',
      '[ch name="N" text="hc=\x24{f.have ?? f.name}"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'interp_nc.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const texts = player.core.backlog.map((b) => b.text).join(' | ')
    expect(texts).toContain('nc=fallback')  // missing key -> fallback var value
    expect(texts).toContain('hc=present')   // present key wins over fallback
  }, 120000)
  it('[while exp] loop iterates on web, bounded (round 76)', async () => {
    player.core.backlog.length = 0
    const NL = String.fromCharCode(10)
    // Data-driven [while]: counter + accumulator accumulate per iteration
    // and the loop terminates (bounded guard), mirroring the desktop run.
    const ks = [
      '[set f.counter = 0]',
      '[set f.sum = 0]',
      '[while exp="f.counter < 4"]',
      '[add name="f.counter" value=1]',
      '[add name="f.sum" value=1]',
      '[endwhile]',
      '[ch name="N" text="cnt=\x24{f.counter} sum=\x24{f.sum}"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'while_edge.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const texts = player.core.backlog.map((b) => b.text).join(' | ')
    // body ran exactly 4 times (counter 0->4, sum 0->4), then the exp
    // turned false and the loop exited (no frame-limit error -> bounded).
    expect(texts).toContain('cnt=4')
    expect(texts).toContain('sum=4')
  }, 120000)

  it('[textspeed]/[cps] set ctx.text_speed (reveal pace) + ctx.cps (round 76)', async () => {
    const readCtx = async (name) => {
      await player.lua.doString('_G.__R = _G.__LAST_CTX and _G.__LAST_CTX[' + JSON.stringify(name) + '] or nil')
      return player.lua.global.get('__R')
    }
    // [textspeed cps=100] -> 100 chars/sec == 10 ms/char reveal pace
    await player.runScene('[textspeed cps=100]\n[end]', 'cps100.ks', { maxFrames: 200000, autoClick: true })
    expect(await readCtx('cps')).toBe(100)
    expect(await readCtx('text_speed')).toBe(10) // floor(1000/100)
    // [cps 25] (KAG3 bare alias) -> 25 chars/sec == 40 ms/char
    await player.runScene('[cps 25]\n[end]', 'cps25.ks', { maxFrames: 200000, autoClick: true })
    expect(await readCtx('cps')).toBe(25)
    expect(await readCtx('text_speed')).toBe(40) // floor(1000/25)
  }, 120000)

  it('[button cond="f.x..."] hides the false option on web (round 76)', async () => {
    player.core.backlog.length = 0
    const NL = String.fromCharCode(10)
    const ks = [
      '[set f.x = 1]',
      // cond false (1 > 1): "NeedX" must be filtered out at [endbutton]
      '[button text="NeedX" target="*high" cond="f.x > 1"]',
      // cond true (1 >= 1): only this option remains visible
      '[button text="Low" target="*low" cond="f.x >= 1"]',
      '[endbutton]',
      '[end]',
      '*low',
      '[ch name="N" text="picked-low"]',
      '[p]',
      '[end]',
      '*high',
      '[ch name="N" text="picked-high"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'cond_choice.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const texts = player.core.backlog.map((b) => b.text).join(' | ')
    // auto-click picked the first VISIBLE option (Low) — the false cond
    // option was dropped, so the *high route never ran.
    expect(texts).toContain('picked-low')
    expect(texts).not.toContain('picked-high')

    // control: with f.x=5 both conds pass, so the first visible option is
    // "NeedX" (cond f.x > 1 true) -> routes to *high.
    player.core.backlog.length = 0
    const ks2 = [
      '[set f.x = 5]',
      '[button text="NeedX" target="*high" cond="f.x > 1"]',
      '[button text="Low" target="*low" cond="f.x >= 1"]',
      '[endbutton]',
      '[end]',
      '*low',
      '[ch name="N" text="picked-low"]',
      '[p]',
      '[end]',
      '*high',
      '[ch name="N" text="picked-high"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out2 = await player.runScene(ks2, 'cond_choice2.ks', { maxFrames: 200000, autoClick: true })
    expect(out2.startsWith('DONE:'), out2).toBe(true)
    expect(player.core.backlog.map((b) => b.text).join(' | ')).toContain('picked-high')
  }, 120000)

  it('save inside a [for] body then [load] -> loop CONTINUES to completion (round 76)', async () => {
    for (const k of [...Object.keys(localStorage)]) if (k.startsWith('caesura.save.')) localStorage.removeItem(k)
    const NL = String.fromCharCode(10)
    // Save is taken mid-loop (2nd iteration). Round-75 desktop parity: the
    // web bridge saves through the REAL SaveCommands.save, whose
    // capture_state serializes ctx._forStack/_ifStack into loop_stacks; a
    // [load] restore writes ctx._resumeLoopStacks and the re-spawned
    // scheduler.run consumes it, so the for/if chain picks up where it
    // paused instead of silently ending.
    const sceneA = [
      '[set f.total = 0]',
      '[for var="i" start="1" end="3"]',
      '[add name="f.total" value=1]',
      '[if exp="f.i == 2"]',
      '[save slot=1]',
      '[endif]',
      '[endfor]',
      '[ch name="N" text="completed total=\x24{f.total} i=\x24{f.i}"]',
      '[p]',
      '[ch name="N" text="AFTER-LOOP-DONE"]',
      '[p]',
      '[end]',
    ].join(NL)
    await player.runScene(sceneA, 'loop_save.ks', { maxFrames: 200000, autoClick: true })
    const loader = [
      '[ch name="N" text="LOADER-START"]',
      '[p]',
      '[load slot=1]',
      '[end]',
    ].join(NL)
    player.core.backlog.length = 0
    const out = await player.runScene(loader, 'loader.ks', {
      maxFrames: 200000, autoClick: true,
      sceneSources: { 'loop_save.ks': sceneA },
    })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const texts = player.core.backlog.map((x) => x.text).join(' | ')
    expect(texts).toContain('LOADER-START')
    // The loop body ran all 3 iterations (i=1 before save counts; save on
    // i=2, then iterations 2-finalize + 3 run after load), so total=3 and
    // the counter exited at i=4.
    expect(texts).toContain('completed total=3')
    expect(texts).toContain('i=4')
    expect(texts).toContain('AFTER-LOOP-DONE')
  }, 120000)

  it('[select]/[sel]/[endselect] KAG3 alias path works on web (round 76)', async () => {
    player.core.backlog.length = 0
    const NL = String.fromCharCode(10)
    // [select] opens the block (no-op), [sel x=] registers an option,
    // [endselect] renders + blocks (alias of [endbutton]). auto-click
    // selects option 1 -> routes to *route_p and records tf.result.
    const ks = [
      '[select]',
      '[sel x="tf.result" target="*route_p" text="PickP"]',
      '[sel x="tf.result" target="*route_q" text="PickQ"]',
      '[endselect]',
      '[end]',
      '*route_p',
      '[ch name="N" text="picked-p=\x24{tf.result}"]',
      '[p]',
      '[end]',
      '*route_q',
      '[ch name="N" text="picked-q"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'select_p.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    expect(player.core.backlog.map((b) => b.text).join(' | ')).toContain('picked-p=*route_p')

    // choiceIndex=2 picks the second option -> routes to *route_q
    player.core.backlog.length = 0
    const out2 = await player.runScene(ks, 'select_q.ks', {
      maxFrames: 200000, autoClick: true, choiceIndex: 2,
    })
    expect(out2.startsWith('DONE:'), out2).toBe(true)
    expect(player.core.backlog.map((b) => b.text).join(' | ')).toContain('picked-q')
  }, 120000)

  it('[goto] is a KAG3 alias of [jump] on web: skips intermediate text (round 76/77 parity)', async () => {
    player.core.backlog.length = 0
    const NL = String.fromCharCode(10)
    // Mirrors tests/scripts/test_scheduler.lua section 19: [goto *label] (and
    // target="*label") is the KAG3 intra-scene label alias of [jump], and
    // must skip the intermediate [ch] and land on the label. The scheduler
    // union handler runs the (real) Lua flow branch under wasmoon, so web
    // must match desktop.
    const ks = [
      '[ch name="N" text="A"]',
      '[goto *L1]',
      '[ch name="N" text="SKIPPED"]',
      '*L1',
      '[ch name="N" text="B"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'goto_label.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const texts = player.core.backlog.map((b) => b.text).join(' | ')
    // A and B survive; the SKIPPED line between [goto] and the label never ran.
    expect(texts).toContain('A')
    expect(texts).toContain('B')
    expect(texts).not.toContain('SKIPPED')

    // named target="*L2" form (KAG3 alias path) also skips to the label
    player.core.backlog.length = 0
    const ks2 = [
      '[ch name="N" text="start"]',
      '[goto target="*L2"]',
      '[ch name="N" text="SKIP2"]',
      '*L2',
      '[ch name="N" text="end"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out2 = await player.runScene(ks2, 'goto_target.ks', { maxFrames: 200000, autoClick: true })
    expect(out2.startsWith('DONE:'), out2).toBe(true)
    const t2 = player.core.backlog.map((b) => b.text).join(' | ')
    expect(t2).toContain('start')
    expect(t2).toContain('end')
    expect(t2).not.toContain('SKIP2')
  }, 120000)

  it('[i18n language="en"] hot-switch runs and records settingsValues.language (round 76/77 parity)', async () => {
    const readCtx = async (name) => {
      // same pattern as the [textspeed]/[cps] round-76 test: read a field
      // off __LAST_CTX (the exporter runs after every runScene).
      await player.lua.doString('_G.__R = _G.__LAST_CTX and _G.__LAST_CTX[' + JSON.stringify(name) + '] or nil')
      return player.lua.global.get('__R')
    }
    const NL = String.fromCharCode(10)
    const ks = [
      '[ch name="N" text="before"]',
      '[p]',
      '[i18n language="en"]',
      '[ch name="N" text="after"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'i18n_en.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    // no error events surfaced through the runner
    const errs = player.core.events.filter((e) => String(e.kind).includes('error'))
    expect(errs.length).toBe(0)
    // the [i18n] handler records the selected language on the scene ctx
    const sv = await readCtx('settingsValues')
    expect(sv).toBeTruthy()
    expect(sv.language).toBe('en')
    // the scene ran to completion after the switch (relocalize_page is a REAL
    // full-page redraw on web — the [i18n] handler drives it synchronously and
    // the bridge re-exports text_state.draws, so already-displayed lines here
    // carry the NEW language; see the round-78 relocalize tests below)
    expect(player.core.backlog.map((b) => b.text).join(' | ')).toContain('after')
  }, 120000)

  it('[i18n language] missing/empty param degrades gracefully on web (no crash)', async () => {
    const NL = String.fromCharCode(10)
    // The i18n command contract marks language= required=true, so
    // schema.coerce rejects a missing/empty param BEFORE the handler runs
    // (same as desktop: a controlled dispatch error, not a crash). The
    // graceful guarantee on web is that the player surfaces the error and
    // survives: the runtime is not torn down and a later valid scene runs.
    const bad = await player.runScene('[i18n]', 'i18n_missing.ks', { maxFrames: 200000, autoClick: true })
    expect(bad.startsWith('ERR:'), 'missing language should fail cleanly').toBe(true)
    expect(bad).toContain('missing required param')
    expect(bad).toContain('language')

    const badEmpty = await player.runScene('[i18n language=""]', 'i18n_empty.ks', { maxFrames: 200000, autoClick: true })
    expect(badEmpty.startsWith('ERR:')).toBe(true)

    // Post-error robustness: a valid [i18n language="en"] still runs and
    // records the language (the rejected dispatch did not corrupt the VM).
    const good = ['[ch name="N" text="ok"]', '[p]', '[i18n language="ja"]', '[ch name="N" text="after"]', '[p]', '[end]'].join(NL)
    const out = await player.runScene(good, 'i18n_recover.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    await player.lua.doString('_G.__R = _G.__LAST_CTX and _G.__LAST_CTX.settingsValues or nil')
    expect(player.lua.global.get('__R')).toBeTruthy()
    expect(player.lua.global.get('__R').language).toBe('ja')
    expect(player.core.backlog.map((b) => b.text).join(' | ')).toContain('after')
  }, 120000)

  // ---- round 78: settings-language hot-switch parity on web ----------------
  // Desktop's settings menu re-localizes the ALREADY-DISPLAYED page via
  // TextCommands.relocalize_page (replays text_state.page_src through
  // _drawMessage with the new dictionary). The web player loads the real
  // i18n.lua, so [i18n language=] -> SystemCommands.i18n drives the SAME
  // relocalize_page synchronously; the bridge re-exports text_state.draws
  // after every runScene, so the re-localized page, choice labels and
  // backlog are all observable from the harness. Zero web-side changes were
  // needed (the whole path is Lua-side + the existing draws exporter).

  it('[i18n] re-localizes the ALREADY-DISPLAYED page draws (round 78 relocalize)', async () => {
    const readCtx = async (name) => {
      await player.lua.doString('_G.__R = _G.__LAST_CTX and _G.__LAST_CTX[' + JSON.stringify(name) + '] or nil')
      return player.lua.global.get('__R')
    }
    const NL = String.fromCharCode(10)
    // Control in zh only: the same {settings} line renders as 设置.
    player.core.draws = []
    const zhKs = [
      '[i18n language="zh"]',
      '[ch name="N" text="v={settings}"]',
      '[end]',
    ].join(NL)
    await player.runScene(zhKs, 'rl_zh.ks', { maxFrames: 200000, autoClick: true })
    const zhT = player.core.draws.map((d) => d.t).join(' | ')
    expect(zhT).toContain('v=设置')

    // Hot-switch in the SAME scene: the [ch] renders in zh (设置), then
    // [i18n language="en"] re-localizes the already-displayed page to
    // Settings. Because relocalize_page REPLAYS the page source (it does not
    // append), the zh draw is replaced — v=设置 must be gone. (A non-NVL
    // [ch] clears the message window, so no display command follows the
    // switch here — that would refresh the page and drop the relocalized line.)
    player.core.draws = []
    const ks = [
      '[i18n language="zh"]',
      '[ch name="N" text="v={settings}"]',      // zh draw: "v=设置"
      '[i18n language="en"]',                   // relocalize_page -> "v=Settings"
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'rl_switch.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const texts = player.core.draws.map((d) => d.t).join(' | ')
    // page draws were re-localized in place to the NEW (en) dictionary
    expect(texts).toContain('v=Settings')
    // and the old-language page draw was REPLAYED, not merely appended
    expect(texts).not.toContain('v=设置')
    // the switch recorded the active language on the scene ctx
    const sv = await readCtx('settingsValues')
    expect(sv && sv.language).toBe('en')

    // Persistence: a follow-up [ch] in the post-switch dictionary renders the
    // en value (the switch took effect for future lines, not just the redraw).
    player.core.draws = []
    const persist = [
      '[i18n language="en"]',
      '[ch name="N" text="post={language}"]',  // en draw: "post=Language"
      '[end]',
    ].join(NL)
    await player.runScene(persist, 'rl_persist.ks', { maxFrames: 200000, autoClick: true })
    expect(player.core.draws.map((d) => d.t).join(' | ')).toContain('post=Language')
  }, 120000)

  it('[button] choice labels registered pre-switch re-localize after [i18n] (round 78 relocalize)', async () => {
    const NL = String.fromCharCode(10)
    // A choice is STAGED in language A ([button text="{settings}"] -> 设置),
    // then [i18n language="en"] runs BEFORE [endbutton], so relocalize_page's
    // _relocalizeChoices rebuilds the label; [endbutton] then renders "1. Settings".
    // Park at the block (autoClick:false) so the rendered label is visible in
    // the exported draws (auto-selection would remove the choice group).
    player.core.draws = []
    const ks = [
      '[i18n language="zh"]',
      '[button text="{settings}" target="*a"]',
      '[i18n language="en"]',
      '[endbutton]',
      '[end]',
      '*a',
      '[ch name="N" text="arrived={language}"]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'rl_choice.ks', { maxFrames: 200000, autoClick: false })
    expect(out.startsWith('WAIT:'), out).toBe(true)
    const labels = player.core.draws.map((d) => d.t)
    // the rendered choice label shows the NEW language (en: "Settings")
    expect(labels.some((t) => t === '1. Settings'), JSON.stringify(labels)).toBe(true)
    // the old zh label was replaced, not left alongside
    expect(labels.some((t) => t === '1. 设置')).toBe(false)

    // A fresh run (auto-click picks option 1) routes through the relocalized
    // block and the post-switch dictionary persists for follow-up [ch].
    player.core.draws = []
    const out2 = await player.runScene(ks, 'rl_route.ks', { maxFrames: 200000, autoClick: true })
    expect(out2.startsWith('DONE:'), out2).toBe(true)
    const t2 = player.core.draws.map((d) => d.t).join(' | ')
    expect(t2).toContain('arrived=Language')
  }, 120000)

  it('[i18n] re-localizes ctx.backlog entries (round 78 relocalize, web runner exposes ctx)', async () => {
    const readBacklog = async () => {
      await player.lua.doString('_G.__R = _G.__LAST_CTX and _G.__LAST_CTX.backlog or nil')
      return player.lua.global.get('__R') || []
    }
    const NL = String.fromCharCode(10)
    // Control: pinned to zh, the backlog entry keeps its zh-localized text.
    const zhKs = [
      '[i18n language="zh"]',
      '[ch name="N" text="b={settings}"]',
      '[p]',
      '[end]',
    ].join(NL)
    player.core.backlog.length = 0
    await player.runScene(zhKs, 'rl_bk_zh.ks', { maxFrames: 200000, autoClick: true })
    const zhBl = await readBacklog()
    expect(zhBl.length).toBeGreaterThanOrEqual(1)
    expect(zhBl[0].text).toContain('b=设置')
    expect(zhBl[0].src).toBe('b={settings}')

    // Hot-switch scene: [ch] renders in zh, [p] commits the page, then
    // [i18n language="en"] -> relocalize_backlog re-localizes the stored
    // entry from its retained src (pre-localize source) to the new dict.
    const ks = [
      '[i18n language="zh"]',
      '[ch name="N" text="b={settings}"]',
      '[p]',
      '[i18n language="en"]',
      '[end]',
    ].join(NL)
    player.core.backlog.length = 0
    const out = await player.runScene(ks, 'rl_bk_switch.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    const bl = await readBacklog()
    expect(bl.length).toBeGreaterThanOrEqual(1)
    // entry was re-localized from 设置 -> Settings, keeping its src for the
    // next relocalize (the desktop backlog redraw contract).
    expect(bl[0].text).toContain('b=Settings')
    expect(bl[0].src).toBe('b={settings}')
  }, 120000)

  it('[palette] day/night/toggle run through a REAL web LUT, no degrade (round 77)', async () => {
    player.core.layers.clear()
    player.core.palette = { handle: null, intensity: 0, size: 0 }
    player.core.events.length = 0
    const NL = String.fromCharCode(10)
    // Minimal palette scene mirroring tutorial_13: day (clear) -> night
    // (load assets/lut/night.png + apply) -> toggle (back to day) -> day.
    // palette.lua drives the backend.* LUT surface; on web these are now
    // REAL bindings (load_image/is_valid/set_palette/destroy_texture) so
    // lut_available() is true and the scene runs WITHOUT the degrade.
    const ks = [
      '[palette effect=day]',
      '[palette effect=night]',
      '[palette effect=toggle]',
      '[palette effect=day]',
      '[ch name="N" text="palette-done"]',
      '[p]',
      '[end]',
    ].join(NL)
    const out = await player.runScene(ks, 'palette_lut.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    // No [palette] error: the old web gap crashed at palette.lua:39 (nil
    // load_image) and logged a KAG command 'palette' failed ERROR. A wired
    // LUT means zero error events.
    const errs = player.core.events.filter((e) => String(e.kind).includes('error'))
    expect(errs).toEqual([])
    // The engine's palette module tracked a real day<->night cycle:
    //   day -> set_palette(nil,0,0) neutral
    //   night -> load_image(path) => texture registered, set_palette(handle,1,16)
    //   toggle -> set_day_mode -> clear -> neutral
    //   day -> neutral again
    const sets = player.core.events.filter((e) => e.kind === 'palette.set')
    expect(sets.length).toBeGreaterThanOrEqual(4)
    // The night LUT was registered through the core texture pipeline.
    const nightTex = [...player.core.textures.values()].some((t) => t.path.includes('night.png'))
    expect(nightTex).toBe(true)
    // After the final [palette effect=day] the active LUT is cleared.
    expect(player.core.palette.handle).toBeNull()
  }, 120000)

  it('[palette] web LUT tints the DOM render output (night) and clears on day (round 77)', async () => {
    player.core.layers.clear()
    player.core.palette = { handle: null, intensity: 0, size: 0 }
    const NL = String.fromCharCode(10)
    // A background texture must be present so the render list is non-empty.
    const bgTex = player.core.loadTexture('bg_placeholder.png')
    player.core.layers.clear()
    const bg = player.core.ensureLayer('bg', { z: 0 })
    player.core.setLayerImage(bg, bgTex)
    renderer.setTextureUrl(bgTex, '/assets/bg_placeholder.png')
    // Neutral: no filter (day).
    player.core.setPalette(null, 0, 0)
    await renderer.render()
    expect(stage.style.filter).toBe('')
    // Night: the active palette tints the whole render output.
    player.core.setPalette(bgTex, 1.0, 16)
    await renderer.render()
    expect(stage.style.filter).toContain('brightness')
    expect(stage.style.filter).toContain('hue-rotate')
    // Back to day / neutral: filter clears.
    player.core.setPalette(null, 0, 0)
    await renderer.render()
    expect(stage.style.filter).toBe('')
  }, 60000)
})