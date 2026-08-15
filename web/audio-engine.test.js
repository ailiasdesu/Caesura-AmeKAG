// @vitest-environment node
import { describe, it, expect, vi } from 'vitest'
import { AudioEngine } from './audio-engine.js'

// minimal fake WebAudio context for engine tests
function fakeContext() {
  const destinations = []
  const sources = []
  const gains = []
  const ctx = {
    currentTime: 0,
    destination: { kind: 'dest' },
    createGain: () => {
      const g = { gain: { value: 1 }, connect: vi.fn(), disconnect: vi.fn() }
      gains.push(g)
      return g
    },
    createBufferSource: () => {
      const s = { buffer: null, loop: false, connect: vi.fn(), start: vi.fn(), stop: vi.fn(), disconnect: vi.fn() }
      sources.push(s)
      return s
    },
    decodeAudioData: vi.fn(async () => ({ duration: 5 })),
    suspend: vi.fn(async () => {}),
    resume: vi.fn(async () => {}),
    close: vi.fn(async () => {}),
  }
  return { ctx, sources, destinations, gains }
}

// engine wired to a fake context + a fetch that returns a decodable blob
function mkEngine() {
  const { ctx, sources, gains } = fakeContext()
  const eng = new AudioEngine({ ctx })
  globalThis.fetch = vi.fn(async () => ({ ok: true, arrayBuffer: async () => new ArrayBuffer(8) }))
  return { ctx, sources, gains, eng }
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

// =============================================================
// round 83 deep coverage — bus routing
// =============================================================
describe('AudioEngine · bus routing', () => {
  it('routes a source onto its own bus gain (not the destination)', async () => {
    const { ctx, eng } = mkEngine()
    await eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })
    const rec = eng._sources.get('bgm')
    expect(rec.gain).toBe(eng._busGains.get('bgm'))
    expect(rec.gain).not.toBe(ctx.destination)
    expect(rec.source.connect).toHaveBeenCalledWith(eng._busGains.get('bgm'))
  })

  it('plays all three kinds concurrently on independent buses', async () => {
    const eng = mkEngine().eng
    await eng.play('bgm', 'bgm.wav', { assetUrl: 'http://x/' })
    await eng.play('se', 'se.wav', { assetUrl: 'http://x/' })
    await eng.play('voice', 'v.wav', { assetUrl: 'http://x/' })
    expect(eng._sources.size).toBe(3)
    expect(eng._sources.get('bgm').gain).toBe(eng._busGains.get('bgm'))
    expect(eng._sources.get('se').gain).toBe(eng._busGains.get('se'))
    expect(eng._sources.get('voice').gain).toBe(eng._busGains.get('voice'))
    for (const k of ['bgm', 'se', 'voice']) expect(eng.isPlaying(k)).toBe(true)
  })

  it('same-kind play cuts the previous source (one live source per kind)', async () => {
    const { sources, eng } = mkEngine()
    await eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })
    const first = eng._sources.get('bgm').source
    await eng.play('bgm', 'b.wav', { assetUrl: 'http://x/' })
    const second = eng._sources.get('bgm').source
    expect(first).not.toBe(second)
    expect(first.stop).toHaveBeenCalled()
    expect(eng._sources.size).toBe(1)
    expect(sources.length).toBe(2)
    // the replacement source is the live one
    expect(eng._sources.get('bgm').source).toBe(second)
  })

  it('muting one bus does not affect the others', () => {
    const eng = mkEngine().eng
    eng.setBusVolume('se', 0)
    expect(eng._busGains.get('se').gain.value).toBe(0)
    expect(eng._busGains.get('bgm').gain.value).toBe(1)
    expect(eng._busGains.get('voice').gain.value).toBe(1)
  })
})

// =============================================================
// round 83 deep coverage — volume control
// =============================================================
describe('AudioEngine · volume control', () => {
  it('setBusVolume is per-bus independent', () => {
    const eng = mkEngine().eng
    eng.setBusVolume('bgm', 0.5)
    eng.setBusVolume('se', 0.25)
    eng.setBusVolume('voice', 0.8)
    expect(eng._busGains.get('bgm').gain.value).toBe(0.5)
    expect(eng._busGains.get('se').gain.value).toBe(0.25)
    expect(eng._busGains.get('voice').gain.value).toBe(0.8)
  })

  it('accepts 0, 1 and mid values', () => {
    const eng = mkEngine().eng
    eng.setBusVolume('bgm', 0)
    expect(eng._busGains.get('bgm').gain.value).toBe(0)
    eng.setBusVolume('bgm', 1)
    expect(eng._busGains.get('bgm').gain.value).toBe(1)
    eng.setBusVolume('bgm', 0.35)
    expect(eng._busGains.get('bgm').gain.value).toBe(0.35)
  })

  it('global mute mutes all three buses', () => {
    const eng = mkEngine().eng
    for (const k of ['bgm', 'se', 'voice']) eng.setBusVolume(k, 0)
    for (const k of ['bgm', 'se', 'voice']) expect(eng._busGains.get(k).gain.value).toBe(0)
  })

  it('volume persists across plays; explicit play volume overrides the bus', async () => {
    const eng = mkEngine().eng
    eng.setBusVolume('bgm', 0.6)
    await eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })
    expect(eng._busGains.get('bgm').gain.value).toBe(0.6)
    await eng.play('bgm', 'b.wav', { assetUrl: 'http://x/', volume: 0.2 })
    expect(eng._busGains.get('bgm').gain.value).toBe(0.2)
  })

  it('play accepts an explicit muted (0) volume', async () => {
    const eng = mkEngine().eng
    await eng.play('bgm', 'a.wav', { assetUrl: 'http://x/', volume: 0 })
    expect(eng._busGains.get('bgm').gain.value).toBe(0)
  })
})

// =============================================================
// round 83 deep coverage — lifecycle
// =============================================================
describe('AudioEngine · lifecycle', () => {
  it('suspend/resume delegate to the AudioContext', () => {
    const { ctx, eng } = mkEngine()
    eng.suspend()
    eng.resume()
    expect(ctx.suspend).toHaveBeenCalledTimes(1)
    expect(ctx.resume).toHaveBeenCalledTimes(1)
  })

  it('destroy stops all sources, closes the context and clears state', async () => {
    const { ctx, eng } = mkEngine()
    await eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })
    await eng.play('se', 'b.wav', { assetUrl: 'http://x/' })
    eng.destroy()
    expect(ctx.close).toHaveBeenCalledTimes(1)
    expect(eng.ready).toBe(false)
    expect(eng._ctx).toBeNull()
    expect(eng._sources.size).toBe(0)
    expect(eng._busGains.size).toBe(0)
    expect(eng._buffers.size).toBe(0)
  })

  it('calls after destroy degrade safely without throwing', async () => {
    const eng = mkEngine().eng
    eng.destroy()
    await expect(eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })).resolves.toBe(false)
    expect(() => eng.stop('bgm')).not.toThrow()
    expect(eng.isPlaying('bgm')).toBe(false)
    expect(() => eng.setBusVolume('bgm', 0.5)).not.toThrow()
    expect(() => eng.stopAll()).not.toThrow()
    expect(() => eng.suspend()).not.toThrow()
    expect(() => eng.resume()).not.toThrow()
  })

  it('double destroy is safe', () => {
    const eng = mkEngine().eng
    eng.destroy()
    expect(() => eng.destroy()).not.toThrow()
  })

  it('ensureContext re-initializes the three buses after destroy', async () => {
    const eng = mkEngine().eng
    eng.destroy()
    let created = 0
    const fakeCtor = function () { created++; return fakeContext().ctx }
    const prev = globalThis.AudioContext
    globalThis.AudioContext = fakeCtor
    try {
      const ctx2 = eng.ensureContext()
      expect(created).toBe(1)
      expect(ctx2).toBeTruthy()
      expect(eng.ready).toBe(true)
      expect(eng._busGains.has('bgm')).toBe(true)
      expect(eng._busGains.has('se')).toBe(true)
      expect(eng._busGains.has('voice')).toBe(true)
      await expect(eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })).resolves.toBe(true)
    } finally {
      if (prev === undefined) delete globalThis.AudioContext
      else globalThis.AudioContext = prev
    }
  })
})

// =============================================================
// round 83 deep coverage — resource tolerance
// =============================================================
describe('AudioEngine · resource tolerance', () => {
  it('fetch rejection degrades to false without throwing', async () => {
    const eng = mkEngine().eng
    globalThis.fetch = vi.fn(async () => { throw new Error('network down') })
    await expect(eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })).resolves.toBe(false)
    expect(eng.isPlaying('bgm')).toBe(false)
    expect(eng._sources.size).toBe(0)
  })

  it('a non-ok HTTP response degrades to false', async () => {
    const eng = mkEngine().eng
    globalThis.fetch = vi.fn(async () => ({ ok: false, status: 404 }))
    await expect(eng.play('se', 'a.wav', { assetUrl: 'http://x/' })).resolves.toBe(false)
    expect(eng.isPlaying('se')).toBe(false)
  })

  it('a failed load is not cached, so a later retry can succeed', async () => {
    const eng = mkEngine().eng
    globalThis.fetch = vi.fn()
      .mockRejectedValueOnce(new Error('first load fails'))
      .mockResolvedValueOnce({ ok: true, arrayBuffer: async () => new ArrayBuffer(8) })
    await expect(eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })).resolves.toBe(false)
    await expect(eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })).resolves.toBe(true)
    expect(eng._buffers.has('a.wav')).toBe(true)
    expect(globalThis.fetch).toHaveBeenCalledTimes(2)
  })

  it('decodeAudioData rejection degrades to false', async () => {
    const eng = mkEngine().eng
    eng._ctx.decodeAudioData = vi.fn(async () => { throw new Error('decode fail') })
    await expect(eng.play('voice', 'a.wav', { assetUrl: 'http://x/' })).resolves.toBe(false)
  })

  it('an empty (null) decoded buffer is treated as unplayable', async () => {
    const eng = mkEngine().eng
    eng._ctx.decodeAudioData = vi.fn(async () => null)
    await expect(eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })).resolves.toBe(false)
  })

  it('stop on a kind with no active source is a safe no-op', () => {
    const eng = mkEngine().eng
    expect(() => eng.stop('se')).not.toThrow()
    expect(eng._sources.size).toBe(0)
    expect(() => eng.stopAll()).not.toThrow()
  })
})

// =============================================================
// round 83 deep coverage — state queries & events
// =============================================================
describe('AudioEngine · state queries', () => {
  it('isPlaying is kind-independent and clears after the clip ends', async () => {
    const { ctx, eng } = mkEngine()
    await eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })
    await eng.play('voice', 'v.wav', { assetUrl: 'http://x/' })
    expect(eng.isPlaying('bgm')).toBe(true)
    expect(eng.isPlaying('voice')).toBe(true)
    ctx.currentTime = 6 // past the 5s clip
    expect(eng.isPlaying('bgm')).toBe(false)
    expect(eng.isPlaying('voice')).toBe(false)
  })

  it('stop(kind) only halts that kind, leaving others playing', async () => {
    const eng = mkEngine().eng
    await eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })
    await eng.play('se', 'b.wav', { assetUrl: 'http://x/' })
    eng.stop('se')
    expect(eng.isPlaying('se')).toBe(false)
    expect(eng.isPlaying('bgm')).toBe(true)
  })

  it('stopAll clears every kind', async () => {
    const eng = mkEngine().eng
    await eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })
    await eng.play('se', 'b.wav', { assetUrl: 'http://x/' })
    await eng.play('voice', 'c.wav', { assetUrl: 'http://x/' })
    eng.stopAll()
    expect(eng._sources.size).toBe(0)
    for (const k of ['bgm', 'se', 'voice']) expect(eng.isPlaying(k)).toBe(false)
  })

  it('the onended event drops the finished source from the active map', async () => {
    const eng = mkEngine().eng
    await eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })
    const src = eng._sources.get('bgm').source
    expect(typeof src.onended).toBe('function')
    src.onended()
    expect(eng.isPlaying('bgm')).toBe(false)
    expect(eng._sources.get('bgm')).toBeUndefined()
    // a stale onended from a superseded source must not clear a newer one
  })

  it('stale onended from a replaced source does not clear the newer source', async () => {
    const eng = mkEngine().eng
    await eng.play('bgm', 'a.wav', { assetUrl: 'http://x/' })
    const first = eng._sources.get('bgm').source
    await eng.play('bgm', 'b.wav', { assetUrl: 'http://x/' })
    const second = eng._sources.get('bgm').source
    first.onended() // late end from the replaced source
    expect(eng.isPlaying('bgm')).toBe(true)
    expect(eng._sources.get('bgm').source).toBe(second)
  })
})

// =============================================================
// round 83 — bridge integration contract
// =============================================================
describe('AudioEngine · bridge contract', () => {
  it('exposes the method surface createPlayer/backend relies on', () => {
    const eng = new AudioEngine({})
    for (const m of ['play', 'stop', 'isPlaying', 'setBusVolume', 'stopAll', 'suspend', 'resume', 'destroy']) {
      expect(typeof eng[m]).toBe('function')
    }
  })

  it('the backend.audio_play call shape (kind, file, { volume, assetUrl }) routes correctly', async () => {
    // bridge.js: backend.audio_play(kind, file, opts) -> audio.play(kind, file, { volume: opts.volume, assetUrl })
    const { eng } = mkEngine()
    const kind = 'bgm', file = 'music/track1.wav'
    await eng.play(kind, file, { volume: 0.4, assetUrl: (p) => 'http://cdn/' + p })
    expect(eng.isPlaying('bgm')).toBe(true)
    expect(eng._busGains.get('bgm').gain.value).toBe(0.4)
    // backend.audio_stop(kind) -> audio.stop(kind)
    eng.stop(kind)
    expect(eng.isPlaying(kind)).toBe(false)
    // backend.audio_set_bus_volume(kind, v) -> audio.setBusVolume(kind, v)
    eng.setBusVolume('voice', 0.9)
    expect(eng._busGains.get('voice').gain.value).toBe(0.9)
    // backend.audio_is_playing(kind) -> audio.isPlaying(kind)
    await eng.play('se', 'sfx.wav', { assetUrl: 'http://cdn/' })
    expect(eng.isPlaying('se')).toBe(true)
  })
})

})
