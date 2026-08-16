import { describe, it, expect } from 'vitest'
import {
  parseSceneOutline,
  buildOutlineSections,
  parseTagParams,
  getVisibleWindow,
  scrollTopToRevealRow,
  flattenOutlineRows,
} from './sceneOutline'

describe('parseSceneOutline', () => {
  it('produces labels, commands with params, and text content in source order', () => {
    const src = [
      '*start',
      '[bg storage="room.png"]',
      'It was a quiet morning.',
      '[ch name="Hero" text="Good morning"]',
      '*next',
      '[playbgm file="bgm.ogg" loop=1]',
    ].join('\n')

    const items = parseSceneOutline(src)
    expect(items).toEqual([
      { kind: 'label', line: 1, name: 'start' },
      { kind: 'command', line: 2, cmd: 'bg', params: { storage: 'room.png' } },
      { kind: 'text', line: 3, content: 'It was a quiet morning.' },
      { kind: 'command', line: 4, cmd: 'ch', params: { name: 'Hero', text: 'Good morning' } },
      { kind: 'label', line: 5, name: 'next' },
      { kind: 'command', line: 6, cmd: 'playbgm', params: { file: 'bgm.ogg', loop: '1' } },
    ])
  })

  it('includes bare text lines as content (unlike the flat scene stream)', () => {
    const src = '*start\nHello world\nthe story continues.'
    const items = parseSceneOutline(src)
    expect(items).toEqual([
      { kind: 'label', line: 1, name: 'start' },
      { kind: 'text', line: 2, content: 'Hello world' },
      { kind: 'text', line: 3, content: 'the story continues.' },
    ])
  })

  it('skips blank lines and comments', () => {
    const src = [
      '',
      '; a comment',
      '*start',
      '   ',
      '; another comment',
      '[ch text="hi"]',
    ].join('\n')
    const items = parseSceneOutline(src)
    expect(items).toEqual([
      { kind: 'label', line: 3, name: 'start' },
      { kind: 'command', line: 6, cmd: 'ch', params: { text: 'hi' } },
    ])
  })

  it('degrades a malformed unclosed [tag without crashing', () => {
    const src = '*start\n[ch text="a\n---[unclosed\n[bg]'
    const items = parseSceneOutline(src)
    // "[ch text="a" line has a command word -> command row (tolerated)
    expect(items[1]).toMatchObject({ kind: 'command', cmd: 'ch' })
    // "---[unclosed" is a text line (bracket with no command word)
    expect(items[2]).toMatchObject({ kind: 'text', content: '---[unclosed' })
    // well-formed [bg] still parses
    expect(items[3]).toMatchObject({ kind: 'command', cmd: 'bg' })
    expect.assertions(3)
  })

  it('treats a bracket with no command word as text, not a throw', () => {
    const items = parseSceneOutline('some [[ weird line')
    expect(items).toEqual([{ kind: 'text', line: 1, content: 'some [[ weird line' }])
  })

  it('returns [] for empty and whitespace-only sources', () => {
    expect(parseSceneOutline('')).toEqual([])
    expect(parseSceneOutline('\n\n  \n')).toEqual([])
    expect(parseSceneOutline('; only a comment')).toEqual([])
  })

  it('reports 1-based line numbers', () => {
    const items = parseSceneOutline('[bg]\ntext\n*L')
    expect(items.map((i) => i.line)).toEqual([1, 2, 3])
  })
})

describe('buildOutlineSections', () => {
  it('groups items under labels, with a prologue before the first label', () => {
    const items = parseSceneOutline(
      'prologue text\n[bg storage="a.png"]\n*start\n[ch text="hi"]\n*end\nThe end.',
    )
    const sections = buildOutlineSections(items)

    expect(sections[0]).toEqual({
      label: null,
      line: 1,
      items: [
        { kind: 'text', line: 1, content: 'prologue text' },
        { kind: 'command', line: 2, cmd: 'bg', params: { storage: 'a.png' } },
      ],
    })
    expect(sections[1].label).toBe('start')
    expect(sections[1].line).toBe(3)
    expect(sections[1].items).toEqual([
      { kind: 'command', line: 4, cmd: 'ch', params: { text: 'hi' } },
    ])
    expect(sections[2]).toEqual({
      label: 'end',
      line: 5,
      items: [{ kind: 'text', line: 6, content: 'The end.' }],
    })
  })

  it('returns [] for an empty item list', () => {
    expect(buildOutlineSections([])).toEqual([])
  })
})

describe('parseTagParams', () => {
  it('parses quoted, bare and flag params', () => {
    expect(parseTagParams('name="Hero" loop=1 visible')).toEqual({
      name: 'Hero',
      loop: '1',
      visible: 'true',
    })
  })

  it('is tolerant of malformed bodies', () => {
    expect(parseTagParams('a="unclosed')).toEqual({ a: 'unclosed' })
    expect(parseTagParams('')).toEqual({})
  })
})
describe('getVisibleWindow', () => {
  const ROW = 20

  it('renders all rows when the list is shorter than the viewport', () => {
    const w = getVisibleWindow({
      scrollTop: 0,
      viewportHeight: 320,
      rowHeight: ROW,
      totalRows: 8,
    })
    expect(w.startIndex).toBe(0)
    expect(w.endIndex).toBe(7)
    expect(w.paddingTop).toBe(0)
    expect(w.paddingBottom).toBe(0)
  })

  it('windows a long list to the scroll position plus overscan', () => {
    // 2000 rows @ 20px = 40,000px; a 200px viewport shows ~10 rows.
    const w = getVisibleWindow({
      scrollTop: 0,
      viewportHeight: 200,
      rowHeight: ROW,
      totalRows: 2000,
      overscan: 2,
    })
    // first row is 0; last visible = floor(200/20)+overscan = 10+2 = 12
    expect(w.startIndex).toBe(0)
    expect(w.endIndex).toBe(12)
    expect(w.paddingTop).toBe(0)
    expect(w.paddingBottom).toBe((2000 - 1 - 12) * ROW)
  })

  it('shifts the window as scrollTop advances', () => {
    const w = getVisibleWindow({
      scrollTop: 1000,
      viewportHeight: 200,
      rowHeight: ROW,
      totalRows: 2000,
      overscan: 2,
    })
    // floor(1000/20)=50; start = 50-2 = 48; end = floor(1200/20)+2 = 62
    expect(w.startIndex).toBe(48)
    expect(w.endIndex).toBe(62)
    expect(w.paddingTop).toBe(48 * ROW)
    expect(w.paddingBottom).toBe((2000 - 1 - 62) * ROW)
  })

  it('clamps near the end so no range exceeds the last row', () => {
    const w = getVisibleWindow({
      scrollTop: 2000 * ROW,
      viewportHeight: 200,
      rowHeight: ROW,
      totalRows: 2000,
      overscan: 4,
    })
    expect(w.endIndex).toBe(1999)
    // start never exceeds last
    expect(w.startIndex).toBeLessThanOrEqual(1999)
    expect(w.paddingBottom).toBe(0)
  })

  it('handles degenerate inputs without throwing', () => {
    expect(
      getVisibleWindow({ scrollTop: 0, viewportHeight: 0, rowHeight: ROW, totalRows: 100 }),
    ).toEqual({ startIndex: 0, endIndex: 99, paddingTop: 0, paddingBottom: 0 })
    expect(
      getVisibleWindow({ scrollTop: 5, viewportHeight: 100, rowHeight: 0, totalRows: 5 }),
    ).toEqual({ startIndex: 0, endIndex: 4, paddingTop: 0, paddingBottom: 0 })
    expect(
      getVisibleWindow({ scrollTop: 0, viewportHeight: 100, rowHeight: ROW, totalRows: 0 }),
    ).toEqual({ startIndex: 0, endIndex: 0, paddingTop: 0, paddingBottom: 0 })
  })
})

describe('scrollTopToRevealRow', () => {
  const ROW = 20

  it('keeps the scroll position when the row is already visible', () => {
    expect(
      scrollTopToRevealRow({
        rowIndex: 5,
        rowHeight: ROW,
        viewportHeight: 200,
        currentScrollTop: 100,
        totalRows: 100,
      }),
    ).toBe(100)
  })

  it('scrolls up so a row above the viewport becomes visible', () => {
    // row 2 top = 40, below current view (100..300) -> scroll to top of row 2
    expect(
      scrollTopToRevealRow({
        rowIndex: 2,
        rowHeight: ROW,
        viewportHeight: 200,
        currentScrollTop: 100,
        totalRows: 100,
      }),
    ).toBe(40)
  })

  it('scrolls down so a row below the viewport becomes visible', () => {
    // row 20 top=400, bottom=420; view is 100..300 -> scroll to bottom-edge
    expect(
      scrollTopToRevealRow({
        rowIndex: 20,
        rowHeight: ROW,
        viewportHeight: 200,
        currentScrollTop: 100,
        totalRows: 100,
      }),
    ).toBe(400 + ROW - 200) // 220
  })

  it('clamps to the valid scroll range', () => {
    // last row below a tiny viewport -> clamp to maxScrollTop
    expect(
      scrollTopToRevealRow({
        rowIndex: 99,
        rowHeight: ROW,
        viewportHeight: 200,
        currentScrollTop: 0,
        totalRows: 100,
      }),
    ).toBe(100 * ROW - 200) // 1800
  })
})

describe('flattenOutlineRows', () => {
  it('renders each label heading followed by its items, in order', () => {
    const sections = buildOutlineSections(
      parseSceneOutline('a\n*start\none\ntwo\n*end\nthree'),
    )
    const rows = flattenOutlineRows(sections)
    const label = (r: (typeof rows)[number]) => {
      if (r.rowKind === 'section') {
        return r.section.label === null ? '(prologue)' : '*' + r.section.label
      }
      const item = r.item
      if (item.kind === 'text') return item.content
      if (item.kind === 'command') return '[' + item.cmd + ']'
      return '*' + item.name
    }
    expect(rows.map(label)).toEqual([
      '(prologue)',
      'a',
      '*start',
      'one',
      'two',
      '*end',
      'three',
    ])
    // every row carries a stable, non-empty key
    for (const row of rows) expect(row.key.length).toBeGreaterThan(0)
    // keys are unique across the whole list
    expect(new Set(rows.map((r) => r.key)).size).toBe(rows.length)
  })
})

