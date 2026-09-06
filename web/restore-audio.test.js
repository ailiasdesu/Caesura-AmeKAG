// @vitest-environment node
import { afterEach, describe, expect, it, vi } from 'vitest'
import { AudioEngine } from './audio-engine.js'
import { createAudioRestore } from './restore-audio.js'

const clip = { path: 'assets/bgm/a.wav', position: 2.5, gain: 0.4, looping: true }
const snapshot = (bgm = clip) => ({ version: 1, bgm: bgm === false ? false : { ...bgm } })
const deferred = () => {
  let resolve, reject
  const promise = new Promise((yes, no) => { resolve = yes; reject = no })
  return { promise, resolve, reject }
}

function fixture() {
  const sources = [], gains = [], events = []
  const buffer = { duration: 10, length: 480000, sampleRate: 48000, numberOfChannels: 2 }
  const ctx = {
    currentTime: 0, state: 'running', destination: {},
    createGain: vi.fn(() => {
      const gain = { gain: { value: 1 }, connect: vi.fn(), disconnect: vi.fn() }
      gains.push(gain)
      return gain
    }),
    createBufferSource: vi.fn(() => {
      const source = { connect: vi.fn(), disconnect: vi.fn(), start: vi.fn(() => events.push('start')),
        stop: vi.fn(() => events.push('stop')) }
      sources.push(source)
      return source
    }),
    decodeAudioData: vi.fn(async bytes => { events.push('decode'); expect(bytes.byteLength).toBe(4); return buffer }),
    close: vi.fn(async () => { ctx.state = 'closed' }),
  }
  const fetchImpl = vi.fn(async () => { events.push('fetch'); return new Response(new Uint8Array([1, 2, 3, 4])) })
  const audio = new AudioEngine({ ctx, fetchImpl })
  const restore = createAudioRestore({ audio, fetchImpl, assetUrl: path => '/game/' + path })
  return { audio, restore, ctx, buffer, fetchImpl, sources, gains, events }
}

afterEach(() => vi.unstubAllGlobals())

describe('WebAudio prepared restore ownership', () => {
  it('prepares actual retained bytes without changing a live source or user bus volume', async () => {
    const f = fixture()
    f.audio.setBusVolume('bgm', 0.7)
    await f.audio.play('bgm', 'assets/old.wav', { volume: 0.8, loop: true })
    const old = f.sources[0]
    const input = snapshot()
    const ticket = await f.restore.prepare_audio(input)
    input.bgm.path = 'mutated.wav'
    input.bgm.position = 9
    expect(Object.keys(ticket)).toEqual([])
    expect(f.sources).toHaveLength(1)
    expect(old.stop).not.toHaveBeenCalled()
    expect(f.audio._busGains.get('bgm').gain.value).toBe(0.7)
    f.fetchImpl.mockClear()
    f.ctx.decodeAudioData.mockClear()
    expect(f.restore.apply_audio(ticket)).toBe(true)
    expect(old.stop).toHaveBeenCalledOnce()
    const source = f.sources[1]
    expect(source.buffer).toBe(f.buffer)
    expect(source.loop).toBe(true)
    expect(source.start).toHaveBeenCalledWith(0, 2.5)
    expect(f.fetchImpl).not.toHaveBeenCalled()
    expect(f.ctx.decodeAudioData).not.toHaveBeenCalled()
    expect(f.audio._busGains.get('bgm').gain.value).toBe(0.7)
    expect(f.restore.capture_audio()).toEqual(snapshot())
    f.ctx.currentTime = 20.25
    expect(f.restore.capture_audio().bgm.position).toBeCloseTo(2.75)
    expect(f.audio.isPlaying('bgm')).toBe(true)
  })

  it('captures authoritative path, source gain, loop and audio-clock position', async () => {
    const f = fixture()
    await f.audio.play('bgm', clip.path, { volume: 0.6, loop: false })
    f.audio.setBusVolume('bgm', 0)
    f.ctx.currentTime = 3.25
    expect(f.restore.capture_audio()).toEqual(snapshot({ ...clip, position: 3.25, gain: 0.6, looping: false }))
    f.ctx.currentTime = 11
    expect(f.restore.capture_audio()).toEqual(snapshot(false))
  })

  it('consumes tickets once and makes discard/stop idempotent', async () => {
    const f = fixture()
    const cancelled = await f.restore.prepare_audio(snapshot())
    f.restore.discard_audio(cancelled)
    f.restore.discard_audio(cancelled)
    expect(() => f.restore.apply_audio(cancelled)).toThrow(/consumed|unavailable/i)
    expect(() => f.restore.apply_audio({})).toThrow(/consumed|unavailable/i)
    const played = await f.restore.prepare_audio(snapshot())
    f.restore.apply_audio(played)
    expect(() => f.restore.apply_audio(played)).toThrow(/consumed|unavailable/i)
    f.restore.discard_audio(played)
    f.restore.stop_audio()
    f.restore.stop_audio()
    expect(f.sources[0].stop).toHaveBeenCalledOnce()
    expect(f.restore.capture_audio()).toEqual(snapshot(false))
  })

  it('a silent candidate stops every bus without fetch or context fabrication', async () => {
    const f = fixture()
    for (const kind of ['bgm', 'se', 'voice']) await f.audio.play(kind, kind + '.wav')
    f.fetchImpl.mockClear()
    const ticket = await f.restore.prepare_audio(snapshot(false))
    expect(f.restore.apply_audio(ticket)).toBe(true)
    expect(f.sources.every(source => source.stop.mock.calls.length === 1)).toBe(true)
    expect(f.fetchImpl).not.toHaveBeenCalled()
    vi.stubGlobal('AudioContext', undefined)
    vi.stubGlobal('webkitAudioContext', undefined)
    const audio = new AudioEngine()
    const restore = createAudioRestore({ audio, fetchImpl: f.fetchImpl })
    expect(await audio.play('bgm', 'silent.wav')).toBe(false)
    expect(restore.capture_audio()).toEqual(snapshot(false))
    await expect(restore.prepare_audio(snapshot())).rejects.toThrow(/AudioContext|audio context/i)
    expect(restore.apply_audio(await restore.prepare_audio(snapshot(false)))).toBe(true)
  })

  it.each([
    null, { version: 2, bgm: false }, snapshot({ ...clip, path: '../secret.wav' }),
    snapshot({ ...clip, gain: NaN }), snapshot({ ...clip, gain: 17 }),
    snapshot({ ...clip, position: Infinity }), snapshot({ ...clip, position: -1 }),
    snapshot({ ...clip, looping: 'false' }),
  ])('rejects invalid declarations without reading or changing sound (%j)', async value => {
    const f = fixture()
    await expect(f.restore.prepare_audio(value)).rejects.toThrow()
    expect(f.fetchImpl).not.toHaveBeenCalled()
    expect(f.sources).toHaveLength(0)
  })

  it('rejects corrupt, oversized and out-of-range audio before publication', async () => {
    const f = fixture()
    await f.audio.play('bgm', 'old.wav')
    const old = f.sources[0]
    f.ctx.decodeAudioData.mockRejectedValueOnce(new Error('corrupt audio'))
    await expect(f.restore.prepare_audio(snapshot())).rejects.toThrow(/corrupt/)
    f.ctx.decodeAudioData.mockResolvedValueOnce({ ...f.buffer, length: 100000000, duration: 100000000 / 48000 })
    await expect(f.restore.prepare_audio(snapshot())).rejects.toThrow(/decoded|limit/i)
    await expect(f.restore.prepare_audio(snapshot({ ...clip, position: 10 }))).rejects.toThrow(/position|offset/i)
    f.fetchImpl.mockResolvedValueOnce(new Response(new Uint8Array([1]), { headers: { 'content-length': String(65 * 1024 * 1024) } }))
    await expect(f.restore.prepare_audio(snapshot())).rejects.toThrow(/limit/i)
    expect(old.stop).not.toHaveBeenCalled()
    expect(f.sources).toHaveLength(1)
  })

  it('start failure consumes its ticket, disconnects partial nodes and leaves silence', async () => {
    const f = fixture()
    await f.audio.play('bgm', 'old.wav')
    const ticket = await f.restore.prepare_audio(snapshot())
    const source = { connect: vi.fn(), disconnect: vi.fn(), stop: vi.fn(), start: vi.fn(() => { throw new Error('start failed') }) }
    f.ctx.createBufferSource.mockReturnValueOnce(source)
    expect(() => f.restore.apply_audio(ticket)).toThrow(/start failed/)
    expect(source.disconnect).toHaveBeenCalledOnce()
    expect(f.restore.capture_audio()).toEqual(snapshot(false))
    expect(() => f.restore.apply_audio(ticket)).toThrow(/consumed|unavailable/i)
  })

  it('dispose rejects late preparation and retires all unconsumed tickets', async () => {
    const f = fixture()
    const ticket = await f.restore.prepare_audio(snapshot())
    const decode = deferred()
    f.ctx.decodeAudioData.mockImplementationOnce(() => decode.promise)
    const inflight = f.restore.prepare_audio(snapshot())
    await vi.waitFor(() => expect(f.ctx.decodeAudioData).toHaveBeenCalledTimes(2))
    f.restore.dispose()
    decode.resolve(f.buffer)
    await expect(inflight).rejects.toThrow(/closed|disposed/i)
    expect(() => f.restore.apply_audio(ticket)).toThrow(/closed|consumed|unavailable/i)
    expect(() => f.restore.discard_audio(ticket)).not.toThrow()
    await expect(f.restore.prepare_audio(snapshot(false))).rejects.toThrow(/closed|disposed/i)
    expect(f.sources).toHaveLength(0)
  })

  it('rejects a ticket decoded for a destroyed audio context', async () => {
    const f = fixture()
    const ticket = await f.restore.prepare_audio(snapshot())
    f.audio.destroy()
    expect(() => f.restore.apply_audio(ticket)).toThrow(/context|expired/i)
    expect(f.sources).toHaveLength(0)
  })

  it.each(['dispose', 'destroy'])('rejects late restore fetches after %s without creating or decoding a context', async operation => {
    const f = fixture()
    const response = deferred()
    const constructor = vi.fn(function () { return { ...f.ctx, state: 'running' } })
    vi.stubGlobal('AudioContext', constructor)
    f.fetchImpl.mockImplementationOnce(() => response.promise)
    const preparation = f.restore.prepare_audio(snapshot())
    if (operation === 'dispose') f.restore.dispose()
    else f.audio.destroy()
    response.resolve(new Response(new Uint8Array([1, 2, 3, 4])))
    await expect(preparation).rejects.toThrow(/closed|expired/i)
    expect(constructor).not.toHaveBeenCalled()
    expect(f.ctx.decodeAudioData).not.toHaveBeenCalled()
    expect(f.sources).toHaveLength(0)
  })
})

describe('ordinary WebAudio request generations', () => {
  it.each(['stop', 'stopAll', 'destroy'])('never starts late fetch/decode work after %s', async operation => {
    const f = fixture()
    const decode = deferred()
    f.ctx.decodeAudioData.mockImplementationOnce(() => decode.promise)
    const play = f.audio.play('bgm', 'late.wav')
    await vi.waitFor(() => expect(f.ctx.decodeAudioData).toHaveBeenCalledOnce())
    expect(() => f.restore.capture_audio()).toThrow(/pending/i)
    if (operation === 'stop') f.audio.stop('bgm')
    else f.audio[operation]()
    decode.resolve(f.buffer)
    expect(await play).toBe(false)
    expect(f.sources).toHaveLength(0)
    expect(f.restore.capture_audio()).toEqual(snapshot(false))
  })

  it('a slow old fetch cannot replace the newer BGM or its gain', async () => {
    const f = fixture()
    const response = deferred()
    f.fetchImpl.mockImplementationOnce(() => response.promise)
    const old = f.audio.play('bgm', 'slow.wav', { volume: 0.2 })
    expect(await f.audio.play('bgm', 'new.wav', { volume: 0.9, loop: true })).toBe(true)
    response.resolve(new Response(new Uint8Array([1, 2, 3, 4])))
    expect(await old).toBe(false)
    expect(f.sources).toHaveLength(1)
    expect(f.restore.capture_audio().bgm).toMatchObject({ path: 'new.wav', gain: 0.9, looping: true })
    expect(f.sources[0].stop).not.toHaveBeenCalled()
  })

  it.each(['stop', 'stopAll', 'destroy'])('invalidates an ordinary fetch before decoding after %s', async operation => {
    const f = fixture()
    const response = deferred()
    f.fetchImpl.mockImplementationOnce(() => response.promise)
    const play = f.audio.play('bgm', 'fetch-pending.wav')
    if (operation === 'stop') f.audio.stop('bgm')
    else f.audio[operation]()
    response.resolve(new Response(new Uint8Array([1, 2, 3, 4])))
    expect(await play).toBe(false)
    expect(f.sources).toHaveLength(0)
    expect(f.restore.capture_audio()).toEqual(snapshot(false))
    if (operation === 'destroy') expect(f.ctx.decodeAudioData).not.toHaveBeenCalled()
  })

  it('preserves volumes configured before the AudioContext exists', async () => {
    const f = fixture()
    const audio = new AudioEngine({ fetchImpl: f.fetchImpl })
    audio.setBusVolume('bgm', 0.25)
    vi.stubGlobal('AudioContext', function () { return f.ctx })
    await audio.play('bgm', 'lazy.wav', { volume: 0.8, loop: true })
    expect(audio._busGains.get('bgm').gain.value).toBe(0.25)
    expect(audio.captureBgm().gain).toBe(0.8)
  })
})
