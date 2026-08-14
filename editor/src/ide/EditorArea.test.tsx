// @vitest-environment jsdom
import { describe, it, expect, vi, afterEach } from 'vitest'
import { cleanup } from '@testing-library/react'
import { registerEditor, unregisterEditor, revealEditorLine } from './EditorArea'

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
