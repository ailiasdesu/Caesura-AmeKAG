// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { TimelineView, buildTimelineSections } from './TimelineView'
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

function doc(content: string): OpenDoc {
  return { path: 'assets/script/main.ks', name: 'main.ks', language: 'kag', content, dirty: false }
}

beforeEach(() => {
  cleanup()
  revealMock.mockClear()
  useEditor.setState({
    docs: [],
    activePath: null,
    sideView: 'explorer',
    engineConnected: false,
    engineScene: '',
    engineToken: 0,
    enginePaused: false,
    revealRequest: null,
    inspected: null,
  })
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
    // count is part of the section title (split text nodes)
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
    // only the prologue bg remains; ch/playbgm hidden
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
