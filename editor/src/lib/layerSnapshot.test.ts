// @vitest-environment jsdom
// Unit tests for the layer render-state snapshot parser + snippet builder.
import { describe, it, expect } from 'vitest'
import {
  buildLayerSnapshotSnippet,
  parseLayerSnapshot,
  layerSlot,
} from './layerSnapshot'

describe('layerSnapshot parser', () => {
  it('builds a defensive snippet that references layers.forEach', () => {
    const snippet = buildLayerSnapshotSnippet()
    expect(snippet).toContain('layers')
    expect(snippet).toContain('L.forEach')
    expect(snippet).toContain('no-layers')
    // Must guard the module missing case so it never throws on the engine.
    expect(snippet).toContain('type(L.forEach)')
  })

  it('parses a populated snapshot array with slot ordering (bg, fg, msg)', () => {
    const raw = JSON.stringify([
      { id: '1', name: 'msg', z: 10, visible: true, handle: 7, opacity: 1 },
      { id: '2', name: 'bg', z: 0, visible: true, handle: 3, opacity: 1 },
      { id: '3', name: 'fg', z: 5, visible: false, handle: 4, opacity: 0.8 },
      { id: '4', name: '_gallery', z: 100, visible: true, handle: 9, opacity: 1 },
    ])
    const layers = parseLayerSnapshot(raw)
    expect(layers).toHaveLength(4)
    // bg first, then fg, then msg, then 'other' slots.
    expect(layers.map((l) => l.name)).toEqual(['bg', 'fg', 'msg', '_gallery'])
    // fg is hidden and carries its handle/opacity through.
    const fg = layers.find((l) => l.name === 'fg')!
    expect(fg.visible).toBe(false)
    expect(fg.handle).toBe(4)
    expect(fg.opacity).toBe(0.8)
  })

  it('returns an empty array for no-layers / empty / whitespace', () => {
    expect(parseLayerSnapshot('no-layers')).toEqual([])
    expect(parseLayerSnapshot('')).toEqual([])
    expect(parseLayerSnapshot('   ')).toEqual([])
  })

  it('returns an empty array for non-array / malformed JSON', () => {
    expect(parseLayerSnapshot('{"name":"bg"}')).toEqual([])
    expect(parseLayerSnapshot('not json at all')).toEqual([])
  })

  it('drops malformed entries but keeps well-formed ones', () => {
    const raw = JSON.stringify([null, { name: 'bg', z: 1 }, 'x', { id: '9', name: 'fg' }])
    const layers = parseLayerSnapshot(raw)
    expect(layers.length).toBeGreaterThanOrEqual(2)
    for (const l of layers) {
      expect(l).toHaveProperty('id')
      expect(l).toHaveProperty('z')
      expect(l).toHaveProperty('handle')
      expect(l).toHaveProperty('visible')
      expect(l).toHaveProperty('opacity')
    }
  })

  it('defaults missing numeric fields gracefully', () => {
    const layers = parseLayerSnapshot(JSON.stringify([{ name: 'bg' }]))
    expect(layers).toHaveLength(1)
    expect(layers[0].visible).toBe(true)
    expect(layers[0].z).toBe(0)
    expect(layers[0].handle).toBe(0)
    expect(layers[0].opacity).toBe(1)
  })

  it('layerSlot maps bg/fg/msg prefixes and falls through for others', () => {
    expect(layerSlot('bg')).toBe('bg')
    expect(layerSlot('fg')).toBe('fg')
    expect(layerSlot('msg')).toBe('msg')
    expect(layerSlot('bg_top')).toBe('bg')
    expect(layerSlot('_gallery')).toBe('other')
    expect(layerSlot('')).toBe('other')
  })
})