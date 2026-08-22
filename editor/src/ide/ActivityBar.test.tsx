// @vitest-environment jsdom
import { describe, it, expect, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { ActivityBar } from './ActivityBar'
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

const ICONS = ['📁', '🗂', '🐞', '🎬', '✨', '⚙️', '🔨']
const LABELS = [
  'Explorer (assets)',
  'Projects',
  'Run and Debug',
  'Visual Preview',
  'AI Writer',
  'Settings',
  'Build',
]

describe('ActivityBar (component)', () => {
  it('renders all seven view buttons with icons and labels', () => {
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    expect(buttons).toHaveLength(7)
    buttons.forEach((b, i) => expect(b.textContent).toBe(ICONS[i]))
    expect(buttons[0].getAttribute('title')).toBe('Explorer (assets)')
    expect(buttons[1].getAttribute('title')).toBe('Projects')
    expect(buttons[5].getAttribute('title')).toBe('Settings')
  })

  it('marks the active view with the active class', () => {
    useEditor.setState({ sideView: 'visual' })
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    expect(buttons[3].className).toContain('active')
    for (const [idx, b] of buttons.entries()) {
      if (idx !== 3) expect(b.className).not.toContain('active')
    }
  })

  it('switches the active view on click', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[4])
    expect(useEditor.getState().sideView).toBe('ai')
    fireEvent.click(screen.getAllByRole('button')[2])
    expect(useEditor.getState().sideView).toBe('debug')
  })

  it('switches to the project view from its button', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[1])
    expect(useEditor.getState().sideView).toBe('project')
  })

  it('switches to the build view from its button (7th)', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[6])
    expect(useEditor.getState().sideView).toBe('build')
  })

  it('activates the settings view from its button', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[5])
    expect(useEditor.getState().sideView).toBe('settings')
  })

  it('updates the active class after a click', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[3])
    const buttons = screen.getAllByRole('button')
    expect(buttons[3].className).toContain('active')
    expect(buttons[0].className).not.toContain('active')
  })
})
describe('ActivityBar (accessibility & state machine)', () => {
  it('exposes every view button with a role and an accessible label', () => {
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    expect(buttons).toHaveLength(7)
    for (const b of buttons) {
      expect(b.getAttribute('aria-label')).toBeTruthy()
      expect(b.textContent?.length).toBeGreaterThan(0)
    }
    expect(buttons[0].getAttribute('aria-label')).toBe('Explorer (assets)')
    expect(buttons[5].getAttribute('aria-label')).toBe('Settings')
    expect(buttons[5].textContent).toBe('⚙️')
  })

  it('marks the active view with aria-current and clears it elsewhere', () => {
    useEditor.setState({ sideView: 'ai' })
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    expect(buttons[4].getAttribute('aria-current')).toBe('true')
    for (const [idx, b] of buttons.entries()) {
      if (idx !== 4) expect(b.getAttribute('aria-current')).toBeNull()
    }
  })

  it('round-trips through views and settles back on a prior selection', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[2])
    expect(useEditor.getState().sideView).toBe('debug')
    fireEvent.click(screen.getAllByRole('button')[3])
    expect(useEditor.getState().sideView).toBe('visual')
    fireEvent.click(screen.getAllByRole('button')[2])
    expect(useEditor.getState().sideView).toBe('debug')
    fireEvent.click(screen.getAllByRole('button')[0])
    expect(useEditor.getState().sideView).toBe('explorer')
  })

  it('re-clicking the already-active view keeps it active (idempotent)', () => {
    useEditor.setState({ sideView: 'settings' })
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[5])
    expect(useEditor.getState().sideView).toBe('settings')
    const buttons = screen.getAllByRole('button')
    expect(buttons[5].className).toContain('active')
    expect(buttons[5].getAttribute('aria-current')).toBe('true')
  })

  it('keeps the Debug button enabled whether or not the engine is connected', () => {
    render(<ActivityBar />)
    const debug = screen.getAllByRole('button')[2]
    expect((debug as HTMLButtonElement).disabled).toBe(false)
    expect(debug.getAttribute('aria-label')).toBe('Run and Debug')
    fireEvent.click(debug)
    expect(useEditor.getState().sideView).toBe('debug')
  })

  it('keeps buttons focusable in visual order (keyboard tab sequence)', () => {
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    const order = buttons.map((b) => b.getAttribute('aria-label'))
    expect(order).toEqual(LABELS)
    for (const b of buttons) {
      expect((b as HTMLButtonElement).tabIndex).toBe(0)
      expect((b as HTMLButtonElement).disabled).toBe(false)
    }
  })

  it('updates aria-current immediately after switching via click', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[4])
    const buttons = screen.getAllByRole('button')
    expect(buttons[4].getAttribute('aria-current')).toBe('true')
    expect(buttons[0].getAttribute('aria-current')).toBeNull()
  })
})