// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import {
  TimelineView,
  buildTimelineSections,
  collectTextLines,
  buildTimelineSectionRows,
  timelineTokenToLine,
} from './TimelineView'
import { revealEditorLine } from './EditorArea'
import { useEditor, type OpenDoc } from '../store'

vi.mock('./EditorArea', () => ({
  revealEditorLine: vi.fn(),
}))

const revealMock = vi.mocked(revealEditorLine)

const KS = [
  '[bg storage="prologue.png"]',
  '*start',
  '[ch name="Aoi" text="Hello"]',
  '[playbgm file="bgm.ogg" loop=1]',
  '*end',
  '[ch text="Bye"]',
].join('\n')

// A script with bare dialog / text lines interleaved with commands.
const KS_TEXT = [
  '*start',
  'Hello there, Aoi.',
  '[ch name="Aoi" text="hi"]',
  'world',
].join('\n')

function doc(content: string): OpenDoc {
  return { path: 'assets/script/main.ks', name: 'main.ks', language: 'kag', content, dirty: false }
}

function baseState(over: Partial<ReturnType<typeof useEditor.getState>> = {}) {
  useEditor.setState({
    docs: [],
    activePath: null,
    sideView: 'explorer',
    engineConnected: false,
    engineScene: '',
    engineToken: 0,
    enginePaused: false,
    engineCmd: '',
    revealRequest: null,
    inspected: null,
    ...over,
  })
}

beforeEach(() => {
  cleanup()
  revealMock.mockClear()
  baseState()
})

describe('buildTimelineSections (pure)', () => {
  it('groups elements into label sections in order', () => {
    const s = buildTimelineSections(KS)
    expect(s).toHaveLength(3)
    expect(s[0]).toEqual({ label: null, line: 1, elements: [{ line: 1, type: 'bg', text: '[bg]', detail: 'prologue.png', params: { storage: 'prologue.png' } }] })
    expect(s[1].label).toBe('start')
    expect(s[1].line).toBe(2)
    expect(s[1].elements.map((e) => e.text)).toEqual(['[ch]', '[playbgm]'])
    expect(s[2].label).toBe('end')
    expect(s[2].elements.map((e) => e.text)).toEqual(['[ch]'])
  })

  it('returns an empty list for an empty source', () => {
    expect(buildTimelineSections('')).toEqual([])
    expect(buildTimelineSections('; only comments\n\n')).toEqual([])
  })

  it('puts all elements into one unnamed section for label-less scripts', () => {
    const s = buildTimelineSections('[bg]\n[ch text="x"]')
    expect(s).toHaveLength(1)
    expect(s[0].label).toBeNull()
    expect(s[0].elements).toHaveLength(2)
    expect(s[0].line).toBe(1)
  })

  it('keeps labels as empty sections when they have no following elements', () => {
    const s = buildTimelineSections('*only')
    expect(s).toHaveLength(1)
    expect(s[0].label).toBe('only')
    expect(s[0].elements).toEqual([])
  })
})

describe('collectTextLines (pure)', () => {
  it('extracts bare text lines with 1-based line numbers', () => {
    const t = collectTextLines(KS_TEXT)
    expect(t).toEqual([
      { line: 2, text: 'Hello there, Aoi.' },
      { line: 4, text: 'world' },
    ])
  })

  it('skips labels, tags, comments and blank lines', () => {
    const t = collectTextLines('\n; comment\n*start\n[bg storage="a.png"]\n  hello  \n')
    expect(t).toEqual([{ line: 5, text: 'hello' }])
  })

  it('handles empty and comment-only sources', () => {
    expect(collectTextLines('')).toEqual([])
    expect(collectTextLines('; only comments\n\n')).toEqual([])
  })
})

describe('buildTimelineSectionRows (pure)', () => {
  it('includes bare text lines grouped inside their label section', () => {
    const s = buildTimelineSectionRows(KS_TEXT)
    expect(s).toHaveLength(1)
    const sec = s[0]
    expect(sec.label).toBe('start')
    expect(sec.elements.map((e) => e.line)).toEqual([3])
    expect(sec.texts).toEqual([
      { line: 2, text: 'Hello there, Aoi.' },
      { line: 4, text: 'world' },
    ])
  })

  it('keeps element and text lines inside their sections', () => {
    const src = '[bg]\nHello\n[ch text="x"]\nBye\n*next\n[fg]'
    const secs = buildTimelineSectionRows(src)
    const prologue = secs[0]
    const next = secs[1]
    expect(prologue.label).toBeNull()
    expect(prologue.elements.map((e) => e.line)).toEqual([1, 3])
    expect(prologue.texts.map((t) => t.line)).toEqual([2, 4])
    expect(next.label).toBe('next')
    expect(next.elements.map((e) => e.line)).toEqual([6])
  })

  it('is empty for an empty source', () => {
    expect(buildTimelineSectionRows('')).toEqual([])
  })
})

describe('timelineTokenToLine (pure)', () => {
  it('maps a token index to the source line in document order (headings + items)', () => {
    expect(timelineTokenToLine(KS, 1)).toBe(1)   // prologue bg heading
    expect(timelineTokenToLine(KS, 2)).toBe(2)   // *start heading
    expect(timelineTokenToLine(KS, 3)).toBe(3)   // [ch]
    expect(timelineTokenToLine(KS, 4)).toBe(4)   // [playbgm]
    expect(timelineTokenToLine(KS, 6)).toBe(6)   // last [ch]
  })

  it('includes text lines in the token stream', () => {
    expect(timelineTokenToLine(KS_TEXT, 1)).toBe(1) // *start heading
    expect(timelineTokenToLine(KS_TEXT, 2)).toBe(2) // Hello there
    expect(timelineTokenToLine(KS_TEXT, 3)).toBe(3) // [ch]
    expect(timelineTokenToLine(KS_TEXT, 4)).toBe(4) // world
  })

  it('returns null for out-of-range or empty token streams', () => {
    expect(timelineTokenToLine(KS, 0)).toBeNull()
    expect(timelineTokenToLine(KS, 99)).toBeNull()
    expect(timelineTokenToLine('', 1)).toBeNull()
  })
})

describe('TimelineView (component)', () => {
  it('shows the empty hint when no document is open', () => {
    render(<TimelineView />)
    expect(screen.getByText('Open a .ks script to see its timeline')).toBeTruthy()
  })

  it('renders sections with labels and element counts', () => {
    useEditor.setState({ docs: [doc(KS)], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    expect(screen.getByText('(prologue)')).toBeTruthy()
    expect(screen.getByText('* start')).toBeTruthy()
    expect(screen.getByText('* end')).toBeTruthy()
    const titles = Array.from(document.querySelectorAll('.timeline-section-title'))
    expect(titles.some((el) => el.textContent?.includes('L2 · 2'))).toBe(true)
  })

  it('jumps and inspects when an element is clicked', () => {
    useEditor.setState({ docs: [doc(KS)], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    fireEvent.click(screen.getByText('[playbgm]'))
    expect(revealMock).toHaveBeenCalledWith('assets/script/main.ks', 4)
    expect(useEditor.getState().inspected).toEqual({ path: 'assets/script/main.ks', line: 4 })
  })

  it('filters by type via chips', () => {
    useEditor.setState({ docs: [doc(KS)], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    fireEvent.click(screen.getByText('BG'))
    expect(screen.getByText('[bg]')).toBeTruthy()
    expect(screen.queryByText('[playbgm]')).toBeNull()
    expect(screen.queryByText('[ch]')).toBeNull()
  })

  it('shows the no-match hint when a filter has no elements', () => {
    useEditor.setState({ docs: [doc(KS)], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    fireEvent.click(screen.getByText('Audio'))
    expect(screen.getByText('[playbgm]')).toBeTruthy()
    fireEvent.click(screen.getByText('FG'))
    expect(screen.getByText('No matching elements')).toBeTruthy()
  })
})

describe('TimelineView engine exec status (G4)', () => {
  it('shows the live exec bar when the engine is connected and running', () => {
    useEditor.setState({
      docs: [doc(KS)],
      activePath: 'assets/script/main.ks',
      engineConnected: true,
      engineCmd: '[ch]',
      engineToken: 7,
    })
    render(<TimelineView />)
    const bar = document.querySelector('.timeline-exec')
    expect(bar).toBeTruthy()
    expect(bar?.textContent).toContain('[ch]')
    expect(bar?.textContent).toContain('token 7')
    expect(bar?.textContent).toContain('▶')
  })

  it('hides the exec bar when the engine is disconnected', () => {
    useEditor.setState({
      docs: [doc(KS)],
      activePath: 'assets/script/main.ks',
      engineConnected: false,
      engineCmd: '[ch]',
      engineToken: 7,
    })
    render(<TimelineView />)
    expect(screen.queryByText('▶')).toBeNull()
    expect(screen.queryByText('token 7')).toBeNull()
  })

  it('hides the exec bar when no command is executing yet', () => {
    useEditor.setState({
      docs: [doc(KS)],
      activePath: 'assets/script/main.ks',
      engineConnected: true,
      engineCmd: '',
      engineToken: 0,
    })
    render(<TimelineView />)
    expect(screen.queryByText('▶')).toBeNull()
  })
})

describe('TimelineView engine position linkage (G4-4)', () => {
  it('highlights the row matching the live engine token line', () => {
    baseState({
      docs: [doc(KS)],
      activePath: 'assets/script/main.ks',
      engineConnected: true,
      engineScene: 'assets/script/main.ks',
      engineToken: 4, // maps to line 4 ([playbgm])
    })
    render(<TimelineView />)
    const playbgm = screen.getByText('[playbgm]').closest('button')
    expect(playbgm?.className).toContain('outline-current')
    expect(screen.getByText('[bg]').closest('button')?.className).not.toContain('outline-current')
    for (const btn of screen.getAllByText('[ch]')) {
      expect(btn.closest('button')?.className).not.toContain('outline-current')
    }
  })

  it('highlights nothing when the engine scene does not match the open doc', () => {
    baseState({
      docs: [doc(KS)],
      activePath: 'assets/script/main.ks',
      engineConnected: true,
      engineScene: 'assets/script/other.ks',
      engineToken: 4,
    })
    render(<TimelineView />)
    expect(screen.getByText('[playbgm]').closest('button')?.className).not.toContain('outline-current')
  })

  it('highlights a text row when the engine is on a bare text line', () => {
    baseState({
      docs: [doc(KS_TEXT)],
      activePath: 'assets/script/main.ks',
      engineConnected: true,
      engineScene: 'assets/script/main.ks',
      engineToken: 2, // line 2 = "Hello there, Aoi."
    })
    render(<TimelineView />)
    const row = screen.getByText('Hello there, Aoi.').closest('button')
    expect(row?.className).toContain('outline-current')
  })
})

describe('TimelineView text lines & filters (G4 render depth)', () => {
  it('renders bare text lines as rows', () => {
    baseState({ docs: [doc(KS_TEXT)], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    expect(screen.getByText('Hello there, Aoi.')).toBeTruthy()
    expect(screen.getByText('world')).toBeTruthy()
  })

  it('text rows jump and inspect on click', () => {
    baseState({ docs: [doc(KS_TEXT)], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    fireEvent.click(screen.getByText('world'))
    expect(revealMock).toHaveBeenCalledWith('assets/script/main.ks', 4)
    expect(useEditor.getState().inspected).toEqual({ path: 'assets/script/main.ks', line: 4 })
  })

  it('Text filter shows text rows and hides command rows', () => {
    baseState({ docs: [doc(KS_TEXT)], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    fireEvent.click(screen.getByText('Text'))
    expect(screen.getByText('Hello there, Aoi.')).toBeTruthy()
    expect(screen.getByText('world')).toBeTruthy()
    expect(screen.queryByText('[ch]')).toBeNull()
  })

  it('shows the empty-script placeholder when a doc has no timeline rows', () => {
    baseState({ docs: [doc('; comments only\n\n')], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    expect(screen.getByText('No timeline elements (empty script)')).toBeTruthy()
  })
})

describe('TimelineView selection sync & keyboard nav (G4 interaction)', () => {
  it('marks the inspected row with the inspected class and aria-pressed', () => {
    baseState({
      docs: [doc(KS)],
      activePath: 'assets/script/main.ks',
      inspected: { path: 'assets/script/main.ks', line: 4 },
    })
    render(<TimelineView />)
    const playbgm = screen.getByText('[playbgm]').closest('button')
    expect(playbgm?.className).toContain('inspected')
    expect(playbgm?.getAttribute('aria-pressed')).toBe('true')
    expect(screen.getByText('[bg]').closest('button')?.className).not.toContain('inspected')
  })

  it('navigates focus from the first row with ArrowDown', () => {
    baseState({ docs: [doc(KS)], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    const body = screen.getByTestId('timeline-body')
    // ArrowDown settles on the first nav row (prologue heading, line 1)
    fireEvent.keyDown(body, { key: 'ArrowDown' })
    expect(revealMock).toHaveBeenLastCalledWith('assets/script/main.ks', 1)
    // ArrowDown again moves to the prologue bg row (same line 1)
    fireEvent.keyDown(body, { key: 'ArrowDown' })
    expect(revealMock).toHaveBeenLastCalledWith('assets/script/main.ks', 1)
    // ArrowDown once more reaches the *start heading at line 2
    fireEvent.keyDown(body, { key: 'ArrowDown' })
    expect(revealMock).toHaveBeenLastCalledWith('assets/script/main.ks', 2)
  })

  it('clamps ArrowUp at the first row', () => {
    baseState({ docs: [doc(KS)], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    const body = screen.getByTestId('timeline-body')
    fireEvent.keyDown(body, { key: 'ArrowUp' })
    expect(revealMock).toHaveBeenLastCalledWith('assets/script/main.ks', 1)
  })

  it('moves to the last row with End and back up with ArrowUp', () => {
    baseState({ docs: [doc(KS)], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    const body = screen.getByTestId('timeline-body')
    fireEvent.keyDown(body, { key: 'End' })
    expect(revealMock).toHaveBeenLastCalledWith('assets/script/main.ks', 6)
    fireEvent.keyDown(body, { key: 'ArrowUp' })
    expect(revealMock).toHaveBeenLastCalledWith('assets/script/main.ks', 5)
  })

  it('activates the focused row with Enter (jump + inspect)', () => {
    baseState({ docs: [doc(KS)], activePath: 'assets/script/main.ks' })
    render(<TimelineView />)
    const body = screen.getByTestId('timeline-body')
    fireEvent.keyDown(body, { key: 'End' }) // focus last row (line 6)
    fireEvent.keyDown(body, { key: 'Enter' })
    expect(revealMock).toHaveBeenCalledWith('assets/script/main.ks', 6)
    expect(useEditor.getState().inspected).toEqual({ path: 'assets/script/main.ks', line: 6 })
  })
})
