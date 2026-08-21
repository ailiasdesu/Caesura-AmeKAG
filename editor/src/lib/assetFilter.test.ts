import { describe, it, expect } from 'vitest'
import { filterByType, type AssetTypeFilter } from './assetFilter'
import type { AssetEntry } from './rpc'

const ASSETS: AssetEntry[] = [
  { path: 'assets/script/main.ks', name: 'main.ks', type: 'script' },
  { path: 'assets/script/start.ks', name: 'start.ks', type: 'script' },
  { path: 'assets/image/room.png', name: 'room.png', type: 'image', kind: 'bg' },
  { path: 'assets/image/char.png', name: 'char.png', type: 'image', kind: 'fg' },
  { path: 'assets/audio/bgm.ogg', name: 'bgm.ogg', type: 'audio', kind: 'bgm' },
  { path: 'assets/data/meta.ini', name: 'meta.ini', type: 'other' },
]

describe('filterByType', () => {
  it("returns every asset for 'all'", () => {
    const out = filterByType(ASSETS, 'all')
    expect(out).toHaveLength(ASSETS.length)
    expect(out.map((a) => a.path)).toEqual(ASSETS.map((a) => a.path))
  })

  it("returns only script assets for 'script'", () => {
    const out = filterByType(ASSETS, 'script')
    expect(out.map((a) => a.path)).toEqual([
      'assets/script/main.ks',
      'assets/script/start.ks',
    ])
  })

  it("returns only image assets for 'image'", () => {
    const out = filterByType(ASSETS, 'image')
    expect(out.map((a) => a.path)).toEqual([
      'assets/image/room.png',
      'assets/image/char.png',
    ])
  })

  it("returns only audio assets for 'audio'", () => {
    const out = filterByType(ASSETS, 'audio')
    expect(out.map((a) => a.path)).toEqual(['assets/audio/bgm.ogg'])
  })

  it('drops unknown-type assets under a specific filter but keeps them under all', () => {
    const anyFilter: AssetTypeFilter[] = ['image', 'audio', 'script']
    for (const t of anyFilter) {
      const out = filterByType(ASSETS, t)
      expect(out.some((a) => a.type === 'other')).toBe(false)
    }
    expect(filterByType(ASSETS, 'all').some((a) => a.type === 'other')).toBe(true)
  })

  it('does not mutate the input array', () => {
    const copy = ASSETS.slice()
    filterByType(ASSETS, 'image')
    expect(ASSETS).toEqual(copy)
  })
})
