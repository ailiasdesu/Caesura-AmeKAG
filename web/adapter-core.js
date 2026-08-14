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
        opacity: opts.opacity ?? 1,
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

  setLayerImage(node, texId) {
    if (!node) return
    node.texture = texId ?? null
    this._log('layer.image', { name: node.name, tex: texId })
  }

  setLayerVisible(node, v) { if (node) { node.visible = !!v; this._log('layer.visible', { name: node.name, v: !!v }) } }
  setLayerOpacity(node, v) { if (node) { node.opacity = Number(v) ?? 1; this._log('layer.opacity', { name: node.name, v: node.opacity }) } }
  setLayerZ(node, z) { if (node) { node.z = Number(z) ?? 0; this._log('layer.z', { name: node.name, z: node.z }) } }
  moveLayer(node, x, y) { if (node) { node.x = Number(x) ?? node.x; node.y = Number(y) ?? node.y; this._log('layer.move', { name: node.name, x: node.x, y: node.y }) } }

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
  clearText() { this.textBuffer = ''; this._log('text.clear') }
  appendText(s) { if (s) { this.textBuffer += s; this._log('text.append', { len: s.length }) } }
  setText(s) { this.textBuffer = String(s ?? ''); this._log('text.set', { len: this.textBuffer.length }) }

  // -- audio ------------------------------------------------------------
  audioPlay(kind, path, volume = 1) {
    this.audioBus[kind] = { path, volume: Number(volume) ?? 1, playing: true }
    this._log('audio.play', { kind, path, volume })
  }
  audioStop(kind) { if (this.audioBus[kind]) this.audioBus[kind].playing = false; this._log('audio.stop', { kind }) }
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
