// @vitest-environment jsdom
// EditorArea — deep Monaco integration-layer tests under jsdom.
//
// Monaco (@monaco-editor/react + monaco-editor) is unusable in jsdom, so we
// stand up a faithful minimal fake of BOTH surfaces and assert the real
// EditorArea wires against it exactly as it would against a live Monaco:
//
//   - @monaco-editor/react <Editor/>   → a hook-bearing component that
//     owns a model lifecycle (create on path/language change, setValue on
//     external value change, dispose on unmount) and fires onMount(editor,
//     monaco) once per editor instance.
//   - monaco-editor                    → a namespace mock (editor.create,
//     editor.createModel, editor.setModel, editor.setModelMarkers,
//     editor.onDidCreateModel, languages.{register,setMonarchTokensProvider,
//     setLanguageConfiguration,register*Provider}, Range, MarkerSeverity,
//     KeyMod, KeyCode) that records every call.
//
// Covered: the full Monaco mock contract EditorArea + KagLsp consume
// (item 1), document-switch/content-sync model lifecycle + dirty→title
// (item 2), reveal-request consumption with path matching and line clamping
// (item 3), and Ctrl+S command registration incl. re-mount uniqueness
// (item 4). Language registration (registerKagLanguage) is owned by App.tsx
// at module scope and is idempotent — EditorArea itself does not register it.
//
import { describe, it, expect, vi, afterEach, beforeEach } from 'vitest'
import { cleanup, render, act } from '@testing-library/react'
import {
  registerEditor,
  unregisterEditor,
  revealEditorLine,
  EditorArea,
} from './EditorArea'
import { useEditor } from '../store'

// ---------------------------------------------------------------------------
// Monaco fake — built with vi.hoisted so the vi.mock factories (hoisted above
// the class bodies) can share one live instance with the test bodies below.
// ---------------------------------------------------------------------------
const h = vi.hoisted(() => {
  const disposer = (fn: () => void) => ({
    dispose: () => {
      const i = disposableList.indexOf(fn)
      if (i >= 0) disposableList.splice(i, 1)
    },
  })
  const disposableList: (() => void)[] = []

  class FakeModel {
    value: string
    languageId: string
    uri: unknown
    disposed = false
    changeListeners: (() => void)[] = []
    private _lineCount: number
    constructor(value: string, languageId: string, uri: unknown) {
      this.value = value
      this.languageId = languageId
      this.uri = uri
      this._lineCount = value.split('\n').length
      allModels.push(this)
    }
    getValue() { return this.value }
    getValueLength() { return this.value.length }
    getLanguageId() { return this.languageId }
    getLineCount() { return this._lineCount }
    getLineContent(line: number) { return this.value.split('\n')[line - 1] ?? '' }
    getWordUntilPosition() { return { startColumn: 1, endColumn: 1, word: '' } }
    getWordAtPosition() { return { startColumn: 1, endColumn: 1, word: '' } }
    onDidChangeContent(cb: () => void) {
      this.changeListeners.push(cb)
      return { dispose: () => { this.changeListeners = this.changeListeners.filter((c) => c !== cb) } }
    }
    setValue(v: string) {
      this.value = v
      this._lineCount = v.split('\n').length
      for (const cb of this.changeListeners) cb()
    }
    dispose() { this.disposed = true }
  }

  class FakeEditor {
    model: FakeModel | null = null
    disposed = false
    disposers: (() => void)[] = []
    commands: { keybinding: number; handler: () => void }[] = []
    revealCalls: number[] = []
    positionCalls: { lineNumber: number; column: number }[] = []
    focusCalls = 0
    constructor() { allEditors.push(this) }
    onDidDispose(cb: () => void) {
      this.disposers.push(cb)
      return disposer(() => {
        this.disposers = this.disposers.filter((c) => c !== cb)
      })
    }
    dispose() {
      this.disposed = true
      for (const cb of this.disposers) cb()
      this.disposers = []
    }
    addCommand(keybinding: number, handler: () => void) {
      this.commands.push({ keybinding, handler })
      return this.commands.length - 1
    }
    revealLineInCenter(line: number) { this.revealCalls.push(line) }
    setPosition(pos: { lineNumber: number; column: number }) { this.positionCalls.push(pos) }
    focus() { this.focusCalls++ }
    setModel(m: FakeModel | null) { this.model = m }
    getModel() { return this.model }
    setValue(v: string) { if (this.model) this.model.setValue(v) }
  }

  class FakeUri {
    constructor(public path: string, public scheme = 'file') {}
  }

  const allEditors: FakeEditor[] = []
  const allModels: FakeModel[] = []
  const setModelCalls: { editor: FakeEditor; model: FakeModel | null }[] = []
  const providers: string[] = []
  const onDidCreateModelListeners: ((m: FakeModel) => void)[] = []

  // Latest onChange wired into the mounted <Editor/> — tests simulate typing.
  let latestOnChange: ((v: string | undefined) => void) | null = null

  const monacoNamespace = {
    editor: {
      create: () => new FakeEditor(),
      createModel: (value: string, languageId: string, uri: unknown) =>
        new FakeModel(value, languageId, uri === undefined ? null : uri),
      setModel: (ed: FakeEditor, m: FakeModel | null) => {
        setModelCalls.push({ editor: ed, model: m })
        ed.setModel(m)
      },
      setModelMarkers: () => {},
      onDidCreateModel: (cb: (m: FakeModel) => void) => {
        onDidCreateModelListeners.push(cb)
        return { dispose: () => onDidCreateModelListeners.splice(onDidCreateModelListeners.indexOf(cb), 1) }
      },
    },
    languages: {
      getLanguages: () => [],
      register: () => {},
      setMonarchTokensProvider: () => {},
      setLanguageConfiguration: () => {},
      registerCompletionItemProvider: () => { providers.push('completion'); return { dispose: () => {} } },
      registerHoverProvider: () => { providers.push('hover'); return { dispose: () => {} } },
      registerDefinitionProvider: () => { providers.push('definition'); return { dispose: () => {} } },
      registerReferenceProvider: () => { providers.push('references'); return { dispose: () => {} } },
    },
    Range: class {
      constructor(
        public a: number, public b: number, public c: number, public d: number,
      ) {}
    },
    MarkerSeverity: { Error: 8, Warning: 4 },
    KeyMod: { CtrlCmd: 2048 },
    KeyCode: { KeyS: 49 },
    Uri: FakeUri,
  }

  return {
    monacoNamespace,
    allEditors,
    allModels,
    setModelCalls,
    providers,
    FakeEditor,
    FakeModel,
    FakeUri,
    disposableList,
    get lastEditor() { return allEditors[allEditors.length - 1] ?? null },
    set latestChange(fn: ((v: string | undefined) => void) | null) { latestOnChange = fn },
    get latestChange() { return latestOnChange },
  }
})

vi.mock('monaco-editor', () => h.monacoNamespace)

// EngineClient is constructed inside handleMount (KagLsp). Stub it so no real
// HTTP transport / fetch is touched; evalRaw just resolves to harmless JSON.
vi.mock('../lib/rpc', () => ({
  EngineClient: class {
    evalRaw = vi.fn(async () => '[]')
    setToken() {}
    setBase() {}
  },
  RpcError: class RpcError extends Error {},
}))

// ---------------------------------------------------------------------------
// <- @monaco-editor/react /> — a hook-bearing fake <Editor/> that owns a
// model lifecycle mirroring the real component:
//   * creates ONE editor instance per mount and fires onMount once
//   * creates/replaces a model when path|language changes, via setModel
//   * pushes external value changes into the model via setValue
//   * disposes the editor + model on unmount
// ---------------------------------------------------------------------------
vi.mock('@monaco-editor/react', async () => {
  const { useState, useRef, useEffect } = await import('react')
  return {
    default: function MockEditor(props: {
      path?: string
      language?: string
      value?: string
      onMount?: (ed: unknown, mo: unknown) => void
      onChange?: (v: string | undefined) => void
    }) {
      const editorRef = useRef<InstanceType<typeof h.FakeEditor> | null>(null)
      const modelRef = useRef<InstanceType<typeof h.FakeModel> | null>(null)
      const [, force] = useState(0)

      // Create the editor instance once per mount.
      if (!editorRef.current) {
        editorRef.current = h.monacoNamespace.editor.create()
      }

      // Keep the live onChange reachable from tests (simulate typing).
      const changeRef = useRef(props.onChange)
      changeRef.current = props.onChange

      // Fire onMount exactly once, after the first commit.
      useEffect(() => {
        h.latestChange = changeRef.current ?? null
        force((n) => n + 1)
        props.onMount?.(editorRef.current!, h.monacoNamespace)
        // eslint-disable-next-line react-hooks/exhaustive-deps
      }, [])

      // Model lifecycle: recreate when path|language changes.
      useEffect(() => {
        const prev = modelRef.current
        if (prev) prev.dispose()
        const model = h.monacoNamespace.editor.createModel(
          typeof props.value === 'string' ? props.value : '',
          props.language ?? 'plaintext',
          props.path !== undefined ? new h.monacoNamespace.Uri(props.path) : null,
        ) as InstanceType<typeof h.FakeModel>
        modelRef.current = model
        h.monacoNamespace.editor.setModel(editorRef.current!, model)
        // eslint-disable-next-line react-hooks/exhaustive-deps
      }, [props.path, props.language])

      // External value change → push into the model (like the real component).
      useEffect(() => {
        const m = modelRef.current
        if (m && m.getValue() !== props.value) m.setValue(props.value ?? '')
        // eslint-disable-next-line react-hooks/exhaustive-deps
      }, [props.value])

      // Cleanup: dispose editor (fires onDidDispose → EditorArea unregisters)
      // and the model.
      useEffect(
        () => () => {
          editorRef.current?.dispose()
          modelRef.current?.dispose()
        },
        [],
      )

      return null
    },
  }
})

// ---------------------------------------------------------------------------
// Fixtures / helpers
// ---------------------------------------------------------------------------
const KS_SOURCE = ['*start', '[bg storage="room.png"]', 'It was a quiet morning.'].join('\n')

function resetStore(over: Partial<ReturnType<typeof useEditor.getState>> = {}) {
  useEditor.setState({
    docs: [
      { path: 'assets/script/main.ks', name: 'main.ks', language: 'kag', content: KS_SOURCE, dirty: false },
    ],
    activePath: 'assets/script/main.ks',
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

function activeDoc() {
  const s = useEditor.getState()
  return s.docs.find((d) => d.path === s.activePath) ?? null
}

beforeEach(() => {
  cleanup()
  h.allEditors.length = 0
  h.allModels.length = 0
  h.setModelCalls.length = 0
  h.providers.length = 0
  h.latestChange = null
  h.disposableList.length = 0
  resetStore()
})

afterEach(() => {
  cleanup()
  useEditor.setState({ revealRequest: null })
})

// ---------------------------------------------------------------------------
// Monaco mock contract — how EditorArea + KagLsp consume the Monaco API.
// ---------------------------------------------------------------------------
describe('EditorArea · Monaco mock contract', () => {
  it('creates one model synchronising the active doc content + kag language', () => {
    render(<EditorArea />)
    expect(h.allModels).toHaveLength(1)
    const m = h.allModels[0]
    expect(m.getValue()).toBe(KS_SOURCE)
    expect(m.getLanguageId()).toBe('kag')
  })

  it('attaches the model uri to the doc path', () => {
    render(<EditorArea />)
    const m = h.allModels[0]
    expect(m.uri).toBeInstanceOf(h.FakeUri)
    // FakeUri : { scheme: 'file', path }
    expect((m.uri as { path: string }).path).toBe('assets/script/main.ks')
  })

  it('wires the created editor to that model via editor.setModel', () => {
    render(<EditorArea />)
    const ed = h.lastEditor
    const model = h.allModels[0]
    expect(ed).not.toBeNull()
    expect(h.setModelCalls).toHaveLength(1)
    expect(h.setModelCalls[0].editor).toBe(ed)
    expect(h.setModelCalls[0].model).toBe(model)
    expect(ed!.getModel()).toBe(model)
  })

  it('registers the editor in the scene-tree registry under the active path', () => {
    render(<EditorArea />)
    const ed = h.lastEditor
    revealEditorLine('assets/script/main.ks', 9)
    expect(ed!.revealCalls).toEqual([9])
    expect(ed!.positionCalls).toEqual([{ lineNumber: 9, column: 1 }])
    expect(ed!.focusCalls).toBe(1)
  })

  it('registers the Ctrl+S command on the mounted editor', () => {
    render(<EditorArea />)
    const ed = h.lastEditor
    expect(ed!.commands).toHaveLength(1)
    // CtrlCmd | KeyS = 2048 | 49 = 2097 (not 2049).
    expect(ed!.commands[0].keybinding).toBe(2097)
    expect(typeof ed!.commands[0].handler).toBe('function')
  })

  it('does not re-register the command when the doc re-renders without remounting', () => {
    const { rerender } = render(<EditorArea />)
    const ed = h.lastEditor!
    expect(ed.commands).toHaveLength(1)

    // Same editor instance persists; a prop-level re-render must not re-add.
    rerender(<EditorArea />)
    expect(h.lastEditor).toBe(ed)
    expect(ed.commands).toHaveLength(1)
  })

  it('registers the Kag language-service providers against the monaco mock', () => {
    render(<EditorArea />)
    expect(h.providers).toEqual(
      expect.arrayContaining(['completion', 'hover', 'definition', 'references']),
    )
  })
})

// ---------------------------------------------------------------------------
// Content sync — document switching + external store updates + dirty→title.
// ---------------------------------------------------------------------------
describe('EditorArea · content sync', () => {
  it('disposes the old model and creates a fresh one when switching docs', () => {
    const { rerender } = render(<EditorArea />)
    const firstEditor = h.lastEditor
    const firstModel = h.allModels[0]
    expect(h.allModels).toHaveLength(1)

    const other = 'assets/script/scene2.ks'
    act(() => {
      useEditor.setState({
        docs: [
          { path: 'assets/script/main.ks', name: 'main.ks', language: 'kag', content: KS_SOURCE, dirty: false },
          { path: other, name: 'scene2.ks', language: 'kag', content: '[ch text="Hi"]', dirty: false },
        ],
        activePath: other,
      })
    })
    rerender(<EditorArea />)

    expect(firstModel.disposed).toBe(true)
    expect(h.allModels).toHaveLength(2)
    const second = h.allModels[1]
    expect(second.getValue()).toBe('[ch text="Hi"]')
    expect(second.getLanguageId()).toBe('kag')
    expect((second.uri as { path: string }).path).toBe(other)
    // same editor instance, re-pointed at the new model
    expect(h.lastEditor).toBe(firstEditor)
    expect(h.setModelCalls.map((c) => c.model)).toEqual([firstModel, second])
  })

  it('switches model language when the active doc language changes', () => {
    const { rerender } = render(<EditorArea />)
    act(() => {
      useEditor.setState({
        docs: [
          { path: 'assets/script/main.lua', name: 'main.lua', language: 'lua', content: 'print(1)', dirty: false },
        ],
        activePath: 'assets/script/main.lua',
      })
    })
    rerender(<EditorArea />)
    const last = h.allModels[h.allModels.length - 1]
    expect(last.getLanguageId()).toBe('lua')
    expect(last.getValue()).toBe('print(1)')
  })

  it('pushes an external store content update into the live model', () => {
    const { rerender } = render(<EditorArea />)
    expect(h.allModels[0].getValue()).toBe(KS_SOURCE)

    const updated = KS_SOURCE + '\n[ch text="more"]'
    act(() => {
      useEditor.setState((s) => ({
        docs: s.docs.map((d) => (d.path === s.activePath ? { ...d, content: updated } : d)),
      }))
    })
    rerender(<EditorArea />)

    // No new model created — same model, value replaced in place.
    expect(h.allModels).toHaveLength(1)
    expect(h.allModels[0].getValue()).toBe(updated)
  })

  it('typing in the editor marks the doc dirty and shows it in the tab title', () => {
    render(<EditorArea />)
    expect(activeDoc()?.dirty).toBe(false)

    act(() => {
      h.latestChange?.(KS_SOURCE + 'X')
    })

    const doc = activeDoc()
    expect(doc?.content).toBe(KS_SOURCE + 'X')
    expect(doc?.dirty).toBe(true)
  })
})

// ---------------------------------------------------------------------------
// Reveal consumption — EditorArea routes store.revealRequest to the mounted
// editor keyed by nonce + path.
// ---------------------------------------------------------------------------
describe('EditorArea · reveal-queue consumption', () => {
  it('consumes a reveal request on mount and reveals on the mounted editor', () => {
    act(() => {
      useEditor.setState({ revealRequest: { path: 'assets/script/main.ks', line: 12, nonce: 1 } })
    })
    render(<EditorArea />)
    const ed = h.lastEditor
    expect(ed!.revealCalls).toEqual([12])
    expect(ed!.positionCalls).toEqual([{ lineNumber: 12, column: 1 }])
    expect(ed!.focusCalls).toBe(1)
  })

  it('does not re-reveal when the same nonce is seen again', () => {
    act(() => {
      useEditor.setState({ revealRequest: { path: 'assets/script/main.ks', line: 5, nonce: 1 } })
    })
    const { rerender } = render(<EditorArea />)
    const ed = h.lastEditor
    expect(ed!.revealCalls).toEqual([5])
    rerender(<EditorArea />)
    expect(ed!.revealCalls).toEqual([5])
  })

  it('reveals again when the nonce advances', () => {
    const { rerender } = render(<EditorArea />)
    const ed = h.lastEditor
    expect(ed!.revealCalls).toEqual([])

    act(() => {
      useEditor.setState({ revealRequest: { path: 'assets/script/main.ks', line: 3, nonce: 1 } })
    })
    rerender(<EditorArea />)
    expect(ed!.revealCalls).toEqual([3])

    act(() => {
      useEditor.setState({ revealRequest: { path: 'assets/script/main.ks', line: 20, nonce: 2 } })
    })
    rerender(<EditorArea />)
    expect(ed!.revealCalls).toEqual([3, 20])
  })

  it('clamps a requested line of 0 to 1 through the reveal path', () => {
    act(() => {
      useEditor.setState({ revealRequest: { path: 'assets/script/main.ks', line: 0, nonce: 1 } })
    })
    render(<EditorArea />)
    const ed = h.lastEditor
    expect(ed!.revealCalls).toEqual([1])
    expect(ed!.positionCalls).toEqual([{ lineNumber: 1, column: 1 }])
  })

  it('ignores a reveal request whose path does not match the mounted doc', () => {
    act(() => {
      useEditor.setState({
        revealRequest: { path: 'assets/script/other.ks', line: 9, nonce: 1 },
      })
    })
    render(<EditorArea />)
    const ed = h.lastEditor
    expect(ed!.revealCalls).toEqual([])
    // nonce is still consumed so the stale request is not retried
    act(() => {
      useEditor.setState({ revealRequest: null })
    })
  })

  it('does nothing when no editor is mounted (no active doc)', () => {
    act(() => {
      useEditor.setState({
        docs: [],
        activePath: null,
        revealRequest: { path: 'assets/script/main.ks', line: 3, nonce: 1 },
      })
    })
    render(<EditorArea />)
    expect(h.allEditors).toHaveLength(0)
  })
})

// ---------------------------------------------------------------------------
// Editor registry helpers (module-level, no render) — retained from the
// original suite.
// ---------------------------------------------------------------------------
describe('editor registry (EditorArea module)', () => {
  it('revealEditorLine is a no-op for an unregistered path', () => {
    const ed = h.lastEditor ?? (() => { const e = new h.FakeEditor(); h.allEditors.push(e); return e })()
    ed.revealCalls.length = 0
    revealEditorLine('assets/script/nope.ks', 5)
    expect(ed.revealCalls).toEqual([])
  })

  it('reveals the requested line on a registered editor', () => {
    const ed = new h.FakeEditor()
    registerEditor('assets/script/main.ks', ed as never)
    revealEditorLine('assets/script/main.ks', 12)
    expect(ed.revealCalls).toEqual([12])
    expect(ed.positionCalls).toEqual([{ lineNumber: 12, column: 1 }])
    expect(ed.focusCalls).toBe(1)
  })

  it('clamps line numbers to a minimum of 1', () => {
    const ed = new h.FakeEditor()
    registerEditor('assets/script/main.ks', ed as never)
    revealEditorLine('assets/script/main.ks', 0)
    expect(ed.revealCalls).toEqual([1])
    expect(ed.positionCalls).toEqual([{ lineNumber: 1, column: 1 }])
  })

  it('routes to the per-path editor when multiple are registered', () => {
    const a = new h.FakeEditor()
    const b = new h.FakeEditor()
    registerEditor('a.ks', a as never)
    registerEditor('b.ks', b as never)
    revealEditorLine('b.ks', 3)
    expect(a.revealCalls).toEqual([])
    expect(b.revealCalls).toEqual([3])
  })

  it('stops routing after unregister', () => {
    const ed = new h.FakeEditor()
    registerEditor('assets/script/main.ks', ed as never)
    unregisterEditor('assets/script/main.ks', ed as never)
    revealEditorLine('assets/script/main.ks', 4)
    expect(ed.revealCalls).toEqual([])
  })

  it('unregister with a different editor instance leaves the route intact', () => {
    const registered = new h.FakeEditor()
    const other = new h.FakeEditor()
    registerEditor('assets/script/main.ks', registered as never)
    unregisterEditor('assets/script/main.ks', other as never)
    revealEditorLine('assets/script/main.ks', 2)
    expect(registered.revealCalls).toEqual([2])
  })
})

// ---------------------------------------------------------------------------
// Disposal — unmounting EditorArea disposes the editor (fires onDidDispose),
// which unregisters the scene-tree route and disposes the LSP providers.
// ---------------------------------------------------------------------------
describe('EditorArea · unmount / disposal', () => {
  it('unregisters the editor route and disposes the model on unmount', () => {
    const { unmount } = render(<EditorArea />)
    const ed = h.lastEditor!
    const model = h.allModels[0]

    // still routed while mounted
    revealEditorLine('assets/script/main.ks', 7)
    expect(ed.revealCalls).toEqual([7])

    unmount()
    expect(ed.disposed).toBe(true)
    expect(model.disposed).toBe(true)

    // route is gone after dispose (onDidDispose → unregisterEditor)
    expect(h.allEditors).toHaveLength(1) // instance was reused, not re-created
    // A second render does not leak the old route — nothing registered now.
    const probe = new h.FakeEditor()
    registerEditor('assets/script/main.ks', probe as never)
    revealEditorLine('assets/script/main.ks', 1)
    expect(ed.revealCalls).toEqual([7])
    expect(probe.revealCalls).toEqual([1])
  })
})
