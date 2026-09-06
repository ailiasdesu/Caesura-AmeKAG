// @vitest-environment jsdom
// Decoder injection tests ownership and integration, not browser image decoding.
import { afterEach, describe, expect, it, vi } from 'vitest'
import { AdapterCore } from './adapter-core.js'
import { DomRenderer } from './dom-renderer.js'
import { createAssetRestore, readAssetBytes, MAX_IMAGE_BYTES } from './restore-assets.js'

function response(bytes = new Uint8Array([1, 2, 3]), headers = {}) {
  const reader = {
    read: vi.fn().mockResolvedValueOnce({ value: bytes, done: false }).mockResolvedValue({ done: true }),
    cancel: vi.fn().mockResolvedValue(), releaseLock: vi.fn(),
  }
  return { ok: true, status: 200, headers: { get: key => headers[key] ?? null }, body: { getReader: () => reader }, reader }
}

function harness(options = {}) {
  const core = new AdapterCore()
  const resources = []
  const fetchImpl = options.fetchImpl ?? vi.fn(async () => response())
  const decodeImage = vi.fn(async bytes => {
    const resource = { width: 2, height: 3, draw: vi.fn(), dispose: vi.fn(), bytes: [...bytes] }
    resources.push(resource)
    return resource
  })
  const api = createAssetRestore({ core, fetchImpl, assetUrl: path => '/project/' + path, decodeImage, ...options })
  return { core, resources, fetchImpl, decodeImage, api }
}

afterEach(() => { vi.restoreAllMocks(); vi.unstubAllGlobals() })

describe('prepared image ownership', () => {
  it('reads and decodes before commit without changing the current scene or path cache', async () => {
    const { core, resources, fetchImpl, decodeImage, api } = harness()
    const ordinary = core.loadTexture('assets/bg.png')
    core.setLayerImage(core.ensureLayer('bg'), ordinary)
    const events = [...core.events]
    const ticket = await api.prepare_image('assets/bg.png')
    expect(fetchImpl).toHaveBeenCalledWith('/project/assets/bg.png')
    expect(decodeImage).toHaveBeenCalledTimes(1)
    expect(api.image_info(ticket)).toEqual([2, 3])
    expect(core.events).toEqual(events)
    expect(core.textures.size).toBe(1)
    expect(core.getLayer('bg').texture).toBe(ordinary)

    fetchImpl.mockImplementation(() => { throw new Error('source changed after preparation') })
    const id = api.materialize_image(ticket)
    expect(id).not.toBe(ordinary)
    expect(core.loadTexture('assets/bg.png')).toBe(ordinary)
    expect(core.textures.get(id)).toMatchObject({ loaded: true, width: 2, height: 3 })
    expect(api.describe_texture(id)).toEqual({ kind: 'asset', path: 'assets/bg.png' })
    expect(resources[0].bytes).toEqual([1, 2, 3])
    expect(fetchImpl).toHaveBeenCalledTimes(1)
    expect(() => api.materialize_image(ticket)).toThrow(/unavailable|consumed/i)
    expect(() => api.image_info(ticket)).toThrow(/unavailable|consumed/i)
    api.discard_image(ticket)
    expect(resources[0].dispose).not.toHaveBeenCalled()
    core.destroyTexture(id)
    core.destroyTexture(id)
    expect(resources[0].dispose).toHaveBeenCalledTimes(1)
    expect(core.textures.has(ordinary)).toBe(true)
  })

  it('never returns a restore-owned id from the ordinary path cache', async () => {
    const { core, api } = harness()
    const a = api.materialize_image(await api.prepare_image('a.png'))
    const b = api.materialize_image(await api.prepare_image('a.png'))
    const ordinary = core.loadTexture('a.png')
    expect(new Set([a, b, ordinary]).size).toBe(3)
    expect(core.loadTexture('a.png')).toBe(ordinary)
    core.destroyTexture(ordinary)
    expect(core.loadTexture('a.png')).not.toBe(a)
    expect(api.describe_texture(ordinary)).toBeNull()
  })

  it('discards once and rejects forged or foreign tickets', async () => {
    const { resources, api } = harness()
    const ticket = await api.prepare_image('a.png')
    api.discard_image(ticket)
    api.discard_image(ticket)
    api.discard_image({})
    expect(resources[0].dispose).toHaveBeenCalledTimes(1)
    expect(() => api.materialize_image(ticket)).toThrow()
    expect(() => api.materialize_image({})).toThrow()
    expect(() => harness().api.image_info(ticket)).toThrow()
  })

  it('releases a consumed resource if publication fails', async () => {
    const { core, resources, api } = harness()
    const ticket = await api.prepare_image('a.png')
    vi.spyOn(core, 'registerPreparedTexture').mockImplementation(() => { throw new Error('upload failed') })
    expect(() => api.materialize_image(ticket)).toThrow('upload failed')
    api.discard_image(ticket)
    expect(resources[0].dispose).toHaveBeenCalledTimes(1)
    expect(core.textures.size).toBe(0)
  })

  it('rolls back a texture registration that fails while publishing its event', async () => {
    const { core, api, resources } = harness()
    const ticket = await api.prepare_image('a.png')
    vi.spyOn(core, '_log').mockImplementationOnce(() => { throw new Error('publication failed') })
    expect(() => api.materialize_image(ticket)).toThrow('publication failed')
    expect(core.textures.size).toBe(0)
    expect(resources[0].dispose).toHaveBeenCalledTimes(1)
  })

  it('captures ordinary sources without exposing mutable metadata', async () => {
    const { core, api } = harness()
    const id = core.loadTexture('a.png')
    const description = api.describe_texture(id)
    description.path = 'other.png'
    expect(api.describe_texture(id)).toEqual({ kind: 'asset', path: 'a.png' })
    expect(api.describe_texture(0)).toBeNull()
  })
})

describe('bounded preparation failures', () => {
  it.each(['', '../a.png', '/a.png', 'a\\b.png', 'https://site/a.png', 'a\0.png', 12])('rejects invalid path %s before fetch', async path => {
    const { api, fetchImpl } = harness()
    await expect(api.prepare_image(path)).rejects.toThrow(/path/i)
    expect(fetchImpl).not.toHaveBeenCalled()
  })

  it('rejects a failed fetch and a corrupt or unsupported decode without mutation', async () => {
    const { core, api, fetchImpl, decodeImage } = harness()
    fetchImpl.mockResolvedValueOnce({ ok: false, status: 404 })
    await expect(api.prepare_image('missing.png')).rejects.toThrow(/404/)
    decodeImage.mockRejectedValueOnce(new Error('Unsupported image data'))
    await expect(api.prepare_image('broken.png')).rejects.toThrow(/Unsupported/)
    expect(core.textures.size).toBe(0)
    expect(core.events).toHaveLength(0)
  })

  it('rejects a known oversized body without reading or decoding it', async () => {
    const body = response(undefined, { 'content-length': String(MAX_IMAGE_BYTES + 1) })
    const { api, decodeImage } = harness({ fetchImpl: async () => body })
    await expect(api.prepare_image('big.png')).rejects.toThrow(/size limit/)
    expect(body.reader.read).not.toHaveBeenCalled()
    expect(body.reader.cancel).toHaveBeenCalledTimes(1)
    expect(body.reader.releaseLock).toHaveBeenCalledTimes(1)
    expect(decodeImage).not.toHaveBeenCalled()
  })

  it('enforces streamed bytes even if content-length lies', async () => {
    const body = response(new Uint8Array(MAX_IMAGE_BYTES + 1), { 'content-length': '3' })
    const { api, decodeImage } = harness({ fetchImpl: async () => body })
    await expect(api.prepare_image('big.png')).rejects.toThrow(/size limit/)
    expect(body.reader.cancel).toHaveBeenCalledTimes(1)
    expect(body.reader.releaseLock).toHaveBeenCalledTimes(1)
    expect(decodeImage).not.toHaveBeenCalled()
  })

  it('cancels an interrupted read and rejects empty or non-streaming responses', async () => {
    const body = response()
    body.reader.read.mockReset().mockRejectedValueOnce(new Error('connection lost'))
    const { api, fetchImpl, decodeImage } = harness({ fetchImpl: vi.fn(async () => body) })
    await expect(api.prepare_image('a.png')).rejects.toThrow('connection lost')
    expect(body.reader.cancel).toHaveBeenCalledTimes(1)
    expect(body.reader.releaseLock).toHaveBeenCalledTimes(1)
    fetchImpl.mockResolvedValueOnce(response(new Uint8Array()))
    await expect(api.prepare_image('a.png')).rejects.toThrow(/empty/i)
    fetchImpl.mockResolvedValueOnce({ ok: true })
    await expect(api.prepare_image('a.png')).rejects.toThrow(/stream/i)
    expect(decodeImage).not.toHaveBeenCalled()
  })

  it.each([[0, 3], [1.5, 3], [16385, 1], [8192, 8192], [NaN, 3]])('closes invalid decoded dimensions %s by %s', async (width, height) => {
    const decoded = { width, height, draw() {}, dispose: vi.fn() }
    const { api, core } = harness({ decodeImage: async () => decoded })
    await expect(api.prepare_image('a.png')).rejects.toThrow(/dimensions|size limit/i)
    expect(decoded.dispose).toHaveBeenCalledTimes(1)
    expect(core.textures.size).toBe(0)
  })

  it('requires a real browser decoder when no explicit decoder is supplied', async () => {
    vi.stubGlobal('createImageBitmap', undefined)
    const { api } = harness({ decodeImage: undefined })
    await expect(api.prepare_image('a.png')).rejects.toThrow(/decoder unavailable/i)
  })

  it('passes an immutable Blob to the browser decoder and closes the bitmap on discard', async () => {
    const image = { width: 4, height: 2, close: vi.fn() }
    const decode = vi.fn(async () => image)
    vi.stubGlobal('createImageBitmap', decode)
    const { api, core } = harness({ decodeImage: undefined })
    const ticket = await api.prepare_image('a.png')
    expect(decode.mock.calls[0][0]).toBeInstanceOf(Blob)
    expect(decode.mock.calls[0][0].size).toBe(3)
    const id = api.materialize_image(ticket)
    const context = { drawImage: vi.fn() }
    core.textures.get(id).prepared.draw(context)
    expect(context.drawImage).toHaveBeenCalledWith(image, 0, 0)
    core.destroyTexture(id)
    expect(image.close).toHaveBeenCalledTimes(1)
  })

  it('shares the same bounded reader with non-image resources', async () => {
    const bytes = new Uint8Array([3, 4, 5])
    const result = await readAssetBytes('fonts/main.ttf', { fetchImpl: async () => response(bytes), maxBytes: 3 })
    bytes[0] = 9
    expect([...result]).toEqual([3, 4, 5])
    await expect(readAssetBytes('music.ogg', { fetchImpl: async () => response(), maxBytes: 2 })).rejects.toThrow(/size limit/)
    await expect(readAssetBytes('../font.ttf')).rejects.toThrow(/path/)
    await expect(readAssetBytes('font.ttf', { maxBytes: Infinity })).rejects.toThrow(/limit/)
    await expect(readAssetBytes('字'.repeat(1400) + '.ttf')).rejects.toThrow(/path/)
  })

  it('copies streamed chunks before a reader reuses their backing buffer', async () => {
    const chunk = new Uint8Array([1, 2])
    const body = response()
    body.reader.read.mockReset()
      .mockImplementationOnce(async () => ({ value: chunk, done: false }))
      .mockImplementationOnce(async () => { chunk[0] = 3; return { value: chunk, done: false } })
      .mockResolvedValue({ done: true })
    const result = await readAssetBytes('a.png', { fetchImpl: async () => body, maxBytes: 4 })
    expect([...result]).toEqual([1, 2, 3, 2])
  })

  it('disposes pending tickets but leaves materialized textures under core ownership', async () => {
    const { api, core, resources } = harness()
    const pending = await api.prepare_image('pending.png')
    const id = api.materialize_image(await api.prepare_image('live.png'))
    api.dispose()
    api.dispose()
    expect(resources[0].dispose).toHaveBeenCalledTimes(1)
    expect(resources[1].dispose).not.toHaveBeenCalled()
    expect(() => api.materialize_image(pending)).toThrow()
    expect(() => api.prepare_color(0, 0, 0, 255)).toThrow(/closed/)
    await expect(api.prepare_image('next.png')).rejects.toThrow(/closed/)
    core.destroyTexture(id)
    expect(resources[1].dispose).toHaveBeenCalledTimes(1)
  })

  it('attempts every discard even if one resource release throws', async () => {
    const { api, resources } = harness()
    const a = await api.prepare_image('a.png')
    const b = await api.prepare_image('b.png')
    resources[0].dispose.mockImplementation(() => { throw new Error('close failed') })
    expect(() => api.dispose()).toThrow(/discard/)
    expect(resources[1].dispose).toHaveBeenCalledTimes(1)
    api.discard_image(a)
    api.discard_image(b)
    expect(resources[0].dispose).toHaveBeenCalledTimes(1)
  })

  it('releases a decode that completes after the preparation session closes', async () => {
    let resolve
    const decoded = { width: 1, height: 1, draw() {}, dispose: vi.fn() }
    const decoderStarted = Promise.withResolvers()
    const { api } = harness({ decodeImage: () => {
      decoderStarted.resolve()
      return new Promise(done => { resolve = done })
    } })
    const preparation = api.prepare_image('a.png')
    await decoderStarted.promise
    api.dispose()
    resolve(decoded)
    await expect(preparation).rejects.toThrow(/closed/)
    expect(decoded.dispose).toHaveBeenCalledTimes(1)
  })
})

describe('prepared resources reach the DOM', () => {
  it('draws the prepared image without linking its old path and keeps zero opacity', async () => {
    const { core, api, resources } = harness()
    const context = { clearRect: vi.fn() }
    vi.spyOn(HTMLCanvasElement.prototype, 'getContext').mockReturnValue(context)
    const root = document.createElement('div')
    const renderer = new DomRenderer(core, root)
    const id = api.materialize_image(await api.prepare_image('original.png'))
    core.setLayerImage(core.ensureLayer('custom-layer', { opacity: 0 }), id)
    renderer.setTextureUrl(id, '/future/incorrect.png')
    await renderer.render()
    const canvas = root.querySelector('canvas[data-layer="custom-layer"]')
    expect(canvas).not.toBeNull()
    expect(canvas.width).toBe(2)
    expect(canvas.height).toBe(3)
    expect(canvas.style.opacity).toBe('0')
    expect(resources[0].draw).toHaveBeenCalledWith(context)
    expect(root.querySelector('img')).toBeNull()
    await renderer.render()
    expect(resources[0].draw).toHaveBeenCalledTimes(1)
    core.destroyTexture(id)
    await renderer.render()
    expect(root.querySelector('canvas')).toBeNull()
    expect(root.querySelector('[src]')).toBeNull()
  })

  it('prepares transparent colors without fetching and renders exact RGBA', async () => {
    const { core, api, fetchImpl } = harness()
    const context = { fillRect: vi.fn() }
    vi.spyOn(HTMLCanvasElement.prototype, 'getContext').mockReturnValue(context)
    const ticket = api.prepare_color(20, 40, 60, 0)
    expect(api.image_info(ticket)).toEqual([1, 1])
    const id = api.materialize_image(ticket)
    expect(api.describe_texture(id)).toEqual({ kind: 'color', r: 20, g: 40, b: 60, a: 0 })
    core.setLayerImage(core.ensureLayer('bg'), id)
    const root = document.createElement('div')
    await new DomRenderer(core, root).render()
    expect(context.fillStyle).toBe('rgba(20,40,60,0)')
    expect(context.fillRect).toHaveBeenCalledWith(0, 0, 1, 1)
    expect(fetchImpl).not.toHaveBeenCalled()
  })

  it('renders distinct restored layer identities even when their display names match', async () => {
    const { core, api } = harness()
    vi.spyOn(HTMLCanvasElement.prototype, 'getContext').mockReturnValue({ fillRect() {} })
    const a = api.materialize_image(api.prepare_color(255, 0, 0, 255))
    const b = api.materialize_image(api.prepare_color(0, 0, 255, 255))
    core.installPreparedLayers([
      { id: '_root' },
      { id: 'first', name: 'same', parent: '_root', image: { id: a } },
      { id: 'second', name: 'same', parent: '_root', image: { id: b } },
    ])
    const root = document.createElement('div')
    const renderer = new DomRenderer(core, root)
    await renderer.render()
    expect(root.querySelectorAll('canvas[data-layer="same"]')).toHaveLength(2)
    core.removeLayer('first')
    await renderer.render()
    expect(root.querySelectorAll('canvas[data-layer="same"]')).toHaveLength(1)
  })

  it.each([-1, 256, 0.5, NaN, '1'])('rejects invalid color component %s', value => {
    expect(() => harness().api.prepare_color(value, 0, 0, 255)).toThrow(/color/i)
  })
})
