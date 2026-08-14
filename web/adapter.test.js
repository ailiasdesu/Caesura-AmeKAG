// @vitest-environment jsdom
import { describe, it, expect } from 'vitest'
import { AdapterCore } from './adapter-core.js'
import { DomRenderer } from './dom-renderer.js'

describe('AdapterCore (pure state machine)', () => {
  it('creates layers with defaults and idempotent ensure', () => {
    const core = new AdapterCore()
    const bg = core.ensureLayer('bg', { w: 1280, h: 720, layer_type: 2 })
    expect(bg.name).toBe('bg')
    expect(bg.w).toBe(1280)
    expect(core.getLayer('bg')).toBe(bg)
    expect(core.ensureLayer('bg')).toBe(bg) // idempotent
    expect(core.getLayer('nope')).toBeNull()
  })

  it('loads textures deduplicated by path', () => {
    const core = new AdapterCore()
    const a = core.loadTexture('assets/bg/classroom.png')
    const b = core.loadTexture('assets/bg/classroom.png')
    expect(a).toBe(b)
    expect(core.textures.size).toBe(1)
    const c = core.loadTexture('assets/fg/girl.png')
    expect(c).not.toBe(a)
  })

  it('tracks text buffer', () => {
    const core = new AdapterCore()
    core.setText('Hello')
    expect(core.textBuffer).toBe('Hello')
    core.clearText()
    expect(core.textBuffer).toBe('')
    core.appendText('a')
    core.appendText('b')
    expect(core.textBuffer).toBe('ab')
  })

  it('renderList filters invisible/empty layers and sorts by z', () => {
    const core = new AdapterCore()
    const bg = core.ensureLayer('bg', { z: 0 })
    core.setLayerImage(bg, core.loadTexture('a.png'))
    const fg = core.ensureLayer('fg', { z: 5 })
    core.setLayerImage(fg, core.loadTexture('b.png'))
    const hidden = core.ensureLayer('hidden', { z: 9 })
    core.setLayerImage(hidden, core.loadTexture('c.png'))
    core.setLayerVisible(hidden, false)
    const noTex = core.ensureLayer('empty', { z: 1 })
    core.setLayerImage(noTex, null)
    const list = core.renderList()
    expect(list.map((n) => n.name)).toEqual(['bg', 'fg'])
  })

  it('audio bus records play/stop', () => {
    const core = new AdapterCore()
    core.audioPlay('bgm', 'assets/bgm/daily.wav', 0.8)
    expect(core.audioIsPlaying('bgm')).toBe(true)
    core.audioStop('bgm')
    expect(core.audioIsPlaying('bgm')).toBe(false)
    expect(core.events.some((e) => e.kind === 'audio.play')).toBe(true)
  })

  it('logs every mutation for telemetry', () => {
    const core = new AdapterCore()
    core.ensureLayer('bg')
    core.loadTexture('x.png')
    core.setText('hi')
    expect(core.events.map((e) => e.kind)).toEqual([
      'layer.create', 'texture.load', 'text.set',
    ])
  })
})

describe('DomRenderer (jsdom)', () => {
  it('renders visible textured layers as positioned elements', () => {
    const core = new AdapterCore()
    const root = document.createElement('div')
    const ren = new DomRenderer(core, root)
    const bg = core.ensureLayer('bg', { layer_type: 2, z: 0, w: 1280, h: 720 })
    core.setLayerImage(bg, core.loadTexture('bg.png'))
    ren.setTextureUrl(core.textures.keys().next().value, 'http://x/bg.png')
    ren.render()
    const img = root.querySelector('img[data-layer="bg"]')
    expect(img).toBeTruthy()
    expect(img.getAttribute('src')).toBe('http://x/bg.png')
    expect(img.style.zIndex).toBe('0')
    ren.destroy()
    expect(root.children.length).toBe(0)
  })

  it('renders message overlay with text content', () => {
    const core = new AdapterCore()
    const root = document.createElement('div')
    const ren = new DomRenderer(core, root)
    core.setText('Hello world')
    ren.render()
    const msg = root.querySelector('.caesura-message')
    expect(msg?.textContent).toBe('Hello world')
    core.clearText()
    ren.render()
    expect(root.querySelector('.caesura-message')).toBeNull()
    ren.destroy()
  })

  it('removes stale layers on re-render', () => {
    const core = new AdapterCore()
    const root = document.createElement('div')
    const ren = new DomRenderer(core, root)
    const a = core.ensureLayer('a', { layer_type: 2 })
    core.setLayerImage(a, core.loadTexture('a.png'))
    ren.render()
    expect(root.querySelectorAll('.caesura-layer').length).toBe(1)
    core.removeLayer('a')
    ren.render()
    expect(root.querySelectorAll('.caesura-layer').length).toBe(0)
    ren.destroy()
  })
})

describe('AdapterCore audio simulation', () => {
  it('treats a playing clip as ended after its simulated duration', () => {
    const core = new AdapterCore()
    core.audioPlay('voice', 'assets/voice/line01.wav')
    core.tick()
    core.tick()
    expect(core.audioIsPlaying('voice')).toBe(true)
    for (let i = 0; i < 120; i++) core.tick()
    expect(core.audioIsPlaying('voice')).toBe(false)
  })

  it('audioEnded cuts a clip short', () => {
    const core = new AdapterCore()
    core.audioPlay('bgm', 'assets/bgm/daily.wav')
    core.audioEnded('bgm')
    expect(core.audioIsPlaying('bgm')).toBe(false)
  })

  it('tick advances the frame counter', () => {
    const core = new AdapterCore()
    core.tick()
    core.tick()
    core.tick()
    expect(core._frame).toBe(3)
  })
})

describe('AdapterCore backlog (VN history)', () => {
  it('commits distinct text pages to history', () => {
    const core = new AdapterCore()
    core.setDraws([{ t: 'Hello', x: 10, y: 20 }])
    core.setDraws([{ t: 'Hello', x: 10, y: 20 }]) // unchanged -> no new entry
    core.setDraws([{ t: 'Next line', x: 10, y: 40 }])
    expect(core.backlog.length).toBe(2)
    expect(core.backlog[0].text).toBe('Hello')
    expect(core.backlog[1].text).toBe('Next line')
    expect(core.backlog[1].draws[0].y).toBe(40)
  })

  it('ignores empty pages', () => {
    const core = new AdapterCore()
    core.setDraws([])
    core.setDraws([{ t: '   ', x: 0, y: 0 }])
    expect(core.backlog.length).toBe(0)
  })

  it('backlog entries are snapshots (later draws do not mutate them)', () => {
    const core = new AdapterCore()
    core.setDraws([{ t: 'A', x: 1, y: 2 }])
    const first = core.backlog[0].draws[0]
    core.setDraws([{ t: 'B', x: 3, y: 4 }])
    expect(first.x).toBe(1)
    expect(core.backlog[0].text).toBe('A')
  })
})
