// @vitest-environment jsdom
// Scene Builder panel — renders the asset palette and compose controls, and
// turns palette/compose actions into KAG .ks lines inserted into the active
// document (dirty + inspected on every insert).
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
// SceneBuilder imports revealEditorLine from EditorArea, which pulls in
// monaco-editor — not available in jsdom. Mock the module.
vi.mock('./EditorArea', () => ({
  revealEditorLine: vi.fn(),
}))
import { SceneBuilder } from './SceneBuilder'
import { revealEditorLine } from './EditorArea'
import { useEditor, type OpenDoc } from '../store'
import {
  DEMO_ASSETS,
  buildBgLine,
  buildSpriteLine,
  buildDialogueLine,
  PAGE_BREAK_LINE,
} from '../lib/sceneBuilder'

const revealMock = vi.mocked(revealEditorLine)

function doc(path: string, content: string): OpenDoc {
  return {
    path,
    name: path.split('/').pop() ?? path,
    language: 'kag',
    content,
    dirty: false,
  }
}

const KS_SOURCE = ['*start', '[bg storage="bg/room.png"]', 'It was quiet.'].join('\n')

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
    engineCmd: '',
    revealRequest: null,
    inspected: null,
    editorSelection: null,
    editorCursor: null,
  })
})

describe('SceneBuilder (asset palette)', () => {
  it('renders every demo asset from the static manifest', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneBuilder />)
    for (const a of DEMO_ASSETS) {
      expect(screen.getByText(a.label)).toBeTruthy()
    }
    // grouped panel title present
    expect(screen.getByText('Scene Builder')).toBeTruthy()
  })

  it('shows the disabled empty state when no .ks document is active', () => {
    render(<SceneBuilder />)
    expect(
      screen.getByText('Open a .ks script to place backgrounds, sprites and dialogue'),
    ).toBeTruthy()
  })

  it('shows the disabled empty state when the active doc is not a .ks script', () => {
    useEditor.setState({
      docs: [doc('assets/script/init.lua', 'return {}')],
      activePath: 'assets/script/init.lua',
    })
    render(<SceneBuilder />)
    expect(
      screen.getByText('Open a .ks script to place backgrounds, sprites and dialogue'),
    ).toBeTruthy()
  })
})

describe('SceneBuilder (bg insert)', () => {
  it('inserting a background writes the correct .ks line and marks the doc dirty', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneBuilder />)

    // pick the classroom background (line appended at the doc end: 4)
    const btn = screen.getByTitle('Insert ' + buildBgLine('bg/classroom.png'))
    fireEvent.click(btn)

    const d = useEditor.getState().docs.find((x) => x.path === 'assets/script/main.ks')
    const lines = d!.content.split('\n')
    expect(lines[3]).toBe(buildBgLine('bg/classroom.png'))
    expect(lines.length).toBe(4)
    expect(d?.dirty).toBe(true)
  })

  it('selects the inserted background line for the Inspector', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneBuilder />)
    fireEvent.click(screen.getByTitle('Insert ' + buildBgLine('bg/room.png')))
    expect(useEditor.getState().inspected).toEqual({
      path: 'assets/script/main.ks',
      line: 4,
    })
  })
})

describe('SceneBuilder (sprite / 立绘 insert)', () => {
  it('inserts a csp sprite line with the current position', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneBuilder />)

    // set x/y to 100 / 200
    // The position inputs carry no placeholder in the v1 markup; find by label "x"/"y".
    const labels = Array.from(document.querySelectorAll('.builder-pos label'))
    const x = labels.find((l) => l.textContent === 'x')!.querySelector('input')!
    const y = labels.find((l) => l.textContent === 'y')!.querySelector('input')!
    fireEvent.change(x, { target: { value: '100' } })
    fireEvent.change(y, { target: { value: '200' } })

    // The button's accessible name carries the asset basename; click it after
    // the position inputs change so the composed line uses the new x/y.
    fireEvent.click(screen.getByRole('button', { name: /girl_uniform\.png/ }))

    const d = useEditor.getState().docs.find((x) => x.path === 'assets/script/main.ks')!
    const lines = d.content.split('\n')
    expect(lines[3]).toBe(buildSpriteLine('fg/girl_uniform.png', 100, 200))
    expect(d.dirty).toBe(true)
  })
})

describe('SceneBuilder (dialogue + page break)', () => {
  it('composes a [ch] line from the name/text inputs and invents the correct statement', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneBuilder />)

    fireEvent.change(screen.getByPlaceholderText('name (optional)'), {
      target: { value: 'Hero' },
    })
    fireEvent.change(screen.getByPlaceholderText('text…'), {
      target: { value: 'Good morning' },
    })
    fireEvent.click(screen.getByRole('button', { name: /Add \[ch\]/ }))

    const d = useEditor.getState().docs.find((x) => x.path === 'assets/script/main.ks')!
    const lines = d.content.split('\n')
    expect(lines[3]).toBe(buildDialogueLine('Hero', 'Good morning'))
    expect(d.dirty).toBe(true)
    // compose inputs are cleared after a successful insert
    expect((screen.getByPlaceholderText('name (optional)') as HTMLInputElement).value).toBe('')
    expect((screen.getByPlaceholderText('text…') as HTMLInputElement).value).toBe('')
  })

  it('disables the Add [ch] button while the text is blank', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneBuilder />)
    const btn = screen.getByRole('button', { name: /Add \[ch\]/ }) as HTMLButtonElement
    expect(btn.disabled).toBe(true)
  })

  it('inserts a [p] page-break line and selects it for the Inspector', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneBuilder />)
    fireEvent.click(screen.getByRole('button', { name: /Add \[p\]/ }))

    const d = useEditor.getState().docs.find((x) => x.path === 'assets/script/main.ks')!
    const lines = d.content.split('\n')
    expect(lines[3]).toBe(PAGE_BREAK_LINE)
    expect(useEditor.getState().inspected).toEqual({ path: 'assets/script/main.ks', line: 4 })
  })
})

describe('SceneBuilder (insert pointer + inspector/reveal)', () => {
  it('inserts at the tracked cursor line instead of the document end', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
      editorCursor: { path: 'assets/script/main.ks', line: 2, column: 1 },
    })
    render(<SceneBuilder />)
    fireEvent.click(screen.getByTitle('Insert ' + buildBgLine('bg/stage.png')))

    const d = useEditor.getState().docs.find((x) => x.path === 'assets/script/main.ks')!
    const lines = d.content.split('\n')
    // line 2 (index 1) now holds the new bg; original line 2 shifted down.
    expect(lines[1]).toBe(buildBgLine('bg/stage.png'))
    expect(lines[2]).toBe('[bg storage="bg/room.png"]')
  })

  it('reveals the inserted line in the editor', () => {
    useEditor.setState({
      docs: [doc('assets/script/main.ks', KS_SOURCE)],
      activePath: 'assets/script/main.ks',
    })
    render(<SceneBuilder />)
    fireEvent.click(screen.getByTitle('Insert ' + buildBgLine('bg/classroom.png')))
    expect(revealMock).toHaveBeenCalledWith('assets/script/main.ks', 4)
  })
})
