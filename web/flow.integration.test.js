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
  }, 120000)
})
