// @vitest-environment jsdom
import { describe, it, expect, vi, afterEach, beforeEach } from 'vitest'
import { cleanup, render } from '@testing-library/react'
import { registerEditor, unregisterEditor, revealEditorLine, EditorArea } from './EditorArea'
import { useEditor } from '../store'

// EditorArea pulls in @monaco-editor/react + monaco-editor; stub both so
// the module-level editor registry is testable without a Monaco instance.
vi.mock('@monaco-editor/react', () => ({
  default: () => null,
}))
vi.mock('monaco-editor', () => ({
  editor: {},
  KeyMod: { CtrlCmd: 2048 },
  KeyCode: { KeyS: 49 },
}))

interface FakeEditor {
  revealLineInCenter: ReturnType<typeof vi.fn>
  setPosition: ReturnType<typeof vi.fn>
  focus: ReturnType<typeof vi.fn>
  onDidDispose: ReturnType<typeof vi.fn>
  addCommand: ReturnType<typeof vi.fn>
}

const makeEditor = (): FakeEditor => ({
  revealLineInCenter: vi.fn(),
  setPosition: vi.fn(),
  focus: vi.fn(),
  onDidDispose: vi.fn(() => () => {}),
  addCommand: vi.fn(),
})

afterEach(() => cleanup())

describe('editor registry (EditorArea module)', () => {
  it('revealEditorLine is a no-op for an unregistered path', () => {
    const ed = makeEditor()
    revealEditorLine('assets/script/nope.ks', 5)
    expect(ed.revealLineInCenter).not.toHaveBeenCalled()
  })

  it('reveals the requested line on a registered editor', () => {
    const ed = makeEditor()
    registerEditor('assets/script/main.ks', ed as never)
    revealEditorLine('assets/script/main.ks', 12)
    expect(ed.revealLineInCenter).toHaveBeenCalledWith(12)
    expect(ed.setPosition).toHaveBeenCalledWith({ lineNumber: 12, column: 1 })
    expect(ed.focus).toHaveBeenCalled()
  })

  it('clamps line numbers to a minimum of 1', () => {
    const ed = makeEditor()
    registerEditor('assets/script/main.ks', ed as never)
    revealEditorLine('assets/script/main.ks', 0)
    expect(ed.revealLineInCenter).toHaveBeenCalledWith(1)
    expect(ed.setPosition).toHaveBeenCalledWith({ lineNumber: 1, column: 1 })
  })

  it('routes to the per-path editor when multiple are registered', () => {
    const a = makeEditor()
    const b = makeEditor()
    registerEditor('a.ks', a as never)
    registerEditor('b.ks', b as never)
    revealEditorLine('b.ks', 3)
    expect(a.revealLineInCenter).not.toHaveBeenCalled()
    expect(b.revealLineInCenter).toHaveBeenCalledWith(3)
  })

  it('stops routing after unregister', () => {
    const ed = makeEditor()
    registerEditor('assets/script/main.ks', ed as never)
    unregisterEditor('assets/script/main.ks', ed as never)
    revealEditorLine('assets/script/main.ks', 4)
    expect(ed.revealLineInCenter).not.toHaveBeenCalled()
  })

  it('unregister with a different editor instance leaves the route intact', () => {
    const registered = makeEditor()
    const other = makeEditor()
    registerEditor('assets/script/main.ks', registered as never)
    unregisterEditor('assets/script/main.ks', other as never)
    revealEditorLine('assets/script/main.ks', 2)
    expect(registered.revealLineInCenter).toHaveBeenCalledWith(2)
  })
})

// ---------------------------------------------------------------------------
// Reveal-queue consumption — the EditorArea mount effect reads
// store.revealRequest and routes it to the registered Monaco editor, keyed by
// nonce so the same request is only revealed once.
// ---------------------------------------------------------------------------

describe('EditorArea reveal-queue consumption', () => {
  beforeEach(() => {
    useEditor.setState({
      docs: [
        { path: 'assets/script/main.ks', name: 'main.ks', language: 'kag', content: '', dirty: false },
      ],
      activePath: 'assets/script/main.ks',
      sideView: 'explorer',
      engineConnected: false,
      engineScene: '',
      engineToken: 0,
      enginePaused: false,
      revealRequest: null,
      inspected: null,
    })
  })

  afterEach(() => {
    cleanup()
    useEditor.setState({ revealRequest: null })
  })

  it('consumes a reveal request on mount and routes to the registered editor', () => {
    const ed = makeEditor()
    registerEditor('assets/script/main.ks', ed as never)
    useEditor.setState({
      revealRequest: { path: 'assets/script/main.ks', line: 12, nonce: 1 },
    })
    render(<EditorArea />)
    expect(ed.revealLineInCenter).toHaveBeenCalledWith(12)
    expect(ed.setPosition).toHaveBeenCalledWith({ lineNumber: 12, column: 1 })
    expect(ed.focus).toHaveBeenCalled()
    unregisterEditor('assets/script/main.ks', ed as never)
  })

  it('does not re-reveal when the same nonce is seen again', () => {
    const ed = makeEditor()
    registerEditor('assets/script/main.ks', ed as never)
    useEditor.setState({
      revealRequest: { path: 'assets/script/main.ks', line: 5, nonce: 1 },
    })
    const { rerender } = render(<EditorArea />)
    expect(ed.revealLineInCenter).toHaveBeenCalledTimes(1)
    // same nonce re-delivered → no second reveal
    rerender(<EditorArea />)
    expect(ed.revealLineInCenter).toHaveBeenCalledTimes(1)
    unregisterEditor('assets/script/main.ks', ed as never)
  })

  it('reveals again when the nonce advances', () => {
    const ed = makeEditor()
    registerEditor('assets/script/main.ks', ed as never)
    let { rerender } = render(<EditorArea />)
    // no revealRequest initially → nothing
    expect(ed.revealLineInCenter).not.toHaveBeenCalled()

    useEditor.setState({ revealRequest: { path: 'assets/script/main.ks', line: 3, nonce: 1 } })
    rerender(<EditorArea />)
    expect(ed.revealLineInCenter).toHaveBeenCalledTimes(1)
    expect(ed.revealLineInCenter).toHaveBeenCalledWith(3)

    useEditor.setState({ revealRequest: { path: 'assets/script/main.ks', line: 20, nonce: 2 } })
    rerender(<EditorArea />)
    expect(ed.revealLineInCenter).toHaveBeenCalledTimes(2)
    expect(ed.revealLineInCenter).toHaveBeenLastCalledWith(20)
    unregisterEditor('assets/script/main.ks', ed as never)
  })

  it('clamps a requested line of 0 to 1 through the reveal path', () => {
    const ed = makeEditor()
    registerEditor('assets/script/main.ks', ed as never)
    useEditor.setState({ revealRequest: { path: 'assets/script/main.ks', line: 0, nonce: 1 } })
    render(<EditorArea />)
    expect(ed.revealLineInCenter).toHaveBeenCalledWith(1)
    expect(ed.setPosition).toHaveBeenCalledWith({ lineNumber: 1, column: 1 })
    unregisterEditor('assets/script/main.ks', ed as never)
  })

  it('does nothing when no editor is registered for the reveal path', () => {
    const ed = makeEditor() // not registered
    useEditor.setState({ revealRequest: { path: 'assets/script/main.ks', line: 9, nonce: 1 } })
    render(<EditorArea />)
    expect(ed.revealLineInCenter).not.toHaveBeenCalled()
  })
})
