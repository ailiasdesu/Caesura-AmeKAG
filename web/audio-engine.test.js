// @vitest-environment node
import { describe, it, expect, vi } from 'vitest'
import { AudioEngine } from './audio-engine.js'

// minimal fake WebAudio context for engine tests
function fakeContext() {
  const destinations = []
  const sources = []
  const ctx = {
    currentTime: 0,
    destination: { kind: 'dest' },
    createGain: () => ({ gain: { value: 1 }, connect: vi.fn(), disconnect: vi.fn() }),
    createBufferSource: () => {
      const s = { buffer: null, loop: false, connect: vi.fn(), start: vi.fn(), stop: vi.fn(), disconnect: vi.fn() }
      sources.push(s)
      return s
    },
    decodeAudioData: vi.fn(async () => ({ duration: 5 })),
  }
  return { ctx, sources, destinations }
}

describe('AudioEngine', () => {
  it('builds the three SoLoud buses on init', () => {
    const { ctx } = fakeContext()
    const eng = new AudioEngine({ ctx })
    expect(eng.ready).toBe(true)
    expect(eng._busGains.has('bgm')).toBe(true)
    expect(eng._busGains.has('se')).toBe(true)
    expect(eng._busGains.has('voice')).toBe(true)
  })

  it('loads, plays and reports isPlaying on a bus', async () => {
    const { ctx, sources } = fakeContext()
    const eng = new AudioEngine({ ctx })
    globalThis.fetch = vi.fn(async () => ({ ok: true, arrayBuffer: async () => new ArrayBuffer(8) }))
    const ok = await eng.play('bgm', 'assets/bgm/daily.wav', { assetUrl: (p) => 'http://x/' + p })
    expect(ok).toBe(true)
    expect(sources[0].buffer).toBeTruthy()
    expect(eng.isPlaying('bgm')).toBe(true)
    ctx.currentTime = 10; // past the 5s clip
    expect(eng.isPlaying('bgm')).toBe(false)
    eng.stopAll()
    expect(eng.isPlaying('bgm')).toBe(false)
  })

  it('stop cuts the source and disconnects', async () => {
    const { ctx, sources } = fakeContext()
    const eng = new AudioEngine({ ctx })
    globalThis.fetch = vi.fn(async () => ({ ok: true, arrayBuffer: async () => new ArrayBuffer(8) }))
    await eng.play('voice', 'v.wav', { assetUrl: 'http://x/' })
    eng.stop('voice')
    expect(eng._sources.size).toBe(0)
    expect(eng.isPlaying('voice')).toBe(false)
  })

  it('caches decoded buffers per path', async () => {
    const { ctx } = fakeContext()
    const eng = new AudioEngine({ ctx })
    globalThis.fetch = vi.fn(async () => ({ ok: true, arrayBuffer: async () => new ArrayBuffer(8) }))
    await eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })
    await eng.play('se', 'a.wav', { assetUrl: 'http://x/' })
    expect(globalThis.fetch).toHaveBeenCalledTimes(1)
  })

  it('degrades without an AudioContext (jsdom/headless)', async () => {
    const eng = new AudioEngine({});
    expect(eng.ready).toBe(false)
    const ok = await eng.play('bgm', 'x.wav', { assetUrl: 'http://x/' })
    expect(ok).toBe(false)
    expect(eng.isPlaying('bgm')).toBe(false)
  })
})
