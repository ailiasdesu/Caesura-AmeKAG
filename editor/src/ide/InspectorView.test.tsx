// @vitest-environment jsdom
import { describe, it, expect, beforeEach } from 'vitest'
import { render, screen, cleanup } from '@testing-library/react'
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
