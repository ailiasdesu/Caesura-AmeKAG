import { readAssetBytes } from './restore-assets.js'
import { MAX_AUDIO_BYTES } from './audio-engine.js'

function validate(snapshot) {
  if (!snapshot || snapshot.version !== 1) throw new Error('Invalid saved audio version')
  if (snapshot.bgm === false) return Object.freeze({ version: 1, bgm: false })
  const bgm = snapshot.bgm
  if (!bgm || typeof bgm.path !== 'string' || !bgm.path || bgm.path.length > 4096
      || new TextEncoder().encode(bgm.path).byteLength > 4096 || bgm.path.startsWith('/')
      || bgm.path.includes('..') || /[\x00-\x1f\\:]/.test(bgm.path)
      || !Number.isFinite(bgm.position) || bgm.position < 0 || bgm.position > 86400
      || !Number.isFinite(bgm.gain) || bgm.gain < 0 || bgm.gain > 16
      || typeof bgm.looping !== 'boolean') throw new Error('Invalid saved BGM state')
  return Object.freeze({ version: 1, bgm: Object.freeze({ path: bgm.path,
    position: bgm.position, gain: bgm.gain, looping: bgm.looping }) })
}

/** Opaque CPU tickets; only apply starts audio. A silent snapshot needs no
 * AudioContext. Active audio always requires the real host decoder. */
export function createAudioRestore({ audio, fetchImpl = globalThis.fetch, assetUrl = path => path }) {
  const tickets = new WeakMap()
  const pending = new Set()
  let closed = false
  const assertOpen = () => { if (closed) throw new Error('Audio preparation session is closed') }
  function consume(ticket) {
    const state = tickets.get(ticket)
    if (!state || !pending.has(state)) throw new Error('Prepared audio is unavailable or consumed')
    tickets.delete(ticket)
    pending.delete(state)
    const packet = state.packet
    state.packet = null
    return { packet, description: state.description }
  }
  return {
    capture_audio() { return validate({ version: 1, bgm: audio.captureBgm() }) },
    async prepare_audio(snapshot) {
      assertOpen()
      const description = validate(snapshot)
      let packet = null
      if (description.bgm !== false) {
        const context = audio.ensureContext()
        if (!context) throw new Error('AudioContext is unavailable')
        const bytes = await readAssetBytes(description.bgm.path, { fetchImpl, assetUrl, maxBytes: MAX_AUDIO_BYTES })
        assertOpen()
        packet = await audio.decodePrepared(bytes, context)
        if (description.bgm.position >= packet.buffer.duration) throw new Error('Saved audio position is outside the decoded clip')
      }
      assertOpen()
      const ticket = Object.freeze({})
      const state = { packet, description }
      tickets.set(ticket, state)
      pending.add(state)
      return ticket
    },
    apply_audio(ticket) {
      assertOpen()
      const { packet, description } = consume(ticket)
      if (description.bgm === false) { audio.stopAll(); return true }
      return audio.applyPreparedBgm(packet, description.bgm)
    },
    discard_audio(ticket) {
      const state = tickets.get(ticket)
      if (state) {
        tickets.delete(ticket)
        pending.delete(state)
        state.packet = null
      }
    },
    stop_audio() { audio.stopAll(); return true },
    dispose() {
      closed = true
      for (const state of pending) state.packet = null
      pending.clear()
    },
  }
}
