// G5 path-B web player — binding adapter CORE (pure state machine, no DOM).
// backend.* / layers.* semantics as observed by the Lua command handlers
// (spike4 contract): add_layer returns a node table, Type is a constant
// table, get returns null when absent, load_texture returns an id.

export class AdapterCore {
  constructor() {
    /** layer name -> node {id,name,x,y,w,h,visible,opacity,z,texture} */
    this.layers = new Map()
    this.textures = new Map() // id -> {path, loaded, width, height}
    this.textBuffer = ''
    this.draws = []
    this.backlog = []
    this._lastBacklog = ''
    this.audioBus = { bgm: null, se: [], voice: null }
    this.events = [] // call log for tests/telemetry
    this._seq = 0
  }

  // -- layers -----------------------------------------------------------
  ensureLayer(name, opts = {}) {
    let n = this.layers.get(name)
    if (!n) {
      n = {
        id: ++this._seq,
        name,
        x: opts.x ?? 0, y: opts.y ?? 0,
        w: opts.w ?? 1280, h: opts.h ?? 720,
        visible: opts.visible ?? true,
        opacity: opts.opacity ?? 255, // engine semantics: 0..255
        z: opts.z ?? 0,
        texture: null,
        layerType: opts.layer_type ?? 0,
      }
      this.layers.set(name, n)
      this._log('layer.create', { name, z: n.z })
    }
    return n
  }

  getLayer(name) { return this.layers.get(name) ?? null }

  removeLayer(name) {
    if (this.layers.delete(name)) this._log('layer.remove', { name })
  }

  /** Resolve the canonical node: Lua-side proxies may wrap the same layer
   *  under a different JS reference, so mutators always write the node
   *  stored in the Map (keyed by name). */
  _canon(node) {
    if (!node) return null
    const hit = this.layers.get(node.name)
    return hit ?? node
  }

  setLayerImage(node, texId) {
    const n = this._canon(node)
    if (!n) return
    n.texture = texId ?? null
    this._log('layer.image', { name: n.name, tex: texId })
  }

  setLayerVisible(node, v) { const n = this._canon(node); if (n) { n.visible = !!v; this._log('layer.visible', { name: n.name, v: !!v }) } }
  setLayerOpacity(node, v) { const n = this._canon(node); if (n) { n.opacity = Number(v) ?? 255; n._lastMutateAt = this._frame ?? 0; this._log('layer.opacity', { name: n.name, v: n.opacity }) } }
  setLayerZ(node, z) { const n = this._canon(node); if (n) { n.z = Number(z) ?? 0; n._lastMutateAt = this._frame ?? 0; this._log('layer.z', { name: n.name, z: n.z }) } }
  moveLayer(node, x, y) { const n = this._canon(node); if (n) { n.x = Number(x) ?? n.x; n.y = Number(y) ?? n.y; n._lastMutateAt = this._frame ?? 0; this._log('layer.move', { name: n.name, x: n.x, y: n.y }) } }

  // -- textures ---------------------------------------------------------
  loadTexture(path) {
    if (!path) return 0
    let id = 0
    for (const [k, v] of this.textures) if (v.path === path) { id = k; break }
    if (!id) { id = ++this._seq; this.textures.set(id, { path, loaded: false, width: 0, height: 0 }) }
    this._log('texture.load', { path, id })
    return id
  }
  markTextureLoaded(id, width, height) {
    const t = this.textures.get(id)
    if (t) { t.loaded = true; t.width = width ?? 0; t.height = height ?? 0; this._log('texture.ready', { id, w: t.width, h: t.height }) }
  }
  destroyTexture(id) { this.textures.delete(id); this._log('texture.destroy', { id }) }

  // -- text -------------------------------------------------------------
  clearText() { this.textBuffer = ''; this.draws = []; this._log('text.clear') }
  appendText(s) { if (s) { this.textBuffer += s; this._log('text.append', { len: s.length }) } }
  setText(s) { this.textBuffer = String(s ?? ''); this._log('text.set', { len: this.textBuffer.length }) }
  /** Structured text draws from the Lua TextScene (x/y/rgb/scale/bold). */
  setDraws(draws) {
    this.draws = Array.isArray(draws) ? draws : []
    // backlog: snapshot non-empty pages so the UI can scroll back (VN
    // convention: every [p]/[er] commits the current page to history).
    const text = this.draws.map((d) => d.t).join('')
    if (text.trim().length > 0 && text !== this._lastBacklog) {
      this.backlog.push({ draws: this.draws.map((d) => ({ ...d })), text })
      this._lastBacklog = text
      this._log('backlog.add', { n: this.backlog.length })
    }
    this._log('text.draws', { n: this.draws.length })
  }

  // -- audio ------------------------------------------------------------
  audioPlay(kind, path, volume = 1) {
    this.audioBus[kind] = { path, volume: Number(volume) ?? 1, playing: true }
    this._log('audio.play', { kind, path, volume })
  }
  audioStop(kind) { if (this.audioBus[kind]) this.audioBus[kind].playing = false; this._log('audio.stop', { kind }) }
  audioSetBusVolume(kind, v) { this._log('audio.volume', { kind, v: Number(v) ?? 1 }) }
  /** Host callback: a clip actually ended (AudioContext onended). */
  audioEnded(kind) { if (this.audioBus[kind]) this.audioBus[kind].playing = false; this._log('audio.ended', { kind }) }
  audioIsPlaying(kind) {
    const e = this.audioBus[kind]
    if (!e || !e.playing) return false
    // Web player: a clip is treated as finished after its simulated
    // duration (real impl uses AudioContext currentTime).
    const elapsed = (this._frame ?? 0) - (e._startFrame ?? 0)
    return elapsed < (e._durationFrames ?? 120)
  }

  // -- z-sorted render list (canvas/DOM both consume this) --------------
  renderList() {
    return [...this.layers.values()]
      .filter((n) => n.visible && n.texture)
      .sort((a, b) => a.z - b.z)
  }

  _log(kind, detail) { this.events.push({ kind, detail, t: this.events.length }) }
  /** Host frame tick (browser rAF / test driver). */
  tick() { this._frame = (this._frame ?? 0) + 1 }
}

// Lua-facing constants mirroring the C++ layer type enum (subset).
export const LAYER_TYPE = { LAYER_NORMAL: 0, LAYER_MESSAGE: 1, LAYER_BG: 2, LAYER_FG: 3 }
