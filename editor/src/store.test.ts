// Editor global store — deep coverage of every action in editor/src/store.ts.
// These tests drive the zustand store directly (no DOM) to assert the exact
// state transitions each action produces, including edge/empty states.
import { describe, it, expect, beforeEach } from 'vitest'
import { useEditor, resolveInsertLine, type OpenDoc, type SideView } from './store'
import type { EngineClient } from './lib/rpc'

const doc = (path: string, partial: Partial<OpenDoc> = {}): OpenDoc => ({
  path,
  name: path.split('/').pop() ?? path,
  language: path.endsWith('.ks') ? 'kag' : 'lua',
  content: '',
  dirty: false,
  ...partial,
})

beforeEach(() => {
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
  })
  useEditor.setState({ editorCursor: null })
})

describe('useEditor.openDoc', () => {
  it('appends a new document and makes it active', () => {
    const { openDoc } = useEditor.getState()
    openDoc(doc('assets/script/main.ks'))
    const s = useEditor.getState()
    expect(s.docs).toHaveLength(1)
    expect(s.docs[0].path).toBe('assets/script/main.ks')
    expect(s.activePath).toBe('assets/script/main.ks')
  })

  it('opening an already-open path only activates it (no duplicate)', () => {
    const { openDoc } = useEditor.getState()
    openDoc(doc('a.ks', { content: 'v1' }))
    openDoc(doc('b.ks'))
    openDoc(doc('a.ks', { content: 'v2' })) // same path, different content
    const s = useEditor.getState()
    expect(s.docs).toHaveLength(2)
    // original content is preserved (openDoc of existing doc is a no-op merge)
    expect(s.docs.find((d) => d.path === 'a.ks')?.content).toBe('v1')
    expect(s.activePath).toBe('a.ks')
  })

  it('opens many docs in order, each becoming active', () => {
    const { openDoc } = useEditor.getState()
    openDoc(doc('a.ks'))
    expect(useEditor.getState().activePath).toBe('a.ks')
    openDoc(doc('b.ks'))
    expect(useEditor.getState().activePath).toBe('b.ks')
    openDoc(doc('c.ks'))
    expect(useEditor.getState().activePath).toBe('c.ks')
    expect(useEditor.getState().docs.map((d) => d.path)).toEqual(['a.ks', 'b.ks', 'c.ks'])
  })
})

describe('useEditor.closeDoc', () => {
  it('removes the document from the list', () => {
    const { openDoc, closeDoc } = useEditor.getState()
    openDoc(doc('a.ks'))
    openDoc(doc('b.ks'))
    closeDoc('a.ks')
    const s = useEditor.getState()
    expect(s.docs.map((d) => d.path)).toEqual(['b.ks'])
  })

  it('falls back to the previous tab when closing the active doc', () => {
    const { openDoc, closeDoc } = useEditor.getState()
    openDoc(doc('a.ks'))
    openDoc(doc('b.ks'))
    openDoc(doc('c.ks'))
    closeDoc('c.ks')
    expect(useEditor.getState().activePath).toBe('b.ks')
  })

  it('activates the doc that slid into the closed slot when there is no previous', () => {
    const { openDoc, closeDoc } = useEditor.getState()
    openDoc(doc('a.ks'))
    openDoc(doc('b.ks'))
    // close the first active doc; no previous tab exists, so the next (b) wins
    closeDoc('a.ks')
    expect(useEditor.getState().activePath).toBe('b.ks')
  })

  it('leaves activePath alone when closing a non-active doc', () => {
    const { openDoc, closeDoc } = useEditor.getState()
    openDoc(doc('a.ks'))
    openDoc(doc('b.ks'))
    useEditor.getState().setActive('b.ks')
    closeDoc('a.ks')
    expect(useEditor.getState().activePath).toBe('b.ks')
  })

  it('clears activePath to null when closing the last open doc', () => {
    const { openDoc, closeDoc } = useEditor.getState()
    openDoc(doc('a.ks'))
    closeDoc('a.ks')
    const s = useEditor.getState()
    expect(s.docs).toHaveLength(0)
    expect(s.activePath).toBeNull()
  })

  it('is a no-op for a path that is not open', () => {
    const { openDoc, closeDoc } = useEditor.getState()
    openDoc(doc('a.ks'))
    closeDoc('nope.ks')
    const s = useEditor.getState()
    expect(s.docs).toHaveLength(1)
    expect(s.activePath).toBe('a.ks')
  })
})

describe('useEditor.setActive', () => {
  it('switches the active document path', () => {
    const { openDoc, setActive } = useEditor.getState()
    openDoc(doc('a.ks'))
    openDoc(doc('b.ks'))
    setActive('a.ks')
    expect(useEditor.getState().activePath).toBe('a.ks')
    setActive('b.ks')
    expect(useEditor.getState().activePath).toBe('b.ks')
  })

  it('accepts a path that is not yet open (no validation)', () => {
    useEditor.getState().setActive('ghost.ks')
    expect(useEditor.getState().activePath).toBe('ghost.ks')
  })
})

describe('useEditor.updateDoc', () => {
  it('replaces content and marks the doc dirty', () => {
    const { openDoc, updateDoc } = useEditor.getState()
    openDoc(doc('a.ks', { content: 'one' }))
    updateDoc('a.ks', 'two')
    const d = useEditor.getState().docs[0]
    expect(d.content).toBe('two')
    expect(d.dirty).toBe(true)
  })

  it('leaves other documents untouched', () => {
    const { openDoc, updateDoc } = useEditor.getState()
    openDoc(doc('a.ks'))
    openDoc(doc('b.ks'))
    updateDoc('a.ks', 'changed')
    const b = useEditor.getState().docs.find((d) => d.path === 'b.ks')
    expect(b?.content).toBe('')
    expect(b?.dirty).toBe(false)
  })

  it('is a no-op when the path is not open', () => {
    useEditor.getState().updateDoc('missing.ks', 'x')
    expect(useEditor.getState().docs).toHaveLength(0)
  })
})

describe('useEditor.setSideView', () => {
  it.each<SideView>(['explorer', 'debug', 'visual', 'ai'])(
    'switches to %s',
    (view) => {
      useEditor.getState().setSideView(view)
      expect(useEditor.getState().sideView).toBe(view)
    },
  )
})

describe('useEditor.setEngine', () => {
  it('merges partial engine state and preserves the rest', () => {
    const { setEngine } = useEditor.getState()
    setEngine({ engineConnected: true })
    let s = useEditor.getState()
    expect(s.engineConnected).toBe(true)
    expect(s.engineScene).toBe('')
    setEngine({ engineScene: 'chapter1/town', engineToken: 9, enginePaused: true, engineCmd: '[ch]' })
    s = useEditor.getState()
    expect(s.engineScene).toBe('chapter1/town')
    expect(s.engineToken).toBe(9)
    expect(s.enginePaused).toBe(true)
    expect(s.engineCmd).toBe('[ch]')
    // connected persists from the earlier partial update
    expect(s.engineConnected).toBe(true)
  })

  it('writing unrelated fields does not clobber engine state', () => {
    const { setEngine, openDoc, setSideView } = useEditor.getState()
    openDoc(doc('a.ks'))
    setEngine({ engineConnected: true, engineScene: 'x', engineToken: 1, enginePaused: false, engineCmd: '' })
    setSideView('ai')
    const s = useEditor.getState()
    expect(s.sideView).toBe('ai')
    expect(s.engineConnected).toBe(true)
    expect(s.docs).toHaveLength(1)
  })

  it('toggles each field independently', () => {
    const { setEngine } = useEditor.getState()
    setEngine({ enginePaused: true })
    expect(useEditor.getState().enginePaused).toBe(true)
    setEngine({ enginePaused: false })
    expect(useEditor.getState().enginePaused).toBe(false)
  })
})

describe('useEditor.insertIntoActive', () => {
  it('appends text to the active doc and marks it dirty', () => {
    const { openDoc, setActive, insertIntoActive } = useEditor.getState()
    openDoc(doc('a.ks', { content: 'head' }))
    openDoc(doc('b.ks', { content: 'B' }))
    setActive('a.ks')
    insertIntoActive('[ch text="hi"]')
    const a = useEditor.getState().docs.find((d) => d.path === 'a.ks')
    const b = useEditor.getState().docs.find((d) => d.path === 'b.ks')
    expect(a?.content).toBe('head[ch text="hi"]')
    expect(a?.dirty).toBe(true)
    expect(b?.content).toBe('B') // untouched
  })

  it('is a no-op when no document is active', () => {
    useEditor.getState().insertIntoActive('x')
    expect(useEditor.getState().docs).toHaveLength(0)
  })
})

describe('useEditor.requestReveal (nonce semantics)', () => {
  it('sets activePath and enqueues a reveal with nonce 1 from a clean state', () => {
    const { openDoc, requestReveal } = useEditor.getState()
    openDoc(doc('a.ks'))
    requestReveal('a.ks', 15)
    const s = useEditor.getState()
    expect(s.revealRequest).toEqual({ path: 'a.ks', line: 15, nonce: 1 })
    expect(s.activePath).toBe('a.ks')
  })

  it('increments the nonce monotonically across reveals', () => {
    const { requestReveal } = useEditor.getState()
    requestReveal('a.ks', 1)
    requestReveal('a.ks', 2)
    requestReveal('b.ks', 3)
    expect(useEditor.getState().revealRequest).toEqual({ path: 'b.ks', line: 3, nonce: 3 })
  })

  it('keeps a previous nonce value and bumps from it (resume scenario)', () => {
    const { requestReveal } = useEditor.getState()
    requestReveal('a.ks', 1)
    useEditor.setState({ revealRequest: { path: 'a.ks', line: 1, nonce: 100 } })
    requestReveal('a.ks', 50)
    expect(useEditor.getState().revealRequest?.nonce).toBe(101)
  })

  it('re-requesting the same path+line still bumps the nonce', () => {
    const { requestReveal } = useEditor.getState()
    requestReveal('a.ks', 7)
    requestReveal('a.ks', 7)
    expect(useEditor.getState().revealRequest).toEqual({ path: 'a.ks', line: 7, nonce: 2 })
  })
})

describe('useEditor.setInspected', () => {
  it('records the inspected element', () => {
    const { setInspected } = useEditor.getState()
    setInspected('assets/script/main.ks', 3)
    expect(useEditor.getState().inspected).toEqual({ path: 'assets/script/main.ks', line: 3 })
  })

  it('replaces a previous inspection', () => {
    const { setInspected } = useEditor.getState()
    setInspected('a.ks', 1)
    setInspected('b.ks', 9)
    expect(useEditor.getState().inspected).toEqual({ path: 'b.ks', line: 9 })
  })

  it('does not disturb revealRequest or activePath', () => {
    const { openDoc, setInspected } = useEditor.getState()
    openDoc(doc('a.ks'))
    setInspected('a.ks', 2)
    const s = useEditor.getState()
    expect(s.inspected).toEqual({ path: 'a.ks', line: 2 })
    expect(s.revealRequest).toBeNull()
  })
})


describe('useEditor.setCursor', () => {
  it('records the live editor cursor (or null when cleared)', () => {
    const { setCursor } = useEditor.getState()
    setCursor({ path: 'a.ks', line: 3, column: 5 })
    expect(useEditor.getState().editorCursor).toEqual({ path: 'a.ks', line: 3, column: 5 })
    setCursor({ path: 'b.ks', line: 1, column: 1 })
    expect(useEditor.getState().editorCursor).toEqual({ path: 'b.ks', line: 1, column: 1 })
    setCursor(null)
    expect(useEditor.getState().editorCursor).toBeNull()
  })
})

describe('useEditor.insertAtCursor', () => {
  it('inserts the statement just above the tracked cursor line', () => {
    const { openDoc, setCursor, insertAtCursor } = useEditor.getState()
    openDoc(doc('a.ks', { content: ['*start', '[bg storage="room.png"]', 'Hello'].join('\n') }))
    setCursor({ path: 'a.ks', line: 3, column: 1 })
    insertAtCursor('[ch name="Hero" text="Hi"]')
    const d = useEditor.getState().docs.find((x) => x.path === 'a.ks')
    expect(d?.content).toBe(
      ['*start', '[bg storage="room.png"]', '[ch name="Hero" text="Hi"]', 'Hello'].join('\n'),
    )
    expect(d?.dirty).toBe(true)
  })

  it('appends to the end when no cursor is tracked for the active doc', () => {
    const { openDoc, setCursor, insertAtCursor } = useEditor.getState()
    openDoc(doc('a.ks', { content: 'one' }))
    setCursor({ path: 'other.ks', line: 1, column: 1 }) // cursor is on a different doc
    insertAtCursor('[p]')
    const d = useEditor.getState().docs.find((x) => x.path === 'a.ks')
    // insertAtCursor always places a statement on its own line (append here).
    expect(d?.content).toBe('one\n[p]')
  })

  it('appends when editorCursor is null', () => {
    const { openDoc, insertAtCursor } = useEditor.getState()
    openDoc(doc('a.ks', { content: 'tail' }))
    insertAtCursor('[p]')
    expect(useEditor.getState().docs[0].content).toBe('tail\n[p]')
  })

  it('clamps the insert line to the document when the cursor is past the end', () => {
    const { openDoc, setCursor, insertAtCursor } = useEditor.getState()
    openDoc(doc('a.ks', { content: ['x', 'y'].join('\n') }))
    setCursor({ path: 'a.ks', line: 99, column: 1 })
    insertAtCursor('[p]')
    expect(useEditor.getState().docs[0].content).toBe(['x', 'y', '[p]'].join('\n'))
  })

  it('is a no-op when no document is active', () => {
    useEditor.getState().setCursor({ path: 'a.ks', line: 1, column: 1 })
    useEditor.getState().insertAtCursor('[p]')
    expect(useEditor.getState().docs).toHaveLength(0)
  })

  it('leaves other documents untouched', () => {
    const { openDoc, setCursor, insertAtCursor } = useEditor.getState()
    openDoc(doc('a.ks', { content: 'A' }))
    openDoc(doc('b.ks', { content: 'B' }))
    useEditor.getState().setActive('a.ks')
    setCursor({ path: 'a.ks', line: 1, column: 1 })
    insertAtCursor('[p]')
    const b = useEditor.getState().docs.find((x) => x.path === 'b.ks')
    expect(b?.content).toBe('B')
    expect(b?.dirty).toBe(false)
  })
})

describe('resolveInsertLine (pure)', () => {
  const cur = (line: number) => ({ path: 'a.ks', line, column: 1 })

  it('maps a cursor line to a 0-based insert index (just above)', () => {
    expect(resolveInsertLine("x\ny\nz", cur(2), 'a.ks')).toBe(1)
  })

  it('has no effect on content or the number of lines (pure)', () => {
    const content = "a\nb"
    resolveInsertLine(content, cur(1), 'a.ks')
    expect(content).toBe("a\nb")
  })

  it('returns the line count when the cursor path mismatches the active doc', () => {
    expect(resolveInsertLine('a\nb', cur(1), 'other.ks')).toBe(2)
  })

  it('returns the line count when no cursor is given', () => {
    expect(resolveInsertLine('a\nb', null, 'a.ks')).toBe(2)
  })

  it('returns 0 for an empty doc so the statement lands on line 1', () => {
    expect(resolveInsertLine('', cur(1), 'a.ks')).toBe(0)
  })
})

describe('useEditor project manager (Project Manager, Sprint 2)', () => {
  beforeEach(() => {
    useEditor.setState({
      client: null,
      projects: [],
      templates: [],
      recentProjects: [],
    })
  })

  it('setEngineClient registers the RPC client', () => {
    const mock = { ping: async () => ({ status: 'ok' }) }
    useEditor.getState().setEngineClient(mock as unknown as EngineClient)
    expect(useEditor.getState().client).toBe(mock)
  })

  it('loadProjects pulls the project list from the registered client', async () => {
    const mock = {
      projectList: async () => [
        { path: 'projects/demo', name: 'demo', template: 'basic', modified: '1' },
      ],
    }
    useEditor.setState({ client: mock as unknown as EngineClient })
    await useEditor.getState().loadProjects()
    expect(useEditor.getState().projects).toEqual([
      { path: 'projects/demo', name: 'demo', template: 'basic', modified: '1' },
    ])
  })

  it('loadTemplates pulls the template list from the registered client', async () => {
    const mock = {
      projectTemplates: async () => [
        { id: 'basic', name: 'basic', description: '', dir: 'basic' },
      ],
    }
    useEditor.setState({ client: mock as unknown as EngineClient })
    await useEditor.getState().loadTemplates()
    expect(useEditor.getState().templates).toEqual([
      { id: 'basic', name: 'basic', description: '', dir: 'basic' },
    ])
  })

  it('loadProjects is a no-op without a registered client', async () => {
    useEditor.setState({ client: null, projects: [] })
    await useEditor.getState().loadProjects()
    expect(useEditor.getState().projects).toEqual([])
  })

  it('loadProjects leaves state untouched when the client rejects', async () => {
    useEditor.setState({
      client: { projectList: async () => { throw new Error('offline') } } as unknown as EngineClient,
      projects: [{ path: 'projects/x', name: 'x', template: '' }],
    })
    await useEditor.getState().loadProjects()
    expect(useEditor.getState().projects).toEqual([
      { path: 'projects/x', name: 'x', template: '' },
    ])
  })

  it('addRecentProject prepends and de-duplicates by path', () => {
    const { addRecentProject } = useEditor.getState()
    addRecentProject('projects/a', 'a')
    addRecentProject('projects/b', 'b')
    addRecentProject('projects/a', 'a') // re-open a → moves to front
    const recent = useEditor.getState().recentProjects
    expect(recent.map((x) => x.path)).toEqual(['projects/a', 'projects/b'])
  })
})
