// G5 path-B web player — DOM renderer for AdapterCore.
// Renders the layer tree into a container element: each visible layer
// with a texture becomes a positioned <img>; the message layer shows
// text. jsdom-testable (no real browser APIs beyond DOM).

// Layer tag -> element kind: image-bearing layers render as <img>;
// message/ui layers as overlay <div>.
const IMAGE_LAYER_TAGS = new Set(['bg', 'fg', 'layer0', 'layer1', 'fore', '_char_', 'image'])

// Web-side LUT color-grading: map the active palette (core.palette) to a
// CSS filter on the render container. This is the DOM render path analog of
// the desktop backend.set_palette binding (s_lutTex applied to future
// submits). handle==null (day/neutral) -> none; otherwise a blue-dark
// "night" grade whose strength follows intensity 0..1.
function paletteFilter(palette) {
  if (!palette || palette.handle == null || !(Number(palette.intensity) > 0)) return ''
  const t = Math.min(1, Math.max(0, Number(palette.intensity) || 0))
  // night = cool, dim blue cast (brightness down, blue/sepia grade up
  // with t). Linear in intensity so day<->night toggle visibly shifts.
  const b = Math.round(16 * t)
  const h = Math.round(198 * t)
  const sat = 1 + 0.25 * t
  return 'brightness(' + (1 - 0.18 * t).toFixed(3) + ') sepia(' + b + '%) hue-rotate(' + h + 'deg) saturate(' + sat.toFixed(3) + ')'
}

export class DomRenderer {
  constructor(core, rootEl, opts = {}) {
    this.core = core
    this.root = rootEl
    this.width = opts.width ?? 1280
    this.height = opts.height ?? 720
    this.textureUrls = new Map() // id -> src string
    this._els = new Map() // layer name -> element
    this._textEl = null
    /** Optional external layer source (Lua Layers.snapshot()); when set it
     *  takes precedence over core.renderList(). */
    this.getLayers = opts.getLayers ?? null
    this._subscribe()
  }

  _subscribe() {
    const render = () => this.render()
    // The core is driven synchronously by Lua; we re-render on every
    // call via a trailing microtask (batches multi-step commands).
    this._flush = render
  }

  /** Set the URL for a texture id (img src resolution). */
  setTextureUrl(id, url) { this.textureUrls.set(id, url) }

  /** Full re-render: sync DOM to core state. Cheap for demo sizes. */
  async render() {
    const alive = new Set()
    // Web-side color grading: the active LUT (backend.set_palette ->
    // core.palette) tints the whole render output via a CSS filter, the
    // DOM analog of the desktop s_lutTex/u_paletteParams binding. day/
    // neutral (handle null) applies nothing; night applies a blue-dark
    // grade scaled by intensity.
    this.root.style.filter = paletteFilter(this.core.palette)
    const list = this.getLayers ? await this.getLayers() : this.core.renderList()
    // text layer rendered separately (overlay) if it has content
    for (const n of list) {
      let el = this._els.get(n.name)
      const isImage = IMAGE_LAYER_TAGS.has(n.name) || (n.tag && IMAGE_LAYER_TAGS.has(n.tag)) ||
        n.name.startsWith('_char_') || (n.tag && n.tag.startsWith('_char_'))
      if (!el || (isImage && el.tagName !== 'IMG') || (!isImage && el.tagName === 'IMG')) {
        if (el) el.remove()
        el = document.createElement(isImage ? 'img' : 'div')
        el.className = 'caesura-layer'
        el.dataset.layer = n.name
        el.style.position = 'absolute'
        this.root.appendChild(el)
        this._els.set(n.name, el)
      }
      // CSS transitions animate engine-driven moves/fades (sprite_move /
      // sprite_fade yield per frame; the DOM sees the endpoint).
      el.style.transition = 'left 300ms linear, top 300ms linear, opacity 300ms linear'
      el.style.left = n.x + 'px'
      el.style.top = n.y + 'px'
      el.style.width = n.w + 'px'
      el.style.height = n.h + 'px'
      // engine opacity is 0..255; DOM wants 0..1
      el.style.opacity = String((Number(n.opacity) || 255) / 255)
      el.style.zIndex = String(n.z)
      const url = n.texture ? this.textureUrls.get(n.texture) : null
      if (el.tagName === 'IMG') {
        if (url) el.setAttribute('src', url)
        else el.removeAttribute('src')
      } else {
        el.textContent = url ? '' : ''
      }
      alive.add(n.name)
    }
    // remove stale layer elements
    for (const [name, el] of this._els) {
      if (!alive.has(name) && name !== '_message') {
        el.remove()
        this._els.delete(name)
      }
    }
    // message overlay — structured draws (x/y/rgb/scale) when available,
    // else the flat textBuffer (legacy fallback).
    const draws = this.core.draws ?? []
    const hasText = draws.length > 0 || this.core.textBuffer.length > 0
    if (hasText && !this._textEl) {
      this._textEl = document.createElement('div')
      this._textEl.className = 'caesura-message'
      this._textEl.style.position = 'absolute'
      this._textEl.style.left = '0'
      this._textEl.style.top = '0'
      this._textEl.style.width = '100%'
      this._textEl.style.height = '100%'
      this._textEl.style.pointerEvents = 'none'
      this.root.appendChild(this._textEl)
    }
    if (this._textEl) {
      this._textEl.textContent = ''
      if (draws.length > 0) {
        for (const d of draws) {
          const span = document.createElement('span')
          span.textContent = d.t
          span.style.position = 'absolute'
          span.style.left = d.x + 'px'
          span.style.top = d.y + 'px'
          span.style.color = 'rgb(' + d.r + ',' + d.g + ',' + d.b + ')'
          span.style.fontSize = Math.round(20 * (d.s || 1)) + 'px'
          if (d.bd) span.style.fontWeight = '700'
          if (d.it) span.style.fontStyle = 'italic'
          this._textEl.appendChild(span)
        }
      } else {
        // flat fallback: bottom text box
        const box = document.createElement('div')
        box.textContent = this.core.textBuffer
        box.style.position = 'absolute'
        box.style.left = '0'
        box.style.right = '0'
        box.style.bottom = '0'
        box.style.padding = '16px 24px'
        box.style.background = 'rgba(0,0,0,0.55)'
        box.style.color = '#fff'
        // inherit the player font stack (@font-face CaesuraNoto + fallbacks);
        // a hard-coded system-ui here would override the W3 CJK font.
        box.style.fontFamily = ''
        box.style.fontSize = '20px'
        box.style.whiteSpace = 'pre-wrap'
        this._textEl.appendChild(box)
      }
    }
    if (!hasText && this._textEl) { this._textEl.remove(); this._textEl = null }
  }

  destroy() {
    for (const el of this._els.values()) el.remove()
    this._els.clear()
    if (this._textEl) { this._textEl.remove(); this._textEl = null }
  }
}