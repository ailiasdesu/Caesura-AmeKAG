// G5 path-B web player — DOM renderer for AdapterCore.
// Renders the layer tree into a container element: each visible layer
// with a texture becomes a positioned <img>; the message layer shows
// text. jsdom-testable (no real browser APIs beyond DOM).

const LAYER_TAGS = new Map([
  [0, 'div'], [1, 'div'], [2, 'img'], [3, 'img'],
])

export class DomRenderer {
  constructor(core, rootEl, opts = {}) {
    this.core = core
    this.root = rootEl
    this.width = opts.width ?? 1280
    this.height = opts.height ?? 720
    this.textureUrls = new Map() // id -> src string
    this._els = new Map() // layer name -> element
    this._textEl = null
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
  render() {
    const alive = new Set()
    const list = this.core.renderList()
    // text layer rendered separately (overlay) if it has content
    for (const n of list) {
      let el = this._els.get(n.name)
      if (!el) {
        el = document.createElement(LAYER_TAGS.get(n.layerType) ?? 'div')
        el.className = 'caesura-layer'
        el.dataset.layer = n.name
        el.style.position = 'absolute'
        this.root.appendChild(el)
        this._els.set(n.name, el)
      }
      el.style.left = n.x + 'px'
      el.style.top = n.y + 'px'
      el.style.width = n.w + 'px'
      el.style.height = n.h + 'px'
      el.style.opacity = String(n.opacity)
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
    // message overlay
    const hasText = this.core.textBuffer.length > 0
    if (hasText && !this._textEl) {
      this._textEl = document.createElement('div')
      this._textEl.className = 'caesura-message'
      this._textEl.style.position = 'absolute'
      this._textEl.style.left = '0'
      this._textEl.style.right = '0'
      this._textEl.style.bottom = '0'
      this._textEl.style.padding = '16px 24px'
      this._textEl.style.background = 'rgba(0,0,0,0.55)'
      this._textEl.style.color = '#fff'
      this._textEl.style.fontFamily = 'system-ui, sans-serif'
      this._textEl.style.fontSize = '20px'
      this._textEl.style.whiteSpace = 'pre-wrap'
      this.root.appendChild(this._textEl)
    }
    if (this._textEl) this._textEl.textContent = this.core.textBuffer
    if (!hasText && this._textEl) { this._textEl.remove(); this._textEl = null }
  }

  destroy() {
    for (const el of this._els.values()) el.remove()
    this._els.clear()
    if (this._textEl) { this._textEl.remove(); this._textEl = null }
  }
}
