// @vitest-environment jsdom
import { describe, it, expect, beforeEach, vi } from 'vitest'
import { render, screen, cleanup } from '@testing-library/react'
// SceneTree pulls in EditorArea -> monaco-editor, not available in jsdom
// (collection crash guard, round 94).
vi.mock('./EditorArea', () => ({
  revealEditorLine: vi.fn(),
}))
import { parseTagParams } from './SceneTree'
import { InspectorView } from './InspectorView'
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
