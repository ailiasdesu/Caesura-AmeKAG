// Web implementation of the shared LayerState image preparation contract.
// Preparation owns decoded pixels; publication transfers that ownership to a
// distinct texture ID, so a restored scene never inherits the ordinary cache.
export const MAX_IMAGE_BYTES = 64 * 1024 * 1024
const MAX_IMAGE_DIMENSION = 16384

function validPath(path) {
  return typeof path === 'string' && path.length > 0 && path.length <= 4096
    && new TextEncoder().encode(path).byteLength <= 4096
    && !path.startsWith('/') && !path.includes('..') && !/[\x00-\x1f\\:]/.test(path)
}

async function readResponseBytes(response, maxBytes) {
  if (!response?.ok) throw new Error('Asset fetch failed: ' + (response?.status ?? 'unavailable'))
  if (!response.body?.getReader) throw new Error('Asset byte stream unavailable')
  const reader = response.body.getReader()
  try {
    if (Number(response.headers?.get('content-length')) > maxBytes) {
      throw new Error('Asset exceeds encoded size limit')
    }
    let bytes = new Uint8Array(0)
    let size = 0
    while (true) {
      const { done, value } = await reader.read()
      if (done) break
      // Fetch/Response may originate in another realm (iframe or host bridge).
      // The backing view contract, unlike instanceof, survives that boundary.
      if (!ArrayBuffer.isView(value) || value.BYTES_PER_ELEMENT !== 1) throw new Error('Invalid asset byte stream')
      const nextSize = size + value.byteLength
      if (nextSize > maxBytes) throw new Error('Asset exceeds encoded size limit')
      if (nextSize > bytes.byteLength) {
        const capacity = Math.min(maxBytes, Math.max(nextSize, bytes.byteLength * 2, 4096))
        const grown = new Uint8Array(capacity)
        grown.set(bytes)
        bytes = grown
      }
      // Copy before reading again; the reader may reuse its backing buffer.
      // One bounded buffer also avoids unbounded bookkeeping for tiny chunks.
      bytes.set(value, size)
      size = nextSize
    }
    if (size === 0) throw new Error('Asset data is empty')
    return bytes.byteLength === size ? bytes : bytes.slice(0, size)
  } catch (error) {
    try { await reader.cancel() } catch { /* Preserve the original read failure. */ }
    throw error
  } finally {
    reader.releaseLock()
  }
}

/** Shared bounded reader for image, font and audio preparation. Returns an
 *  owned Uint8Array; callers must decode it before committing live state. */
export async function readAssetBytes(path, { fetchImpl = globalThis.fetch, assetUrl = value => value, maxBytes = MAX_IMAGE_BYTES } = {}) {
  if (!validPath(path)) throw new Error('Unsafe asset path')
  if (!Number.isSafeInteger(maxBytes) || maxBytes <= 0 || maxBytes > MAX_IMAGE_BYTES) {
    throw new Error('Invalid asset byte limit')
  }
  return readResponseBytes(await fetchImpl(await assetUrl(path)), maxBytes)
}

async function decodeBrowserImage(bytes) {
  if (typeof globalThis.createImageBitmap !== 'function') throw new Error('Browser image decoder unavailable')
  // No URL or extension decides success: the browser must decode the retained
  // bytes. Unsupported/corrupt formats reject here, before touching live state.
  const image = await globalThis.createImageBitmap(new Blob([bytes]))
  return {
    width: image.width, height: image.height,
    draw: context => context.drawImage(image, 0, 0),
    dispose: () => image.close(),
  }
}

function ownedImage(decoded) {
  let disposed = false
  const resource = {
    width: decoded?.width, height: decoded?.height,
    draw(context) {
      if (disposed) throw new Error('Prepared image is unavailable')
      decoded.draw(context)
    },
    dispose() {
      if (disposed) return
      disposed = true
      decoded?.dispose?.()
    },
  }
  const { width, height } = resource
  if (!Number.isInteger(width) || !Number.isInteger(height) || width <= 0 || height <= 0
      || width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION
      || width * height * 4 > MAX_IMAGE_BYTES || typeof decoded.draw !== 'function'
      || typeof decoded.dispose !== 'function') {
    resource.dispose()
    throw new Error('Invalid decoded image dimensions or pixel size limit')
  }
  return Object.freeze(resource)
}

/** `decodeImage` is an explicit host/test injection returning
 *  {width, height, draw(context), dispose()}; production defaults to actual
 *  browser decoding. image_info returns [width, height] for Lua to unpack. */
export function createAssetRestore({ core, fetchImpl = globalThis.fetch, assetUrl = path => path, decodeImage = decodeBrowserImage }) {
  const tickets = new WeakMap()
  const pending = new Set()
  let closed = false
  function box(resource, source) {
    if (closed) { resource.dispose(); throw new Error('Image preparation session is closed') }
    const ticket = Object.freeze({})
    const state = { resource, source }
    tickets.set(ticket, state)
    pending.add(state)
    return ticket
  }
  function take(ticket) {
    const state = tickets.get(ticket)
    if (!state?.resource) throw new Error('Prepared image is unavailable or consumed')
    return state
  }
  function consume(state) {
    const resource = state.resource
    state.resource = null
    pending.delete(state)
    return resource
  }
  return {
    async prepare_image(path) {
      if (!validPath(path)) throw new Error('Unsafe image path')
      if (closed) throw new Error('Image preparation session is closed')
      const bytes = await readAssetBytes(path, { fetchImpl, assetUrl })
      const resource = ownedImage(await decodeImage(bytes))
      return box(resource, Object.freeze({ kind: 'asset', path }))
    },
    prepare_color(r, g, b, a) {
      if (![r, g, b, a].every(value => Number.isInteger(value) && value >= 0 && value <= 255)) {
        throw new Error('Color components must be integers from 0 to 255')
      }
      const source = Object.freeze({ kind: 'color', r, g, b, a })
      const resource = ownedImage({
        width: 1, height: 1,
        draw(context) { context.fillStyle = `rgba(${r},${g},${b},${a / 255})`; context.fillRect(0, 0, 1, 1) },
        dispose() {},
      })
      return box(resource, source)
    },
    image_info(ticket) {
      const { resource } = take(ticket)
      return [resource.width, resource.height]
    },
    materialize_image(ticket) {
      const state = take(ticket)
      const resource = consume(state)
      try { return core.registerPreparedTexture(state.source, resource) }
      catch (error) { resource.dispose(); throw error }
    },
    discard_image(ticket) {
      const state = tickets.get(ticket)
      if (state?.resource) consume(state).dispose()
    },
    describe_texture(id) {
      const texture = core.textures.get(id)
      if (!texture) return null
      if (texture.source) return { ...texture.source }
      if (validPath(texture.path)) return { kind: 'asset', path: texture.path }
      throw new Error('Texture has no reconstructible source')
    },
    // The host calls this before releasing a VM; materialized resources remain
    // owned by core.destroyTexture, while abandoned CPU preparations are freed.
    dispose() {
      closed = true
      const errors = []
      for (const state of pending) {
        try { consume(state).dispose() } catch (error) { errors.push(error) }
      }
      if (errors.length) throw new AggregateError(errors, 'Cannot discard prepared images')
    },
  }
}
