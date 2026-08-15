// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { SceneOutlinePanel } from './SceneOutlinePanel'
import { revealEditorLine } from './EditorArea'
import { useEditor, type OpenDoc } from '../store'

// SceneOutlinePanel imports revealEditorLine from EditorArea, which pulls in
// monaco-editor — not available in jsdom. Mock the module so the wired panel
// can be rendered without a Monaco instance.
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
  'It was a quiet morning.',
  '[ch name="Hero" text="Good morning"]',
  '*next',
  '[playbgm file="bgm.ogg" loop=1]',
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
    inspected: null,
  })
})

describe('SceneOutlinePanel (store wiring)', () => {
  it('shows the empty-state hint when no .ks document is open', () => {
    render(<SceneOutlinePanel />)
    expect(screen.getByText('Open a .ks script to see its outline')).toBeTruthy()
    expect(screen.queryByText('Scene Outline')).toBeTruthy()
  })

  it('shows the empty-state hint when the active doc is not a .ks script', () => {
    useEditor.setState({
      docs: [doc('assets/script/init.lua', 'return {}')],
      activePath: 'assets/script/init.lua',
    })
    render(<SceneOutlinePanel />)
    expect(screen.getByText('Open a .ks script to see its outline')).toBeTruthy()
  })

  it('renders labels, commands, and text rows from the store doc', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    const { container } = render(<SceneOutlinePanel />)

    // label headings from the active doc
    expect(screen.getAllByText(/^\*start$/)).toHaveLength(1)
    expect(screen.getAllByText(/^\*next$/)).toHaveLength(1)

    // command rows with command names and key params
    const cmds = Array.from(container.querySelectorAll('.outline-cmd')).map((el) => el.textContent ?? '')
    expect(cmds).toEqual(['[bg]', '[ch]', '[playbgm]'])
    expect(screen.getByText(/storage=room.png/)).toBeTruthy()

    // text dialogue line kept as content
    expect(screen.getByText('It was a quiet morning.')).toBeTruthy()
  })

  it('clicking a label heading fires the navigation handler (reveal + inspect)', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneOutlinePanel />)

    fireEvent.click(screen.getByText(/^\*next$/))
    // direct Monaco reveal
    expect(revealMock).toHaveBeenCalledWith('assets/script/main.ks', 5)

    // store reveal request (consumed by EditorArea)
    const state = useEditor.getState()
    expect(state.revealRequest).toEqual({
      path: 'assets/script/main.ks',
      line: 5,
      nonce: expect.any(Number) as unknown as number,
    })
    // unify with the G4 inspector selection
    expect(state.inspected).toEqual({ path: 'assets/script/main.ks', line: 5 })
  })

  it('shows the outline empty state for an empty / comment-only .ks doc', () => {
    useEditor.setState({
      docs: [doc('assets/script/blank.ks', '; nothing here')],
      activePath: 'assets/script/blank.ks',
    })
    render(<SceneOutlinePanel />)
    // SceneOutline's own empty hint
    expect(screen.getByText('No scene outline (empty script)')).toBeTruthy()
  })
})
