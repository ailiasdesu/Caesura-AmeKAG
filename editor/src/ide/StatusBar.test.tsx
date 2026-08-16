// @vitest-environment jsdom
import { describe, it, expect, beforeEach } from 'vitest'
import { render, screen, cleanup } from '@testing-library/react'
import { StatusBar } from './StatusBar'
import { useEditor } from '../store'

beforeEach(() => {
  cleanup()
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
  })
})

describe('StatusBar (component)', () => {
  it('shows offline defaults: disconnected, no scene, token 0, running', () => {
    render(<StatusBar />)
    expect(screen.getByText('Engine: offline')).toBeTruthy()
    expect(screen.getByText('scene: —')).toBeTruthy()
    expect(screen.getByText('token: 0')).toBeTruthy()
    expect(screen.getByText('▶ running')).toBeTruthy()
  })

  it('flips to connected when the engine is up', () => {
    useEditor.setState({ engineConnected: true })
    render(<StatusBar />)
    expect(screen.getByText('Engine: connected')).toBeTruthy()
    expect(screen.queryByText('Engine: offline')).toBeNull()
  })

  it('shows the current scene name when present', () => {
    useEditor.setState({ engineScene: 'chapter2/forest' })
    render(<StatusBar />)
    expect(screen.getByText('scene: chapter2/forest')).toBeTruthy()
  })

  it('shows the token index', () => {
    useEditor.setState({ engineToken: 42 })
    render(<StatusBar />)
    expect(screen.getByText('token: 42')).toBeTruthy()
  })

  it('shows the current execution command (em-dash when idle)', () => {
    render(<StatusBar />)
    expect(screen.getByText('cmd: —')).toBeTruthy()
    cleanup()
    useEditor.setState({ engineCmd: '[ch]' })
    render(<StatusBar />)
    expect(screen.getByText('cmd: [ch]')).toBeTruthy()
  })

  it('shows paused state', () => {
    useEditor.setState({ enginePaused: true })
    render(<StatusBar />)
    expect(screen.getByText('⏸ paused')).toBeTruthy()
    expect(screen.queryByText('▶ running')).toBeNull()
  })

  it('renders the editor brand label', () => {
    render(<StatusBar />)
    expect(screen.getByText('Caesura Editor')).toBeTruthy()
  })

  it('combines connected + scene + token + paused in one bar', () => {
    useEditor.setState({
      engineConnected: true,
      engineScene: 'prologue',
      engineToken: 7,
      enginePaused: true,
    })
    render(<StatusBar />)
    expect(screen.getByText('Engine: connected')).toBeTruthy()
    expect(screen.getByText('scene: prologue')).toBeTruthy()
    expect(screen.getByText('token: 7')).toBeTruthy()
    expect(screen.getByText('⏸ paused')).toBeTruthy()
  })
})

describe('StatusBar (state machine & error boundaries)', () => {
  it('applies the connected/disconnected status-engine class', () => {
    render(<StatusBar />)
    const bad = screen.getByText('Engine: offline')
    expect(bad.className).toContain('status-engine')
    expect(bad.className).toContain('bad')
    expect(bad.className).not.toContain('ok')
    cleanup()
    useEditor.setState({ engineConnected: true })
    render(<StatusBar />)
    const ok = screen.getByText('Engine: connected')
    expect(ok.className).toContain('ok')
    expect(ok.className).not.toContain('bad')
  })

  it('re-renders when a store update bumps the token (token stays in sync)', () => {
    useEditor.setState({ engineConnected: true, engineToken: 0 })
    const { rerender } = render(<StatusBar />)
    expect(screen.getByText('token: 0')).toBeTruthy()
    // Keep component mounted across a store mutation (live engine updates).
    useEditor.setState({ engineToken: 5 })
    rerender(<StatusBar />)
    expect(screen.getByText('token: 5')).toBeTruthy()
    expect(screen.queryByText('token: 0')).toBeNull()
  })

  it('shows scene placeholder when engine has no scene yet', () => {
    useEditor.setState({ engineConnected: true, engineScene: '' })
    render(<StatusBar />)
    expect(screen.getByText('scene: —')).toBeTruthy()
  })

  it('falls back to em-dash when the engine command is empty', () => {
    useEditor.setState({ engineConnected: true, engineCmd: '' })
    render(<StatusBar />)
    expect(screen.getByText('cmd: —')).toBeTruthy()
  })

  it('truncates an over-long scene name with an ellipsis', () => {
    const long = 'chapter7/final_battle/very_long_scene_name_over_forty_chars_yes'
    useEditor.setState({ engineConnected: true, engineScene: long })
    render(<StatusBar />)
    const item = screen.getByText((t) => t.startsWith('scene: '))
    expect(item.textContent).not.toBe('scene: ' + long)
    expect(item.textContent).toMatch(/…$/)
    // The untruncated path is still available as the tooltip.
    expect(item.getAttribute('title')).toBe(long)
  })

  it('applies the paused class to the run-state item', () => {
    useEditor.setState({ enginePaused: true })
    render(<StatusBar />)
    const paused = screen.getByText('⏸ paused')
    expect(paused.className).toContain('paused')
  })

  it('renders running without the paused class', () => {
    useEditor.setState({ enginePaused: false })
    render(<StatusBar />)
    const running = screen.getByText('▶ running')
    expect(running.className).not.toContain('paused')
  })

  it('reflects a changing scene through the live bar (scene stays in sync)', () => {
    useEditor.setState({ engineConnected: true, engineScene: 'alpha' })
    const { rerender } = render(<StatusBar />)
    expect(screen.getByText('scene: alpha')).toBeTruthy()
    useEditor.setState({ engineScene: 'beta' })
    rerender(<StatusBar />)
    expect(screen.getByText('scene: beta')).toBeTruthy()
    expect(screen.queryByText('scene: alpha')).toBeNull()
  })
})

