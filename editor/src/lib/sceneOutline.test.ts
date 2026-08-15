import { describe, it, expect } from 'vitest'
import {
  parseSceneOutline,
  buildOutlineSections,
  parseTagParams,
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
