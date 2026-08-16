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

describe('ActivityBar (component)', () => {
  it('renders all five view buttons with icons and labels', () => {
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    expect(buttons).toHaveLength(5)
    expect(buttons[0].textContent).toBe('📁')
    expect(buttons[1].textContent).toBe('🐞')
    expect(buttons[2].textContent).toBe('🎬')
    expect(buttons[3].textContent).toBe('✨')
    expect(buttons[4].textContent).toBe('⚙️')
    expect(buttons[0].getAttribute('title')).toBe('Explorer (assets)')
    expect(buttons[1].getAttribute('title')).toBe('Run and Debug')
    expect(buttons[2].getAttribute('title')).toBe('Visual Preview')
    expect(buttons[3].getAttribute('title')).toBe('AI Writer')
    expect(buttons[4].getAttribute('title')).toBe('Settings')
  })

  it('marks the active view with the active class', () => {
    useEditor.setState({ sideView: 'visual' })
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    expect(buttons[2].className).toContain('active')
    expect(buttons[0].className).not.toContain('active')
    expect(buttons[1].className).not.toContain('active')
    expect(buttons[3].className).not.toContain('active')
    expect(buttons[4].className).not.toContain('active')
  })

  it('switches the active view on click', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[3])
    expect(useEditor.getState().sideView).toBe('ai')
    fireEvent.click(screen.getAllByRole('button')[1])
    expect(useEditor.getState().sideView).toBe('debug')
  })

  it('activates the settings view from its button', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[4])
    expect(useEditor.getState().sideView).toBe('settings')
  })

  it('updates the active class after a click', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[2])
    const buttons = screen.getAllByRole('button')
    expect(buttons[2].className).toContain('active')
    expect(buttons[0].className).not.toContain('active')
  })
})
describe('ActivityBar (accessibility & state machine)', () => {
  it('exposes every view button with a role and an accessible label', () => {
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    expect(buttons).toHaveLength(5)
    for (const b of buttons) {
      expect(b.getAttribute('aria-label')).toBeTruthy()
      expect(b.textContent?.length).toBeGreaterThan(0)
    }
    expect(buttons[0].getAttribute('aria-label')).toBe('Explorer (assets)')
    expect(buttons[4].getAttribute('aria-label')).toBe('Settings')
    // The icon glyph is visually hidden from the accessible name.
    expect(buttons[4].textContent).toBe('⚙️')
  })

  it('marks the active view with aria-current and clears it elsewhere', () => {
    useEditor.setState({ sideView: 'ai' })
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    expect(buttons[3].getAttribute('aria-current')).toBe('true')
    for (const [idx, b] of buttons.entries()) {
      if (idx !== 3) expect(b.getAttribute('aria-current')).toBeNull()
    }
  })

  it('round-trips through views and settles back on a prior selection', () => {
    render(<ActivityBar />)
    // explorer -> debug -> visual -> back to debug -> explorer
    fireEvent.click(screen.getAllByRole('button')[1])
    expect(useEditor.getState().sideView).toBe('debug')
    fireEvent.click(screen.getAllByRole('button')[2])
    expect(useEditor.getState().sideView).toBe('visual')
    fireEvent.click(screen.getAllByRole('button')[1])
    expect(useEditor.getState().sideView).toBe('debug')
    fireEvent.click(screen.getAllByRole('button')[0])
    expect(useEditor.getState().sideView).toBe('explorer')
  })

  it('re-clicking the already-active view keeps it active (idempotent)', () => {
    useEditor.setState({ sideView: 'settings' })
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[4])
    expect(useEditor.getState().sideView).toBe('settings')
    const buttons = screen.getAllByRole('button')
    expect(buttons[4].className).toContain('active')
    expect(buttons[4].getAttribute('aria-current')).toBe('true')
  })

  it('keeps the Debug button enabled whether or not the engine is connected', () => {
    // ActivityBar does NOT gate navigation on engine state: view switching is
    // always available; the Debug *panel* degrades its controls when offline.
    render(<ActivityBar />)
    const debug = screen.getAllByRole('button')[1]
    expect((debug as HTMLButtonElement).disabled).toBe(false)
    expect(debug.getAttribute('aria-label')).toBe('Run and Debug')
    fireEvent.click(debug)
    expect(useEditor.getState().sideView).toBe('debug')
  })

  it('keeps buttons focusable in visual order (keyboard tab sequence)', () => {
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    const order = buttons.map((b) => b.getAttribute('aria-label'))
    expect(order).toEqual([
      'Explorer (assets)',
      'Run and Debug',
      'Visual Preview',
      'AI Writer',
      'Settings',
    ])
    // All are reachable via Tab (not disabled / not hidden).
    for (const b of buttons) {
      expect((b as HTMLButtonElement).tabIndex).toBe(0)
      expect((b as HTMLButtonElement).disabled).toBe(false)
    }
  })

  it('updates aria-current immediately after switching via click', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[3])
    const buttons = screen.getAllByRole('button')
    expect(buttons[3].getAttribute('aria-current')).toBe('true')
    expect(buttons[0].getAttribute('aria-current')).toBeNull()
  })
})

