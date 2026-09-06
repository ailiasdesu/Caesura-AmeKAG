// @vitest-environment node
// Real Wasmoon/shared runner/AudioEngine wiring; the explicit AudioContext fake
// checks lifecycle contracts and is not evidence of audible browser playback.
import { afterEach, expect, it, vi } from 'vitest'
import { existsSync, readFileSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { createPlayer } from './bridge.js'

const here = dirname(fileURLToPath(import.meta.url))
let player

afterEach(async () => {
  await player?.dispose()
  player = null
  vi.unstubAllGlobals()
})

it('Lua playback queries preserve a real looping AudioEngine source and stop it synchronously', async () => {
  const sources = []
  class Context {
    constructor() { this.currentTime = 0; this.state = 'running'; this.destination = {} }
    createGain() { return { gain: { value: 1 }, connect() {}, disconnect() {} } }
    createBufferSource() {
      const source = { connect: vi.fn(), start: vi.fn(), stop: vi.fn(), disconnect: vi.fn() }
      sources.push(source)
      return source
    }
    async decodeAudioData(bytes) {
      expect(bytes.byteLength).toBe(4)
      return { duration: 1, length: 48000, sampleRate: 48000, numberOfChannels: 1 }
    }
    async close() { this.state = 'closed' }
  }
  vi.stubGlobal('AudioContext', Context)
  vi.stubGlobal('FontFace', undefined)
  player = await createPlayer({
    scriptsBase: 'http://local/scripts/',
    langBase: 'http://local/assets/lang/',
    wasmFile: join(here, 'node_modules/wasmoon/dist/glue.wasm'),
    audioAssetUrl: path => 'http://local/' + path,
    fetchImpl: async url => {
      const pathname = new URL(url).pathname
      if (pathname === '/assets/query-loop.wav') return new Response(new Uint8Array([1, 2, 3, 4]))
      const path = pathname === '/scripts/index.json'
        ? join(here, 'scripts-index.json') : join(here, '..', pathname.slice(1))
      return new Response(existsSync(path) ? readFileSync(path) : '', { status: existsSync(path) ? 200 : 404 })
    },
  })

  expect(await player.runScene('[playbgm file="assets/query-loop.wav" loop=true]\n[end]', 'query-loop.ks', {
    maxFrames: 100,
  })).toMatch(/^DONE:/)
  expect(sources).toHaveLength(1)
  expect(sources[0].loop).toBe(true)
  expect(sources[0].start).toHaveBeenCalledOnce()
  expect(player.audio.isPlaying('bgm')).toBe(true)
  const allPlaying = await player.lua.doString(`
    local audio = require('backend')
    local all_playing = true
    for i=1,300 do
      if audio.audio_is_playing('bgm') ~= true then all_playing=false end
    end
    return all_playing
  `)
  expect.soft(allPlaying, 'queries must observe the source, not expire a telemetry frame counter').toBe(true)
  expect(player.audio.isPlaying('bgm')).toBe(true)
  expect(sources[0].stop).not.toHaveBeenCalled()
  expect(await player.lua.doString(`
    local audio = require('backend')
    audio.audio_stop('bgm')
    return audio.audio_is_playing('bgm') == false
  `)).toBe(true)
  expect(player.audio.isPlaying('bgm')).toBe(false)
  expect(sources[0].stop).toHaveBeenCalledOnce()
})
