import { describe, it, expect } from 'vitest'
import { parseAssetDrop } from '../lib/assetDrop'

describe('parseAssetDrop', () => {
  it('parses a valid script asset payload', () => {
    const r = parseAssetDrop('{"path":"assets/script/main.ks","type":"script"}')
    expect(r).toEqual({ path: 'assets/script/main.ks', type: 'script' })
  })

  it('parses image/audio payloads', () => {
    expect(parseAssetDrop('{"path":"assets/bg/x.png","type":"image"}'))
      .toEqual({ path: 'assets/bg/x.png', type: 'image' })
  })

  it('returns null for empty/missing payload', () => {
    expect(parseAssetDrop(null)).toBeNull()
    expect(parseAssetDrop('')).toBeNull()
    expect(parseAssetDrop(undefined)).toBeNull()
  })

  it('returns null for malformed JSON', () => {
    expect(parseAssetDrop('not-json')).toBeNull()
  })

  it('returns null for payloads missing path or type', () => {
    expect(parseAssetDrop('{"path":"x"}')).toBeNull()
    expect(parseAssetDrop('{"type":"script"}')).toBeNull()
    expect(parseAssetDrop('{"path":5,"type":"script"}')).toBeNull()
  })
})
