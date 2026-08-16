// Panels integration test — the real workbench wiring, not per-file isolation.
//
// Mounts the real <App/> (all panels + store) with only the transport mocked:
//  - EngineClient (the HTTP/fetch surface) is replaced by a mock class whose
//    methods resolve to harmless defaults — the STORE stays real.
//  - Monaco (@monaco-editor/react + monaco-editor) is stubbed because it is
//    not usable in jsdom.
// Everything else — SceneTree / SceneOutlinePanel / InspectorView /
// TimelineView / StatusBar / VisualView / DebugView / ActivityBar / EditorArea
// and the zustand editor store — is the real component graph.
//
// Covered end-to-end collaboration chains:
//   1. Open doc -> SceneOutline renders -> click *label -> revealRequest
//      consumed by EditorArea + Inspector shows the element.
//   2. setEngine (engine position) -> SceneOutline highlights + TimelineView
//      highlights + StatusBar reflects state + VisualView shows the live badge.
//   3. ActivityBar view switch -> the matching panel cluster mounts and the
//      others unmount (no stale cross-view state).
//
// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { render, screen, fireEvent, cleanup, act, within } from '@testing-library/react'
import { App } from '../App'
import { useEditor, type OpenDoc } from '../store'

// ---------------------------------------------------------------------------
// Monaco stub. EditorArea registers a fake editor (for reveal assertions) and
// KagLsp.register() touches a handful of provider-registration APIs — provide
// enough of the namespace so no call throws (provider callbacks never fire in
// jsdom, so their internals are never exercised).
// ---------------------------------------------------------------------------
const fakeEditorH = vi.hoisted(() => ({
  revealLineInCenter: vi.fn(),
  setPosition: vi.fn(),
  focus: vi.fn(),
  onDidDispose: vi.fn(() => () => {}),
  onDidChangeCursorPosition: vi.fn(() => ({ dispose: vi.fn() })),
  addCommand: vi.fn(),
}))

const fakeEditor = fakeEditorH

vi.mock('@monaco-editor/react', async () => {
  const mono = await import('monaco-editor')
  return {
    default: (props: {
      path?: string
      onMount?: (ed: unknown, mo: unknown) => Promise<void> | void
    }) => {
      // Fire the mount callback during render so the real EditorArea registers
      // its editor + LSP with the (mocked) Monaco namespace, exactly as it
      // would with a live Monaco instance.
      if (props.onMount) void props.onMount(fakeEditorH, mono)
      return null
    },
  }
})

vi.mock('monaco-editor', () => ({
  editor: {
    setModelMarkers: () => {},
    onDidCreateModel: () => ({ dispose: () => {} }),
  },
  languages: {
    getLanguages: () => [],
    register: () => {},
    setMonarchTokensProvider: () => {},
    setLanguageConfiguration: () => {},
    registerCompletionItemProvider: () => ({ dispose: () => {} }),
    registerHoverProvider: () => ({ dispose: () => {} }),
    registerDefinitionProvider: () => ({ dispose: () => {} }),
    registerReferenceProvider: () => ({ dispose: () => {} }),
  },
  Range: class {
    constructor(
      public a: number, public b: number, public c: number, public d: number,
    ) {}
  },
  MarkerSeverity: { Error: 8, Warning: 4 },
  KeyMod: { CtrlCmd: 2048 },
  KeyCode: { KeyS: 49 },
}))

// ---------------------------------------------------------------------------
// EngineClient mock — the only transport mocked. Every mounted panel uses the
// SAME instance App creates (clientRef). Defined inside vi.hoisted so the
// vi.mock factory (which is hoisted above the class declaration) can reference
// it without a TDZ error.
// ---------------------------------------------------------------------------
const h = vi.hoisted(() => {
  const mockClients: unknown[] = []

  class MockEngineClient {
    static instances = mockClients
    base = '/api'
    token = ''
    ping = vi.fn(async () => ({ status: 'ok', engine: 'caesura-test' }))
    status = vi.fn(async () => ({ status: 'ok', engine: 'caesura-test', lua: true, port: 9876 }))
    assets = vi.fn(async () => [] as { path: string; name: string; type: string }[])
    logs = vi.fn(async () => [] as { level: string; message: string; time: string }[])
    inspect = vi.fn(async () => '')
    evalRaw = vi.fn(async () => '')
    state = vi.fn(async () => ({ status: 'ok' }))
    stats = vi.fn(async () => ({ status: 'ok' }))
    frame = vi.fn(async () => ({ status: 'error', error: 'no frame in jsdom' }))
    pick = vi.fn(async () => ({ status: 'ok', hits: '[]' }))
    smaValidate = vi.fn(async () => ({ status: 'ok', ok: true, errors: [], meta: '{}' }))
    smaSave = vi.fn(async () => ({ status: 'ok', ok: true, errors: [] }))
    live2dModels = vi.fn(async () => [] as { path: string; name: string }[])
    live2dLoad = vi.fn(async () => ({ status: 'ok', modelId: 0 }))
    debugState = vi.fn(async () => { throw new Error('no debugger in jsdom') })
    run = vi.fn(async () => ({ status: 'ok' }))
    stop = vi.fn(async () => ({ status: 'ok' }))
    reload = vi.fn(async () => ({ status: 'ok' }))
    build = vi.fn(async () => ({ status: 'ok' }))
    setBreakpoint = vi.fn(async () => ({ status: 'ok' }))
    removeBreakpoint = vi.fn(async () => ({ status: 'ok' }))
    clearBreakpoints = vi.fn(async () => ({ status: 'ok' }))
    debugContinue = vi.fn(async () => ({ status: 'ok' }))

    constructor() {
      mockClients.push(this)
    }
    setToken(t: string) { this.token = t }
    setBase(b: string) { this.base = b }
  }

  return { MockEngineClient, mockClients }
})

const { mockClients } = h

vi.mock('../lib/rpc', () => ({
  EngineClient: h.MockEngineClient,
  RpcError: class RpcError extends Error {},
}))

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

const KS_SOURCE = [
  '*start',
  '[bg storage="room.png"]',
  'It was a quiet morning.',
  '[ch name="Hero" text="Hello there" speed=2]',
  '*next',
  '[playbgm file="bgm.ogg" loop=1]',
  '*end',
  '[ch text="Bye"]',
].join('\n')

function doc(path: string, content: string): OpenDoc {
  return { path, name: path.split('/').pop() ?? path, language: 'kag', content, dirty: false }
}

function resetStore(over: Partial<ReturnType<typeof useEditor.getState>> = {}) {
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

/** Opening a .ks doc as the active one seeds every explorer panel. */
function openActiveKs() {
  resetStore({
    docs: [doc('assets/script/main.ks', KS_SOURCE)],
    activePath: 'assets/script/main.ks',
  })
}

beforeEach(() => {
  cleanup()
  mockClients.length = 0
  fakeEditor.revealLineInCenter.mockClear()
  fakeEditor.setPosition.mockClear()
  fakeEditor.focus.mockClear()
  resetStore()
})

afterEach(() => {
  cleanup()
  resetStore()
})

describe('integration · document → outline → reveal → inspector', () => {
  it('renders the .ks outline, inspects on label click, and reveals the line', () => {
    openActiveKs()
    render(<App />)

    // SceneOutline renders *label headings and command rows from the store
    // doc. The labels also appear in SceneTree, so scope to the outline pane.
    const outlinePane = screen.getByText('Scene Outline').closest('.sidebar-pane') as HTMLElement
    const outline = within(outlinePane)
    expect(outline.getAllByText('*start').length).toBeGreaterThan(0)
    expect(outline.getAllByText('*next').length).toBeGreaterThan(0)
    expect(outline.getAllByText('*end').length).toBeGreaterThan(0)
    expect(outline.getAllByText('[bg]').length).toBeGreaterThan(0)

    // Click a label heading in the real SceneOutline panel.
    fireEvent.click(outline.getByText('*next'))

    // SceneOutline's handleSelectLabel unifies selection + reveal in the store.
    const state = useEditor.getState()
    expect(state.inspected).toEqual({ path: 'assets/script/main.ks', line: 5 })
    expect(state.revealRequest).toEqual({
      path: 'assets/script/main.ks',
      line: 5,
      nonce: expect.any(Number) as unknown as number,
    })

    // EditorArea consumed the reveal and routed it to the registered (fake)
    // Monaco editor at the requested source line.
    expect(fakeEditor.revealLineInCenter).toHaveBeenCalledWith(5)
    expect(fakeEditor.setPosition).toHaveBeenCalledWith({ lineNumber: 5, column: 1 })
    expect(fakeEditor.focus).toHaveBeenCalled()
  })

  it('inspecting a command row in the real chain shows its params', () => {
    openActiveKs()
    render(<App />)

    // Click the SceneTree [ch] row (line 4) to select it. [ch] appears in
    // several panels, so scope the click to the Scene Tree pane.
    const sceneTreePane = screen.getByText('Scene Tree').closest('.sidebar-pane') as HTMLElement
    const chBtn = within(sceneTreePane)
      .getAllByText('[ch]')
      .map((el) => el.closest('button'))
      .find((b) => b !== null)
    fireEvent.click(chBtn as HTMLButtonElement)

    const state = useEditor.getState()
    expect(state.inspected).toEqual({ path: 'assets/script/main.ks', line: 4 })

    // InspectorView renders the command + full param table (scoped to the
    // Inspector pane because '[ch]' also appears in SceneTree/Timeline).
    const inspectorPane = screen.getByText('Inspector').closest('.sidebar-pane') as HTMLElement
    const insp = within(inspectorPane)
    expect(insp.getByText('ch')).toBeTruthy()
    expect(insp.getAllByText('[ch]').length).toBeGreaterThan(0)
    expect(insp.getByText('Hero')).toBeTruthy()
    expect(insp.getByText('Hello there')).toBeTruthy()
    expect(insp.getByText('2')).toBeTruthy()
  })
})

describe('integration · engine connection propagates across panels', () => {
  it('setEngine highlights Outline + Timeline and updates StatusBar + VisualView', () => {
    openActiveKs()
    const { container } = render(<App />)

    // Connect the engine and point it at the running scene + a token that
    // resolves to the [bg] line (3) in both the outline and the timeline.
    act(() => {
      useEditor.getState().setEngine({
        engineConnected: true,
        engineScene: 'assets/script/main.ks',
        engineToken: 2, // -> line 2 ([bg storage="room.png"])
        enginePaused: false,
        engineCmd: '[bg]',
      })
    })

    // StatusBar reflects the live engine state.
    expect(screen.getByText('Engine: connected')).toBeTruthy()
    expect(screen.getByText('scene: assets/script/main.ks')).toBeTruthy()
    expect(screen.getByText('token: 2')).toBeTruthy()
    expect(screen.getByText('cmd: [bg]')).toBeTruthy()
    expect(screen.getByText('▶ running')).toBeTruthy()

    // SceneOutline highlights the command row at the live token line (line 2).
    const outlineBody = container.querySelector('.outline-body') as HTMLElement
    const bgRow = Array.from(outlineBody.querySelectorAll('.outline-row')).find(
      (el) => el.textContent?.includes('[bg]'),
    )
    expect(bgRow?.className).toContain('outline-current')
    // Other rows are not highlighted.
    const textRow = Array.from(outlineBody.querySelectorAll('.outline-row')).find(
      (el) => el.textContent?.includes('quiet morning'),
    )
    expect(textRow?.className).not.toContain('outline-current')

    // Timeline shows the exec bar and highlights the matching row.
    expect(screen.getByText('token 2')).toBeTruthy()
    const timelineExec = container.querySelector('.timeline-exec')
    expect(timelineExec?.textContent).toContain('[bg]')
    // The Timeline row at the live token line is highlighted.
    const timelinePane = screen.getByText('Timeline').closest('.sidebar-pane') as HTMLElement
    const timelineBg = within(timelinePane)
      .getAllByText('[bg]')
      .map((el) => el.closest('button'))
      .find((b) => b !== null)
    expect(timelineBg?.className).toContain('outline-current')

    // VisualView: switch to it (engineConnected true) -> live badge shows.
    fireEvent.click(screen.getByTitle('Visual Preview'))
    expect(screen.getAllByText('live').length).toBeGreaterThan(0)
  })

  it('highlights the outline section heading when the token is on a label', () => {
    openActiveKs()
    const { container } = render(<App />)
    act(() => {
      useEditor.getState().setEngine({
        engineConnected: true,
        engineScene: 'assets/script/main.ks',
        engineToken: 1, // -> *start heading (line 1)
        enginePaused: false,
        engineCmd: '*start',
      })
    })
    const outlineBody = container.querySelector('.outline-body') as HTMLElement
    const startHeading = Array.from(
      outlineBody.querySelectorAll('.outline-label-row'),
    ).find((el) => el.textContent?.includes('*start'))
    expect(startHeading?.className).toContain('outline-current')
    // Timeline exec bar too.
    expect(screen.getByText('token 1')).toBeTruthy()
  })
})

describe('integration · ActivityBar mounts/unmounts panel clusters', () => {
  it('switching views mounts only the selected panel cluster', () => {
    openActiveKs()
    render(<App />)

    // Explorer default: the script panels are all mounted.
    expect(screen.getByText('Scene Tree')).toBeTruthy()
    expect(screen.getByText('Scene Outline')).toBeTruthy()
    expect(screen.getByText('Timeline')).toBeTruthy()
    expect(screen.getByText('Inspector')).toBeTruthy()

    // Debug: only the debug panel is mounted; explorer panels unmount.
    fireEvent.click(screen.getByTitle('Run and Debug'))
    expect(screen.getByText('Run and Debug')).toBeTruthy()
    expect(screen.queryByText('Scene Tree')).toBeNull()
    expect(screen.queryByText('Scene Outline')).toBeNull()
    expect(screen.queryByText('Timeline')).toBeNull()
    expect(screen.queryByText('Inspector')).toBeNull()

    // Visual: only the visual preview panel.
    fireEvent.click(screen.getByTitle('Visual Preview'))
    expect(screen.getByText('Visual Preview')).toBeTruthy()
    expect(screen.queryByText('Run and Debug')).toBeNull()
    expect(screen.queryByText('Scene Tree')).toBeNull()

    // AI: only the AI writer panel.
    fireEvent.click(screen.getByTitle('AI Writer'))
    expect(screen.getByText('AI Writer')).toBeTruthy()
    expect(screen.queryByText('Visual Preview')).toBeNull()
    expect(screen.queryByText('Scene Tree')).toBeNull()

    // Back to Explorer: the script panels remount with the same store doc.
    fireEvent.click(screen.getByTitle('Explorer (assets)'))
    expect(screen.getByText('Scene Tree')).toBeTruthy()
    expect(screen.getByText('Scene Outline')).toBeTruthy()
    expect(screen.getByText('Timeline')).toBeTruthy()
  })

  it('re-establishes the outline highlight after switching away and back to Explorer', () => {
    openActiveKs()
    const { container } = render(<App />)
    act(() => {
      useEditor.getState().setEngine({
        engineConnected: true,
        engineScene: 'assets/script/main.ks',
        engineToken: 1, // -> *start heading (line 1)
        enginePaused: false,
        engineCmd: '*start',
      })
    })
    fireEvent.click(screen.getByTitle('Run and Debug'))
    fireEvent.click(screen.getByTitle('Explorer (assets)'))
    const outlineBody = container.querySelector('.outline-body') as HTMLElement
    const current = outlineBody.querySelector('.outline-label-row.outline-current')
    expect(current?.textContent).toContain('*start')
  })
})
