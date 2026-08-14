// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { SceneTree } from './SceneTree'
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

const KS_SOURCE = [
  '*start',
  '[bg storage="room.png"]',
  '[ch name="Hero" text="Hello there"]',
  '[playbgm file="bgm.ogg" loop=1]',
  '*end',
].join('\n')

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
  })
})

describe('SceneTree (component)', () => {
  it('shows the empty-state hint when no document is open', () => {
    render(<SceneTree />)
    expect(screen.getByText('Open a .ks script to see its scene')).toBeTruthy()
    expect(screen.queryByRole('button')).toBeNull()
  })

  it('renders one button per parsed scene element with icons and line numbers', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneTree />)

    const buttons = screen.getAllByRole('button')
    expect(buttons).toHaveLength(5)
    // label row
    expect(buttons[0].textContent).toContain('🏷')
    expect(buttons[0].textContent).toContain('*start')
    expect(buttons[0].textContent).toContain('1')
    // bg row carries the extracted storage detail
    expect(buttons[1].textContent).toContain('🖼')
    expect(buttons[1].textContent).toContain('[bg]')
    expect(buttons[1].textContent).toContain('room.png')
    // ch row
    expect(buttons[2].textContent).toContain('💬')
    // audio row
    expect(buttons[3].textContent).toContain('🎵')
    expect(buttons[3].textContent).toContain('bgm.ogg')
    // trailing label
    expect(buttons[4].textContent).toContain('*end')
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

  it('requests a reveal when an element is clicked', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneTree />)

    fireEvent.click(screen.getAllByRole('button')[1]) // [bg] at line 2
    expect(revealMock).toHaveBeenCalledWith('assets/script/main.ks', 2)
    const state = useEditor.getState()
    expect(state.revealRequest).toEqual({
      path: 'assets/script/main.ks',
      line: 2,
      nonce: expect.any(Number) as unknown as number,
    })
  })

  it('requests a reveal of line 1 on mount (initial focus)', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneTree />)
    // mount effect pushes a reveal request into the store (consumed by
    // EditorArea); the direct revealEditorLine call happens on clicks.
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
    expect(screen.queryByRole('button')).toBeNull()
  })
})
