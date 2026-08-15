// G5 web player - audio engine. Wraps WebAudio for real playback;
// degrades to a state machine when no AudioContext exists (jsdom/tests).

export class AudioEngine {
  constructor(opts = {}) {
    this._ctx = opts.ctx ?? null
    this._buffers = new Map()
    this._sources = new Map()
    this._busGains = new Map()
    this.ready = false
    this._init()
  }

  _init() {
    if (this._ctx) {
      for (const kind of ['bgm', 'se', 'voice']) {
        const g = this._ctx.createGain()
        g.gain.value = 1
        g.connect(this._ctx.destination)
        this._busGains.set(kind, g)
      }
      this.ready = true
    }
  }
  ensureContext() {
    if (this._ctx) return this._ctx
    const Ctor = globalThis.AudioContext || globalThis.webkitAudioContext
    if (!Ctor) return null
    this._ctx = new Ctor()
    this._init()
    return this._ctx
  }

  async _load(path, assetUrl) {
    if (!this._ctx) return null
    if (this._buffers.has(path)) return this._buffers.get(path)
    const url = typeof assetUrl === 'function' ? assetUrl(path) : assetUrl + path
    const p = (async () => {
      const res = await fetch(url)
      if (!res.ok) throw new Error('audio fetch ' + res.status + ' ' + url)
      const buf = await res.arrayBuffer()
      return await this._ctx.decodeAudioData(buf)
    })()
    this._buffers.set(path, p)
    try {
      const buf = await p
      return buf
    } catch {
      // load/decode failure: drop the failed entry so a later play(path)
      // can retry, and signal "unavailable" to the caller instead of
      // surfacing the rejection (graceful degradation).
      this._buffers.delete(path)
      return null
    }
  }
  async play(kind, path, opts = {}) {
    const ctx = this.ensureContext()
    if (!ctx) return false
    const buffer = await this._load(path, opts.assetUrl)
    if (!buffer) return false
    this.stop(kind)
    const source = ctx.createBufferSource()
    source.buffer = buffer
    source.loop = !!opts.loop
    const bus = this._busGains.get(kind) ?? ctx.destination
    source.connect(bus)
    if (opts.volume != null) {
      const v = Number(opts.volume)
      if (Number.isFinite(v)) bus.gain.value = v
    }
    // When WebAudio signals the clip has naturally ended, drop it from the
    // active map so isPlaying reflects the end without waiting on the clock
    // (and so a later play(same kind) is not fought by a stale entry).
    source.onended = () => {
      const cur = this._sources.get(kind)
      if (cur && cur.source === source) this._sources.delete(kind)
    }
    source.start()
    this._sources.set(kind, { source, gain: bus, started: ctx.currentTime, duration: buffer.duration })
    return true
  }

  stop(kind) {
    const s = this._sources.get(kind)
    if (s) {
      try { s.source.stop() } catch { }
      try { s.source.disconnect() } catch { }
      this._sources.delete(kind)
    }
  }

  isPlaying(kind) {
    const s = this._sources.get(kind)
    if (!s) return false
    const elapsed = this._ctx.currentTime - s.started
    return elapsed < s.duration
  }

  setBusVolume(kind, v) {
    const g = this._busGains.get(kind)
    if (!g) return
    const n = Number(v)
    g.gain.value = Number.isFinite(n) ? n : 1
  }

  stopAll() { for (const k of [...this._sources.keys()]) this.stop(k) }

  // ---- lifecycle (round 83 tests) ----
  /** Suspend the underlying AudioContext. */
  suspend() {
    if (this._ctx && typeof this._ctx.suspend === 'function') return this._ctx.suspend()
    return undefined
  }
  /** Resume the underlying AudioContext. */
  resume() {
    if (this._ctx && typeof this._ctx.resume === 'function') return this._ctx.resume()
    return undefined
  }
  /** Tear down: stop sources, close the context, clear state. Later calls
   *  degrade safely (play returns false, stop no-ops). */
  destroy() {
    this.stopAll()
    try { if (this._ctx && typeof this._ctx.close === 'function') this._ctx.close() } catch { }
    this._ctx = null
    this.ready = false
    this._busGains.clear()
    this._buffers.clear()
  }
}
