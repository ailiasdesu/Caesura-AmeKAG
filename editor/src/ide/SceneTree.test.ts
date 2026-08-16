// @vitest-environment jsdom
import { describe, it, expect, vi } from 'vitest'
// SceneTree imports revealEditorLine from EditorArea, which pulls in
// monaco-editor — not available in jsdom (collection crash guard, round 94).
vi.mock('./EditorArea', () => ({
  revealEditorLine: vi.fn(),
}))
import { parseSceneElements } from './SceneTree'

describe('parseSceneElements', () => {
  it('extracts labels, typed commands, and skips comments/blank lines', () => {
    const src = [
      '*start',
      '; comment line',
      '[bg storage="room.png"]',
      '[ch name="Hero" text="Hello"]',
      '[playbgm file="bgm.ogg" loop=1]',
      '',
      '[fg chara_show name="hero"]',
      '[image file="cg1.png"]',
      '*end_label',
    ].join('\n')

    const els = parseSceneElements(src)
    expect(els).toEqual([
      { line: 1, type: 'label', text: '*start', detail: '', params: {} },
      { line: 3, type: 'bg', text: '[bg]', detail: 'room.png', params: { storage: 'room.png' } },
      { line: 4, type: 'ch', text: '[ch]', detail: 'Hero', params: { name: 'Hero', text: 'Hello' } },
      { line: 5, type: 'audio', text: '[playbgm]', detail: 'bgm.ogg', params: { file: 'bgm.ogg', loop: '1' } },
      { line: 7, type: 'fg', text: '[fg]', detail: 'hero', params: { chara_show: 'true', name: 'hero' } },
      { line: 8, type: 'bg', text: '[image]', detail: 'cg1.png', params: { file: 'cg1.png' } },
      { line: 9, type: 'label', text: '*end_label', detail: '', params: {} },
    ])
  })

  it('is tolerant of unclosed tags and bare text lines', () => {
    const src = '[ch text="a"]\n---[unclosed tag\nplain text line\n[bg]'
    const els = parseSceneElements(src)
    // unclosed tag line: the regex tolerates it (captures "---[unclosed" as cmd)
    expect(els.length).toBeGreaterThanOrEqual(2)
    // bare text line is skipped
    expect(els.some((e) => e.text.includes('plain'))).toBe(false)
  })

  it('named-param detail is NOT truncated (extracted verbatim)', () => {
    const long = 'x'.repeat(60)
    const src = '[ch text="' + long + '"]'
    const els = parseSceneElements(src)
    expect(els[0].detail).toBe(long) // storage/file/text/name extraction wins
  })

  it('bare long rest IS truncated to 30 chars with ellipsis', () => {
    const long = 'x'.repeat(60)
    const src = '[bg ' + long + ']'
    const els = parseSceneElements(src)
    expect(els[0].detail.length).toBe(31) // 30 + ellipsis
    expect(els[0].detail.endsWith('…')).toBe(true)
  })

  it('classifies command families', () => {
    const src = [
      '[bg storage="a.png"]',
      '[image file="b.png"]',
      '[fg name="x"]',
      '[chara_show name="y"]',
      '[ch text="z"]',
      '[text content="w"]',
      '[playbgm file="m.ogg"]',
      '[playse file="s.ogg"]',
      '[play file="v.ogg"]',
      '[wait time=100]',
    ].join('\n')
    const els = parseSceneElements(src)
    expect(els.map((e) => e.type)).toEqual([
      'bg', 'bg', 'fg', 'fg', 'ch', 'ch',
      'audio', 'audio', 'audio', 'other',
    ])
  })

  it('handles empty source', () => {
    expect(parseSceneElements('')).toEqual([])
    expect(parseSceneElements('\n\n\n')).toEqual([])
  })

  it('reports 1-based line numbers', () => {
    const src = '[bg]\n[fg]\n[ch]'
    const els = parseSceneElements(src)
    expect(els.map((e) => e.line)).toEqual([1, 2, 3])
  })
})