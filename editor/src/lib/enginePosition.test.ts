import { describe, it, expect } from 'vitest'
import {
  buildPositionProbeSnippet,
  parsePositionProbe,
  sceneMatchesDoc,
  tokenToOutlineLine,
} from './enginePosition'
import { parseSceneOutline, buildOutlineSections } from './sceneOutline'

describe('buildPositionProbeSnippet', () => {
  it('anchors on _CAESURA_CTX and reports no-ctx when absent', () => {
    const snip = buildPositionProbeSnippet()
    expect(snip).toContain('_G._CAESURA_CTX')
    expect(snip).toContain('return "no-ctx"')
  })

  it('reports no-scene when the ctx carries no scene/token yet', () => {
    const snip = buildPositionProbeSnippet()
    expect(snip).toContain('return "no-scene"')
  })

  it('emits a JSON {scene, token} object for a live position', () => {
    const snip = buildPositionProbeSnippet()
    expect(snip).toContain('scene')
    expect(snip).toContain('token')
  })
})

describe('parsePositionProbe', () => {
  it('parses a well-formed reply', () => {
    expect(parsePositionProbe('{"scene":"main.ks","token":12}')).toEqual({
      scene: 'main.ks',
      token: 12,
    })
  })

  it('returns null for no-ctx / no-scene / empty', () => {
    expect(parsePositionProbe('no-ctx')).toBeNull()
    expect(parsePositionProbe('no-scene')).toBeNull()
    expect(parsePositionProbe('')).toBeNull()
    expect(parsePositionProbe('   ')).toBeNull()
  })

  it('returns null for malformed or wrong-shaped replies (never throws)', () => {
    expect(parsePositionProbe('not json')).toBeNull()
    expect(parsePositionProbe('{"scene":1}')).toBeNull()
    expect(parsePositionProbe('{"token":"x"}')).toBeNull()
    expect(parsePositionProbe('null')).toBeNull()
  })
})

describe('sceneMatchesDoc', () => {
  it('matches exact normalized paths', () => {
    expect(sceneMatchesDoc('assets/script/main.ks', 'assets/script/main.ks')).toBe(true)
  })

  it('matches on basename when paths differ (bundle vs source)', () => {
    expect(sceneMatchesDoc('assets/script/main.ks', 'main.ks')).toBe(true)
    expect(sceneMatchesDoc('assets/script/main.ks', 'assets/other/main.ks')).toBe(true)
  })

  it('rejects different scenes and empty inputs', () => {
    expect(sceneMatchesDoc('assets/script/main.ks', 'assets/script/other.ks')).toBe(false)
    expect(sceneMatchesDoc('', 'main.ks')).toBe(false)
    expect(sceneMatchesDoc('main.ks', '')).toBe(false)
  })
})

describe('tokenToOutlineLine', () => {
  const src = [
    '*start',
    '[bg storage="room.png"]',
    'It was a quiet morning.',
    '*next',
    '[playbgm file="bgm.ogg" loop=1]',
  ].join('\n')
  const sections = buildOutlineSections(parseSceneOutline(src))

  it('maps the Nth token to the Nth outline row (document order)', () => {
    // rows: *start(1), [bg](2), text(3), *next(4), [playbgm](5)
    expect(tokenToOutlineLine(sections, 1)).toBe(1)
    expect(tokenToOutlineLine(sections, 2)).toBe(2)
    expect(tokenToOutlineLine(sections, 3)).toBe(3)
    expect(tokenToOutlineLine(sections, 4)).toBe(4)
    expect(tokenToOutlineLine(sections, 5)).toBe(5)
  })

  it('returns null out of range or on empty outline', () => {
    expect(tokenToOutlineLine(sections, 0)).toBeNull()
    expect(tokenToOutlineLine(sections, 6)).toBeNull()
    expect(tokenToOutlineLine([], 1)).toBeNull()
  })
})
