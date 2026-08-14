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
