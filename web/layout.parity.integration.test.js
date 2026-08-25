// @vitest-environment jsdom
// R107-A parity: the [layout] command family (declarative hbox / vbox /
// grid coordinate calculators) must behave identically under the web
// runner (wasmoon) as on desktop.
//
// [layout] is a pure-Lua COORDINATE CALCULATOR (kag/layout_math.lua +
// kag/commands/layout.lua): it resolves registered child layers and
// writes their absolute x/y via the shared layers.move_layer binding.
// On the web that binding routes through the wasmoon bridge (bridge.js
// jsLayers.move_layer -> AdapterCore.moveLayer -> core.layers node.x/node.y),
// the SAME nodes the DOM renderer (dom-renderer.js render() -> core.
// renderList()) reads to position <img>/<div> elements. So layout
// coordinates flow from the Lua handler to the rendered DOM with ZERO web
// adapter change --- parity by construction. These tests lock that.
//
// As with the R106 [tween] suite, R107-A has not yet registered the layout
// modules in kag.lua / regenerated scripts-index.json (shared coupling
// points), so this suite mounts the REAL scripts/kag/layout_math.lua and
// scripts/kag/commands/layout.lua sources into the live VM and registers
// kag.layout / kag.layout_slot / kag.layout_place --- test-local
// registrations that exercise the actual shipped implementation without
// touching the repo registration files.
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
    const rel = u.pathname.replace('/assets/lang/', '')
    const p = join(assetsDir, 'lang', ...rel.split('/'))
    return { ok: existsSync(p), status: existsSync(p) ? 200 : 404, text: async () => (existsSync(p) ? readFileSync(p, 'utf8') : ''), json: async () => index }
  }
  const rel = u.pathname.replace('/scripts/', '')
  const p = join(scriptsDir, ...rel.split('/'))
  return { text: async () => readFileSync(p, 'utf8'), json: async () => index, status: 200, ok: true }
}

let player = null
let layoutLoaded = false

// Mount the REAL layout stack (kag.layout_math + kag.commands.layout) into
// the live VM and register kag.layout / layout_slot / layout_place. Read from
// disk at runtime so the suite always exercises the shipped implementation.
async function mountLayout(player) {
  const mathSrc = readFileSync(join(rootDir, 'scripts', 'kag', 'layout_math.lua'), 'utf8')
  const cmdSrc = readFileSync(join(rootDir, 'scripts', 'kag', 'commands', 'layout.lua'), 'utf8')
  player.lua.global.set('__LAYOUT_MATH_SRC', mathSrc)
  player.lua.global.set('__LAYOUT_CMD_SRC', cmdSrc)
  await player.lua.doString([
    "local m = assert(load(_G.__LAYOUT_MATH_SRC, '@kag/layout_math.lua', 't', _ENV))",
    "package.preload['kag.layout_math'] = function() return m() end",
    "local c = assert(load(_G.__LAYOUT_CMD_SRC, '@kag/commands/layout.lua', 't', _ENV))",
    "package.preload['kag.commands.layout'] = function() return c() end",
    "local lay = require('kag.commands.layout')",
    "require('kag').layout = lay.layout",
    "require('kag').layout_slot = lay.layout_slot",
    "require('kag').layout_place = lay.layout_place",
    "_G.__LAYOUT_MOUNTED = (type(lay.layout) == 'function' and type(lay.layout_slot) == 'function' and lay.layout_place ~= nil) and true or false",
  ].join(String.fromCharCode(10)))
  return (await player.lua.global.get('__LAYOUT_MOUNTED')) === true
}

beforeAll(async () => {
  player = await createPlayer({
    scriptsBase: 'http://local/scripts/',
    fetchImpl: fileFetch,
    langBase: 'http://local/assets/lang/',
    wasmFile: join(here, 'node_modules', 'wasmoon', 'dist', 'glue.wasm'),
  })
  layoutLoaded = await mountLayout(player)
})

describe('web [layout] parity (wasmoon + real layout stack)', () => {
  it('mounts the real layout command + math modules', () => {
    expect(layoutLoaded).toBe(true)
  })

  // hbox of 3 + a separate vbox + a 2x2 grid, all in one scene. Each target
  // layer must land at the position the container math computes. The 'name'
  // key of each container drives registration via [layout_slot]; [layout] is
  // re-declared (idempotent) so the scene reads naturally.
  it('hbox(3) + vbox(2) + grid(2x2) complete DONE and snap the exact container-math coordinates (R107)', async () => {
    player.core.layers.clear()
    const ks = [
      // hbox: 3 elements, w=90 each, gap=10, padding=10, container 460x120
      '[layout name=hboxBox kind=hbox x=40 y=60 w=460 h=120 gap=10 padding=10]',
      '[layout_slot parent=hboxBox layer=elH1 index=1 size="90x80"]',
      '[layout_slot parent=hboxBox layer=elH2 index=2 size="90x80"]',
      '[layout_slot parent=hboxBox layer=elH3 index=3 size="90x80"]',
      // vbox: 2 elements, h=60 each, gap=12, padding=8, container 300x150
      '[layout name=vboxBox kind=vbox x=520 y=60 w=300 h=150 gap=12 padding=8]',
      '[layout_slot parent=vboxBox layer=elV1 index=1 size="100x60"]',
      '[layout_slot parent=vboxBox layer=elV2 index=2 size="100x60"]',
      // grid: 2x2, cols=2, cell basis 80x50, gap=8, container 190x120
      '[layout name=gridBox kind=grid x=860 y=60 w=190 h=120 cols=2 gap=8 padding=4]',
      '[layout_slot parent=gridBox layer=elG1 index=1 size="80x50"]',
      '[layout_slot parent=gridBox layer=elG2 index=2 size="80x50"]',
      '[layout_slot parent=gridBox layer=elG3 index=3 size="80x50"]',
      '[layout_slot parent=gridBox layer=elG4 index=4 size="80x50"]',
      '[ch name="N" text="arranged"]',
      '[p]',
      '[end]',
    ].join(String.fromCharCode(10))
    const out = await player.runScene(ks, 'layout_full.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), 'layout scene should complete: ' + out).toBe(true)
    // no command errors surfaced
    const errs = player.core.events.filter((e) => String(e.kind).includes('error'))
    expect(errs, 'no error events for [layout]').toEqual([])

    const L = (name) => player.core.layers.get(name)
    const expectPos = (name, x, y) => {
      const n = L(name)
      expect(n, 'layer ' + name + ' created by layout slot').toBeTruthy()
      expect([n.x, n.y]).toEqual([x, y])
    }

    // hbox (w=90, gap=10, pad=10): content x0=10, y0=10, boxW=440, boxH=100.
    // slots flow left->right from x0=10: 10, 110, 210. Origin (40,60).
    expectPos('elH1', 40 + 10, 60 + 10);
    expectPos('elH2', 40 + 110, 60 + 10);   // middle hbox element
    expectPos('elH3', 40 + 210, 60 + 10);
    // vbox (h=60, gap=12, pad=8): content x0=8, y0=8. slots flow top->down: 8, 80. Origin (520,60).
    expectPos('elV1', 520 + 8, 60 + 8);
    expectPos('elV2', 520 + 8, 60 + 80);
    // grid 2x2 (cellW=(190-8-8*... )) -- computed via layout_math.grid below;
    // assert exact positions through the same math call (Lua side).
  }, 60000)

  it('the written layer positions equal the pure Lua layout_math measure for the same containers (R107)', async () => {
    // Desktop parity: the layer x/y that [layout] writes must be exactly the
    // slots returned by the shared pure Lua calculator (origin added). Because
    // the web and desktop both run this identical module, asserting the layer
    // values against the SAME module proves cross-platform consistency.
    player.core.layers.clear()
    const ks = [
      '[layout name=h kind=hbox x=40 y=60 w=460 h=120 gap=10 padding=10]',
      '[layout_slot parent=h layer=elH1 index=1 size="90x80"]',
      '[layout_slot parent=h layer=elH2 index=2 size="90x80"]',
      '[layout_slot parent=h layer=elH3 index=3 size="90x80"]',
      '[layout name=v kind=vbox x=520 y=60 w=300 h=150 gap=12 padding=8]',
      '[layout_slot parent=v layer=elV1 index=1 size="100x60"]',
      '[layout_slot parent=v layer=elV2 index=2 size="100x60"]',
      '[layout name=g kind=grid x=860 y=60 w=190 h=120 cols=2 gap=8 padding=4]',
      '[layout_slot parent=g layer=elG1 index=1 size="80x50"]',
      '[layout_slot parent=g layer=elG2 index=2 size="80x50"]',
      '[layout_slot parent=g layer=elG3 index=3 size="80x50"]',
      '[layout_slot parent=g layer=elG4 index=4 size="80x50"]',
      '[p]',
      '[end]',
    ].join(String.fromCharCode(10))
    const out = await player.runScene(ks, 'layout_math.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)

    // Compute the same slots in Lua, then compare to the written layer x/y.
    const expected = await player.lua.doString([
      "local math2 = require('kag.layout_math')",
      "local mk = function(w,h) return { w = w, h = h } end",
      "local hres = math2.measure('hbox', { mk(90,80), mk(90,80), mk(90,80) }, { w=460, h=120, gap=10, padding=10 })",
      "local vres = math2.measure('vbox', { mk(100,60), mk(100,60) }, { w=300, h=150, gap=12, padding=8 })",
      "local gres = math2.measure('grid', { mk(80,50), mk(80,50), mk(80,50), mk(80,50) }, { w=190, h=120, cols=2, gap=8, padding=4 })",
      "return { h = hres.slots, v = vres.slots, g = gres.slots }",
    ].join(String.fromCharCode(10)))
    const L = (name) => player.core.layers.get(name)
    const origin = { h: [40, 60], v: [520, 60], g: [860, 60] }
    const check = (slots, names, ox, oy) => {
      expect(slots && slots.length).toBe(names.length)
      for (let i = 0; i < names.length; i++) {
        const s = slots[i], n = L(names[i]);
        expect(n, 'layer ' + names[i]).toBeTruthy()
        expect([n.x, n.y]).toEqual([ox + s.x, oy + s.y]);
      }
    };
    check(expected.h, ['elH1', 'elH2', 'elH3'], origin.h[0], origin.h[1]);
    check(expected.v, ['elV1', 'elV2'], origin.v[0], origin.v[1]);
    check(expected.g, ['elG1', 'elG2', 'elG3', 'elG4'], origin.g[0], origin.g[1]);
  }, 60000)

  it('DOM renderer positions <img> elements at the computed layout coordinates (R107)', async () => {
    player.core.layers.clear()
    const stage = document.createElement('div')
    document.body.appendChild(stage)
    const renderer = new DomRenderer(player.core, stage)
    for (const name of ['elH1', 'elH2', 'elH3']) {
      const n = player.core.ensureLayer(name, { w: 90, h: 80 })
      player.core.setLayerImage(n, player.core.loadTexture(name + '.png'))
    }
    const ks = [
      '[layout name=h kind=hbox x=40 y=60 w=460 h=120 gap=10 padding=10]',
      '[layout_slot parent=h layer=elH1 index=1 size="90x80"]',
      '[layout_slot parent=h layer=elH2 index=2 size="90x80"]',
      '[layout_slot parent=h layer=elH3 index=3 size="90x80"]',
      '[ch name="N" text="arranged"]',
      '[p]',
      '[end]',
    ].join(String.fromCharCode(10))
    const out = await player.runScene(ks, 'layout_dom.ks', { maxFrames: 200000, autoClick: true })
    expect(out.startsWith('DONE:'), out).toBe(true)
    await renderer.render()
    const lefts = {}
    // The renderer makes an <img> for image-bearing layers, a <div> otherwise;
    // both carry [data-layer]. Query [data-layer] so positioning is asserted
    // regardless of element kind.
    for (const el of stage.querySelectorAll('[data-layer]')) {
      lefts[el.dataset.layer] = { left: el.style.left, top: el.style.top }
    }
    // hbox slots at x0=10,110,210 + origin x=40 -> 50,150,250; y=60+10=70.
    expect(lefts.elH1).toEqual({ left: '50px', top: '70px' })
    expect(lefts.elH2).toEqual({ left: '150px', top: '70px' })
    expect(lefts.elH3).toEqual({ left: '250px', top: '70px' })
    renderer.destroy()
    stage.remove()
  }, 60000)
})