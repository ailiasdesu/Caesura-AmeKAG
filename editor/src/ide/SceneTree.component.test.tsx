// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup, act } from '@testing-library/react'
import { SceneTree, TYPE_ORDER } from './SceneTree'
import { revealEditorLine } from './EditorArea'
import { useEditor, type OpenDoc } from '../store'

// SceneTree imports revealEditorLine from EditorArea, which pulls in
// monaco-editor — not available in jsdom. Mock the module so the
// component can be rendered without a Monaco instance.
vi.mock('./EditorArea', () => ({
  revealEditorLine: vi.fn(),
}))

const revealMock = vi.mocked(revealEditorLine)

function doc(path: string, content: string): OpenDoc {
  return { path, name: path.split('/').pop() ?? path, language: 'kag', content, dirty: false }
}

const KS_SOURCE = '*start\n[bg storage="room.png"]\n[ch name="Hero" text="Hello there"]\n[playbgm file="bgm.ogg" loop=1]\n*end'

// Element rows only (filters out group-header toggles). Because the tree
// groups by type, rows appear grouped, not in source order.
function elementButtons(): HTMLElement[] {
  return Array.from(document.querySelectorAll('.scene-tree .scene-el'))
}

function rowContaining(text: string): HTMLElement {
  const row = elementButtons().find((b) => (b.textContent ?? '').includes(text))
  if (!row) throw new Error('no scene row containing: ' + text)
  return row
}

function groupHeaders(): HTMLElement[] {
  return Array.from(document.querySelectorAll('.scene-tree .scene-group-header'))
}

function allButtons(): number {
  return document.querySelectorAll('.scene-tree button').length
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
    editorSelection: null,
  })
})

describe('SceneTree (component)', () => {
  it('shows the empty-state hint when no document is open', () => {
    render(<SceneTree />)
    expect(screen.getByText('Open a .ks script to see its scene')).toBeTruthy()
    expect(allButtons()).toBe(0)
  })

  it('renders every element as a button with icons, details and line numbers', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneTree />)

    const rows = elementButtons()
    expect(rows).toHaveLength(5)
    const start = rowContaining('*start')
    expect(start.textContent).toContain('🏷')
    expect(start.textContent).toContain('1')
    const bg = rowContaining('[bg]')
    expect(bg.textContent).toContain('🖼')
    expect(bg.textContent).toContain('room.png')
    const ch = rowContaining('[ch]')
    expect(ch.textContent).toContain('💬')
    const audio = rowContaining('[playbgm]')
    expect(audio.textContent).toContain('🎵')
    expect(audio.textContent).toContain('bgm.ogg')
  })

  it('creates one collapsible group per present type with a per-group count', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    const { container } = render(<SceneTree />)
    const headers = groupHeaders()
    expect(headers).toHaveLength(4)
    expect(headers[0].textContent).toContain('Labels')
    expect(headers[1].textContent).toContain('Backgrounds')
    const groups = Array.from(container.querySelectorAll('.scene-group'))
    for (const g of groups) {
      const typeClass = Array.from(g.classList).find((c) => c.startsWith('scene-group-'))!.replace('scene-group-', '')
      const gRows = g.querySelectorAll('.scene-el')
      for (const r of Array.from(gRows)) expect(r.classList.contains('scene-' + typeClass)).toBe(true)
    }
  })

  it('shows the per-type counts in the panel title', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneTree />)
    const title = screen.getByText('Scene Tree').parentElement
    expect(title?.textContent).toContain('2🏷')
    expect(title?.textContent).toContain('1🖼')
    expect(title?.textContent).toContain('1💬')
  })

  it('requests a reveal + inspects when an element is clicked', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneTree />)

    fireEvent.click(rowContaining('[bg]'))
    expect(revealMock).toHaveBeenCalledWith('assets/script/main.ks', 2)
    const state = useEditor.getState()
    expect(state.revealRequest).toEqual({ path: 'assets/script/main.ks', line: 2, nonce: expect.any(Number) })
    expect(state.inspected).toEqual({ path: 'assets/script/main.ks', line: 2 })
  })

  it('category-toggle clicks never trigger a navigation', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneTree />)
    fireEvent.click(groupHeaders()[1])
    expect(revealMock).not.toHaveBeenCalled()
    expect(useEditor.getState().inspected).toBeNull()
  })

  it('marks the inspected element with the inspected class + aria-pressed', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
      inspected: { path: 'assets/script/main.ks', line: 3 },
    })
    render(<SceneTree />)
    const ch = rowContaining('[ch]')
    const start = rowContaining('*start')
    expect(ch.className).toContain('inspected')
    expect(start.className).not.toContain('inspected')
    expect(ch.getAttribute('aria-pressed')).toBe('true')
    expect(start.getAttribute('aria-pressed')).toBe('false')
  })

  it('re-highlights reactively when the store inspected value changes after mount', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
      inspected: null,
    })
    render(<SceneTree />)
    expect(elementButtons().some((b) => b.className.includes('inspected'))).toBe(false)
    act(() => useEditor.setState({ inspected: { path: 'assets/script/main.ks', line: 3 } }))
    const ch = rowContaining('[ch]')
    expect(ch.className).toContain('inspected')
    expect(ch.getAttribute('aria-pressed')).toBe('true')
  })

  it('expands and collapses a group, flipping aria-expanded', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    const { container } = render(<SceneTree />)
    expect(elementButtons()).toHaveLength(5)

    const bgHeader = groupHeaders()[1]
    expect(bgHeader.getAttribute('aria-expanded')).toBe('true')
    fireEvent.click(bgHeader)
    expect(elementButtons()).toHaveLength(4)
    expect(container.querySelectorAll('.scene-group-bg .scene-el')).toHaveLength(0)
    expect(bgHeader.getAttribute('aria-expanded')).toBe('false')

    fireEvent.click(bgHeader)
    expect(elementButtons()).toHaveLength(5)
    expect(bgHeader.getAttribute('aria-expanded')).toBe('true')
  })

  it('rebuilds the tree when the active document switches', () => {
    useEditor.setState({
      docs: [
        doc('a.ks', '[bg storage="a.png"]'),
        doc('b.ks', '*two\n[ch text="B"]\n[playse file="s.ogg"]'),
      ],
      activePath: 'a.ks',
    })
    render(<SceneTree />)
    expect(elementButtons()).toHaveLength(1)
    expect(rowContaining('[bg]').textContent).toContain('a.png')

    act(() => useEditor.setState({ activePath: 'b.ks' }))
    const rows = elementButtons()
    expect(rows).toHaveLength(3)
    expect(rowContaining('*two').textContent).toContain('*two')
    expect(rowContaining('[ch]')).toBeTruthy()
    expect(rowContaining('[playse]').textContent).toContain('s.ogg')
  })

  it('rows are keyboard-reachable and native activation navigates', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneTree />)
    const row = rowContaining('[playbgm]')
    row.focus()
    expect(document.activeElement).toBe(row)
    fireEvent.keyDown(row, { key: 'Enter' })
    fireEvent.click(row)
    expect(revealMock).toHaveBeenCalledWith('assets/script/main.ks', 4)
    expect(useEditor.getState().inspected).toEqual({ path: 'assets/script/main.ks', line: 4 })
  })

  it('requests a reveal of line 1 on mount (initial focus)', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneTree />)
    const state = useEditor.getState()
    expect(state.revealRequest?.path).toBe('assets/script/main.ks')
    expect(state.revealRequest?.line).toBe(1)
    expect(revealMock).not.toHaveBeenCalled()
  })

  it('renders the no-elements hint for an empty script', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', '; only a comment')],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneTree />)
    expect(screen.getByText('No scene elements found')).toBeTruthy()
    expect(allButtons()).toBe(0)
  })

  it('tolerates corrupted / non-KAG content without crashing', () => {
    const corrupt = ']]]{{{{??*almost\n[bg storage=\n???\n    [ch'
    useEditor.setState({
      docs: [doc('assets/script/broken.ks', corrupt)],
      activePath: 'assets/script/broken.ks',
    })
    expect(() => render(<SceneTree />)).not.toThrow()
    expect(document.querySelector('.scene-tree')).toBeTruthy()
  })
})

// Long-list boundary assessment (round 90+): SceneTree is NOT virtualized.
// Rendering stays synchronous and the panel scrolls (.scene-tree has
// overflow:auto); active docs are far below an interactive threshold, so a
// full render is the pragmatic baseline — revisit if a scene ever grows past
// a few thousand elements (the shared SceneOutline virtualization landed
// separately in round 95).
function longListSource(n: number): string {
  const out: string[] = []
  for (let i = 0; i < n; i++) out.push('[bg storage="' + i + '.png"]')
  out.push('*end')
  return out.join('\n')
}

describe('SceneTree (long list assessment)', () => {
  it('renders a large flat scene in full (no virtualization needed at this size)', () => {
    useEditor.setState({
      docs: [doc('assets/script/long.ks', longListSource(400))],
      activePath: 'assets/script/long.ks',
    })
    render(<SceneTree />)
    expect(elementButtons()).toHaveLength(401)
    expect(groupHeaders().length).toBeGreaterThan(0)
  })
})

// keep TYPE_ORDER referenced so its exported shape is exercised here too
void TYPE_ORDER