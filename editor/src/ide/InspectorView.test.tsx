// @vitest-environment jsdom
import { describe, it, expect, beforeEach, vi } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
// SceneTree pulls in EditorArea -> monaco-editor, not available in jsdom
// (collection crash guard, round 94).
vi.mock('./EditorArea', () => ({
  revealEditorLine: vi.fn(),
}))
// The live position poller is only used by SceneOutlinePanel in the round 82
// outline↔inspector integration test; stub it so the panel renders deterministically.
vi.mock('./useEnginePosition', () => ({
  useEnginePosition: () => {},
}))
import { parseTagParams } from './SceneTree'
import { InspectorView } from './InspectorView'
import { SceneOutlinePanel } from './SceneOutlinePanel'
import { useEditor } from '../store'

describe('parseTagParams (G4 inspector)', () => {
  it('extracts quoted string params', () => {
    expect(parseTagParams('name="Hero" text="Hello world"')).toEqual({
      name: 'Hero',
      text: 'Hello world',
    })
  })

  it('extracts bare numeric params as strings', () => {
    expect(parseTagParams('loop=1 speed=0.5 fade=-2')).toEqual({
      loop: '1',
      speed: '0.5',
      fade: '-2',
    })
  })

  it('treats bare flags as true', () => {
    expect(parseTagParams('no_fade autoturn=1')).toEqual({
      no_fade: 'true',
      autoturn: '1',
    })
  })

  it('allows = inside quoted values', () => {
    expect(parseTagParams('text="a=b=c"')).toEqual({ text: 'a=b=c' })
  })

  it('returns an empty table for empty or malformed bodies', () => {
    expect(parseTagParams('')).toEqual({})
    expect(parseTagParams('   ')).toEqual({})
    expect(parseTagParams('===]')).toEqual({})
  })

  it('is tolerant of unclosed quotes', () => {
    const out = parseTagParams('name="Hero text=hello')
    expect(out.name ?? '').toBe('Hero text=hello')
  })
})

const DOC = {
  path: 'assets/script/main.ks',
  name: 'main.ks',
  language: 'kag',
  content: [
    '*start',
    '[bg storage="room.png"]',
    '[ch name="Hero" text="Hello" speed=2]',
    '[playbgm file="bgm.ogg" loop=1]',
  ].join('\n'),
  dirty: false,
}

beforeEach(() => {
  cleanup()
  useEditor.setState({
    docs: [DOC],
    activePath: DOC.path,
    sideView: 'explorer',
    engineConnected: false,
    engineScene: '',
    engineToken: 0,
    enginePaused: false,
    revealRequest: null,
    inspected: null,
  })
})

describe('InspectorView (component)', () => {
  it('shows the empty hint before anything is inspected', () => {
    render(<InspectorView />)
    expect(screen.getByText('Click a scene element to inspect it')).toBeTruthy()
  })

  it('renders type, command and the full parameter table', () => {
    useEditor.setState({ inspected: { path: DOC.path, line: 3 } })
    render(<InspectorView />)
    expect(screen.getByText('ch')).toBeTruthy()
    expect(screen.getByText('[ch]')).toBeTruthy()
    expect(screen.getByText('Hero')).toBeTruthy()
    expect(screen.getByText('Hello')).toBeTruthy()
    expect(screen.getByText('2')).toBeTruthy()
  })

  it('shows the raw source line of the inspected element', () => {
    useEditor.setState({ inspected: { path: DOC.path, line: 2 } })
    render(<InspectorView />)
    expect(screen.getByText('[bg storage="room.png"]')).toBeTruthy()
  })

  it('shows the em-dash for an element without params', () => {
    useEditor.setState({ inspected: { path: DOC.path, line: 1 } })
    render(<InspectorView />)
    expect(screen.getByText('—')).toBeTruthy()
  })

  it('handles a missing document gracefully', () => {
    useEditor.setState({ inspected: { path: 'assets/script/gone.ks', line: 1 } })
    render(<InspectorView />)
    expect(screen.getByText('Element not found (document closed or line removed)')).toBeTruthy()
  })

  it('follows SceneTree clicks through the store (integration)', () => {
    // SceneTree sets inspected on click; InspectorView reads it.
    // This test drives the store directly to keep the suite fast; the
    // SceneTree component test already asserts the click → setInspected path.
    useEditor.setState({ inspected: { path: DOC.path, line: 4 } })
    const { rerender } = render(<InspectorView />)
    expect(screen.getByText('bgm.ogg')).toBeTruthy()
    expect(screen.getByText('1')).toBeTruthy()
    // switch inspection
    useEditor.setState({ inspected: { path: DOC.path, line: 2 } })
    rerender(<InspectorView />)
    expect(screen.getByText('room.png')).toBeTruthy()
  })
})


// ---------------------------------------------------------------------------
// Round 82 — engine status strip, command lint, and bidirectional label jumps.
// ---------------------------------------------------------------------------

/** Minimal fake EngineClient exercising the evalRaw surface InspectorView uses. */
function makeFakeClient(overrides: { onEval?: (code: string) => string } = {}) {
  const evalRaw = vi.fn((code: string) => Promise.resolve(overrides.onEval ? overrides.onEval(code) : 'ok'))
  return { evalRaw }
}

describe('InspectorView engine status strip (round 82)', () => {
  beforeEach(() => {
    useEditor.setState({
      engineConnected: true,
      engineScene: DOC.path,
      engineToken: 3,
      enginePaused: false,
    })
  })

  it('renders the live scene basename, token index and running badge when scenes match', () => {
    useEditor.setState({ inspected: { path: DOC.path, line: 3 } })
    render(<InspectorView />)
    expect(screen.getByText('main.ks')).toBeTruthy()
    expect(screen.getByText('#3')).toBeTruthy()
    expect(screen.getByText('running')).toBeTruthy()
  })

  it('shows a paused badge when the engine is paused', () => {
    useEditor.setState({ enginePaused: true, inspected: { path: DOC.path, line: 3 } })
    render(<InspectorView />)
    expect(screen.getByText('paused')).toBeTruthy()
  })

  it('omits the status strip when the engine is disconnected even if scene matches', () => {
    useEditor.setState({ engineConnected: false, inspected: { path: DOC.path, line: 3 } })
    render(<InspectorView />)
    expect(screen.queryByText('main.ks')).toBeNull()
  })

  it('omits the status strip when the engine scene does not match the inspected doc', () => {
    useEditor.setState({ engineScene: 'assets/script/other.ks', inspected: { path: DOC.path, line: 3 } })
    render(<InspectorView />)
    expect(screen.queryByText('main.ks')).toBeNull()
  })
})

describe('InspectorView command lint (round 82)', () => {
  beforeEach(() => {
    useEditor.setState({
      engineConnected: false,
      engineScene: '',
      engineToken: 0,
      enginePaused: false,
    })
  })

  it('marks an unknown command name with a warning', () => {
    // [notacommand foo=1] is not in KNOWN_COMMANDS
    const doc = { ...DOC, content: '[notacommand foo="bar"]' }
    useEditor.setState({ docs: [doc], inspected: { path: doc.path, line: 1 } })
    render(<InspectorView />)
    expect(screen.getByText(/Unknown command 'notacommand'/)).toBeTruthy()
  })

  it('does not warn for a recognized command', () => {
    useEditor.setState({ inspected: { path: DOC.path, line: 2 } }) // [bg ...]
    render(<InspectorView />)
    expect(screen.queryByText(/Unknown command/)).toBeNull()
  })

  it('flags an unlisted param (soft hint) for a curated command', () => {
    // [bg storage="room.png" wobble=2] — wobble is not a documented bg param
    const doc = { ...DOC, content: '[bg storage="room.png" wobble=2]' }
    useEditor.setState({ docs: [doc], inspected: { path: doc.path, line: 1 } })
    render(<InspectorView />)
    expect(screen.getByText(/1 unlisted param for \[bg\]/)).toBeTruthy()
  })

  it('shows no unlisted hint when every param is documented', () => {
    useEditor.setState({ inspected: { path: DOC.path, line: 2 } }) // [bg storage="room.png"]
    render(<InspectorView />)
    expect(screen.queryByText(/unlisted param/)).toBeNull()
  })
})

describe('InspectorView label navigation (round 82)', () => {
  beforeEach(() => {
    useEditor.setState({
      engineConnected: false,
      engineScene: '',
      engineToken: 0,
      enginePaused: false,
    })
  })

  it('reveals the label-jump and follow-engine buttons on a *label row', () => {
    useEditor.setState({ inspected: { path: DOC.path, line: 1 } }) // *start
    render(<InspectorView />)
    expect(screen.getByRole('button', { name: /Jump engine → label/ })).toBeTruthy()
    expect(screen.getByRole('button', { name: 'Follow engine position' })).toBeTruthy()
  })

  it('does not show the jump buttons on a command row', () => {
    useEditor.setState({ inspected: { path: DOC.path, line: 2 } })
    render(<InspectorView />)
    expect(screen.queryByRole('button', { name: /Jump engine/ })).toBeNull()
  })

  it('disables the live-jump button when the engine is disconnected', () => {
    useEditor.setState({ inspected: { path: DOC.path, line: 1 } })
    const { container } = render(<InspectorView client={makeFakeClient() as any} />)
    const btn = container.querySelector('button.primary') as HTMLButtonElement
    expect(btn.disabled).toBe(true)
  })

  it('fires buildLabelJumpSnippet via the client when connected', async () => {
    const client = makeFakeClient()
    useEditor.setState({
      engineConnected: true,
      engineScene: DOC.path,
      engineToken: 3,
      enginePaused: false,
      inspected: { path: DOC.path, line: 1 },
    })
    const { getByRole } = render(<InspectorView client={client as any} />)
    getByRole('button', { name: /Jump engine → label/ }).click()
    await vi.waitFor(() => {
      expect(client.evalRaw).toHaveBeenCalledTimes(1)
    })
    const code = (client.evalRaw as any).mock.calls[0][0] as string
    expect(code).toContain('kag.jump(c, "*start")')
  })

  it('follows the engine position back into the editor (token → source line)', () => {
    // token 3 resolves to outline row line 3 ([ch ...]).
    useEditor.setState({
      engineConnected: true,
      engineScene: DOC.path,
      engineToken: 3,
      enginePaused: false,
      inspected: { path: DOC.path, line: 1 },
    })
    const { getByRole } = render(<InspectorView client={makeFakeClient() as any} />)
    getByRole('button', { name: 'Follow engine position' }).click()
    const state = useEditor.getState()
    expect(state.inspected).toEqual({ path: DOC.path, line: 3 })
    expect(state.revealRequest?.path).toBe(DOC.path)
    expect(state.revealRequest?.line).toBe(3)
  })
})


// ---------------------------------------------------------------------------
// Round 82 continuation — lint boundaries, engine disconnect timing, jump
// error handling + idempotency, outline↔inspector sync, and store boundaries.
// ---------------------------------------------------------------------------

describe('InspectorView lint interaction boundaries (round 82 cont.)', () => {
  beforeEach(() => {
    useEditor.setState({
      engineConnected: false,
      engineScene: '',
      engineToken: 0,
      enginePaused: false,
      docs: [DOC],
      inspected: null,
    })
  })

  it('shows the unknown-command warning AND keeps flag styling on a bare-flag param at once', () => {
    // [notacommand foo] — unknown command name + a bare flag param.
    const doc = { ...DOC, content: '[notacommand foo]' }
    useEditor.setState({ docs: [doc], inspected: { path: doc.path, line: 1 } })
    const { container } = render(<InspectorView />)
    expect(screen.getByText(/Unknown command 'notacommand'/)).toBeTruthy()
    // the bare flag is still linted even for an unknown command
    const flagEl = container.querySelector('b.inspector-param-flag') as Element
    expect(flagEl).toBeTruthy()
    expect(flagEl.textContent).toBe('true')
  })

  it('surfaces an unlisted param tooltip via the title attribute', () => {
    // [bg storage="room.png" wobble=2] — wobble is not a documented bg param.
    const doc = { ...DOC, content: '[bg storage="room.png" wobble=2]' }
    useEditor.setState({ docs: [doc], inspected: { path: doc.path, line: 1 } })
    render(<InspectorView />)
    expect(
      screen.getByTitle('wobble not listed for [bg] — possible typo'),
    ).toBeTruthy()
  })

  it('distinguishes a bare flag from an explicit boolean-ish value', () => {
    // Same command + param, two forms: bare flag vs an explicit value.
    const doc = { ...DOC, content: '[playbgm file="bgm.ogg" loop]' }
    useEditor.setState({ docs: [doc], inspected: { path: doc.path, line: 1 } })
    const { unmount } = render(<InspectorView />)
    expect(
      screen.getByTitle('loop is a bare flag (no value)'),
    ).toBeTruthy()

    unmount()
    // [playbgm file="bgm.ogg" loop=1] — loop=1 is a real value, NOT a flag.
    const doc2 = { ...DOC, content: '[playbgm file="bgm.ogg" loop=1]' }
    useEditor.setState({ docs: [doc2], inspected: { path: doc2.path, line: 1 } })
    render(<InspectorView />)
    expect(screen.queryByTitle('loop is a bare flag (no value)')).toBeNull()
    // and no unlisted hint either — loop is a documented playbgm param
    expect(screen.queryByText(/unlisted param/)).toBeNull()
  })

  it('shows no lint warning or hint on a non-command (label) row', () => {
    useEditor.setState({ inspected: { path: DOC.path, line: 1 } }) // *start
    const { container } = render(<InspectorView />)
    expect(screen.queryByText(/Unknown command/)).toBeNull()
    expect(screen.queryByText(/unlisted param/)).toBeNull()
    expect(container.querySelector('.inspector-param-flag')).toBeNull()
    expect(container.querySelector('.inspector-param-unlisted')).toBeNull()
  })
})

describe('InspectorView engine disconnect timing (round 82 cont.)', () => {
  beforeEach(() => {
    useEditor.setState({
      docs: [DOC],
      inspected: { path: DOC.path, line: 1 }, // *start label
      engineConnected: false,
      engineScene: '',
      engineToken: 0,
      enginePaused: false,
    })
  })

  it('degrades from connected to disconnected in one session (strip + jump disable)', () => {
    // Connected: status strip present, jump enabled.
    useEditor.setState({
      engineConnected: true,
      engineScene: DOC.path,
      engineToken: 3,
    })
    const client = makeFakeClient() as any
    const { rerender } = render(<InspectorView client={client} />)
    expect(screen.getByText('main.ks')).toBeTruthy()
    const jumpBefore = document.querySelector('button.primary') as HTMLButtonElement
    expect(jumpBefore.disabled).toBe(false)

    // Engine drops: rerender the same session; strip gone + jump disabled.
    useEditor.setState({ engineConnected: false, engineScene: '', engineToken: 0 })
    rerender(<InspectorView client={client} />)
    expect(screen.queryByText('main.ks')).toBeNull()
    const jumpAfter = document.querySelector('button.primary') as HTMLButtonElement
    expect(jumpAfter.disabled).toBe(true)
  })

  it('keeps the jump button enabled while the engine is paused (still connected)', () => {
    useEditor.setState({
      engineConnected: true,
      engineScene: DOC.path,
      engineToken: 3,
      enginePaused: true,
    })
    render(<InspectorView client={makeFakeClient() as any} />)
    expect(screen.getByText('paused')).toBeTruthy()
    const jump = document.querySelector('button.primary') as HTMLButtonElement
    expect(jump.disabled).toBe(false)
  })
})

describe('InspectorView jump error handling + idempotency (round 82 cont.)', () => {
  beforeEach(() => {
    useEditor.setState({
      docs: [DOC],
      inspected: { path: DOC.path, line: 1 }, // *start
      engineConnected: true,
      engineScene: DOC.path,
      engineToken: 3,
      enginePaused: false,
    })
  })

  it('surfaces a "missing" label reply under the jump buttons', async () => {
    const client = makeFakeClient({ onEval: () => 'missing' })
    render(<InspectorView client={client as any} />)
    screen.getByRole('button', { name: /Jump engine → label/ }).click()
    await screen.findByText('Label *start not found in the running scene')
  })

  it('surfaces a "no-ctx" reply when no scene is running', async () => {
    const client = makeFakeClient({ onEval: () => 'no-ctx' })
    render(<InspectorView client={client as any} />)
    screen.getByRole('button', { name: /Jump engine → label/ }).click()
    await screen.findByText('No scene running in the engine')
  })

  it('surfaces an "ok" reply as a success message', async () => {
    const client = makeFakeClient({ onEval: () => 'ok' })
    render(<InspectorView client={client as any} />)
    screen.getByRole('button', { name: /Jump engine → label/ }).click()
    await screen.findByText('Jumped to *start')
  })

  it('surfaces a transport rejection as an error message', async () => {
    const evalRaw = vi.fn(() => Promise.reject(new Error('net down')))
    const client = { evalRaw }
    render(<InspectorView client={client as any} />)
    screen.getByRole('button', { name: /Jump engine → label/ }).click()
    await screen.findByText('Engine jump failed — is the engine reachable?')
  })

  it('ignores duplicate clicks while a jump is in flight (idempotent)', async () => {
    let resolveEval: (v: string) => void = () => {}
    const pending = new Promise<string>((res) => {
      resolveEval = res
    })
    const evalRaw = vi.fn(() => pending)
    const client = { evalRaw }
    render(<InspectorView client={client as any} />)
    const btn = screen.getByRole('button', { name: /Jump engine → label/ })

    btn.click()
    btn.click()
    btn.click()
    // only the first click issues an eval; the rest are dropped while pending
    expect(evalRaw).toHaveBeenCalledTimes(1)

    resolveEval('ok')
    await vi.waitFor(() => {
      expect(screen.getByText('Jumped to *start')).toBeTruthy()
    })
    // after completion a new click is allowed again
    btn.click()
    await vi.waitFor(() => expect(evalRaw).toHaveBeenCalledTimes(2))
  })

  it('does not show any jump status line before a jump is attempted', () => {
    render(<InspectorView client={makeFakeClient() as any} />)
    expect(document.querySelector('.inspector-jump-status')).toBeNull()
  })
})

describe('InspectorView ↔ SceneOutline bidirectional sync (round 82 cont.)', () => {
  beforeEach(() => {
    useEditor.setState({
      docs: [DOC],
      activePath: DOC.path,
      engineConnected: false,
      engineScene: '',
      engineToken: 0,
      enginePaused: false,
      inspected: null,
    })
  })

  it('outline label click → inspector shows that label; next outline click → inspector switches', () => {
    // A doc with two *label rows so the bidirectional switch is observable.
    const twoLabelDoc = {
      ...DOC,
      content: ['*start', '[bg storage="room.png"]', '*next', '[ch name="Hero" text="Hi"]'].join('\n'),
    }
    useEditor.setState({ docs: [twoLabelDoc], activePath: twoLabelDoc.path })
    const { container } = render(
      <>
        <SceneOutlinePanel client={undefined} />
        <InspectorView />
      </>,
    )

    // Initially nothing inspected.
    expect(screen.getByText('Click a scene element to inspect it')).toBeTruthy()

    // Click *start in the outline -> inspector follows the store.
    fireEvent.click(screen.getByTitle('label *start'))
    let cmdValue = container.querySelector('.inspector-body .state-row:nth-child(2) b')
    expect(cmdValue?.textContent).toBe('*start')
    expect(useEditor.getState().inspected).toEqual({ path: twoLabelDoc.path, line: 1 })

    // Click *next (line 3 in this doc) -> inspector switches to the new label.
    fireEvent.click(screen.getByTitle('label *next'))
    cmdValue = container.querySelector('.inspector-body .state-row:nth-child(2) b')
    expect(cmdValue?.textContent).toBe('*next')
    expect(useEditor.getState().inspected).toEqual({ path: twoLabelDoc.path, line: 3 })

    // Click back to *start -> inspector switches again (true bidirectional).
    fireEvent.click(screen.getByTitle('label *start'))
    cmdValue = container.querySelector('.inspector-body .state-row:nth-child(2) b')
    expect(cmdValue?.textContent).toBe('*start')
  })
})

describe('InspectorView store boundaries (round 82 cont.)', () => {
  beforeEach(() => {
    useEditor.setState({
      docs: [DOC],
      inspected: null,
      engineConnected: false,
      engineScene: '',
      engineToken: 0,
      enginePaused: false,
    })
  })

  it('shows the placeholder while inspected is null (store default)', () => {
    render(<InspectorView />)
    expect(screen.getByText('Click a scene element to inspect it')).toBeTruthy()
  })

  it('shows the not-found hint when inspected points at a line with no element', () => {
    // a known doc, but line 500 has no scene element
    useEditor.setState({ inspected: { path: DOC.path, line: 500 } })
    render(<InspectorView />)
    expect(
      screen.getByText('Element not found (document closed or line removed)'),
    ).toBeTruthy()
  })

  it('switches cleanly across rapid consecutive inspected changes (no stale content)', () => {
    const { rerender } = render(<InspectorView />)
    // rapid-fire store updates without waiting between renders
    useEditor.setState({ inspected: { path: DOC.path, line: 4 } })
    useEditor.setState({ inspected: { path: DOC.path, line: 2 } })
    useEditor.setState({ inspected: { path: DOC.path, line: 3 } })
    rerender(<InspectorView />)
    // the final selection wins
    const state = useEditor.getState()
    expect(state.inspected).toEqual({ path: DOC.path, line: 3 })
    expect(screen.getByText('Hero')).toBeTruthy() // [ch name="Hero" ...] line 3
    expect(screen.queryByText('bgm.ogg')).toBeNull() // stale line-4 content gone
  })
})
