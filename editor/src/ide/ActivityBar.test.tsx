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
  it('renders all four view buttons with icons and labels', () => {
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    expect(buttons).toHaveLength(4)
    expect(buttons[0].textContent).toBe('📁')
    expect(buttons[1].textContent).toBe('🐞')
    expect(buttons[2].textContent).toBe('🎬')
    expect(buttons[3].textContent).toBe('✨')
    expect(buttons[0].getAttribute('title')).toBe('Explorer (assets)')
    expect(buttons[1].getAttribute('title')).toBe('Run and Debug')
    expect(buttons[2].getAttribute('title')).toBe('Visual Preview')
    expect(buttons[3].getAttribute('title')).toBe('AI Writer')
  })

  it('marks the active view with the active class', () => {
    useEditor.setState({ sideView: 'visual' })
    render(<ActivityBar />)
    const buttons = screen.getAllByRole('button')
    expect(buttons[2].className).toContain('active')
    expect(buttons[0].className).not.toContain('active')
    expect(buttons[1].className).not.toContain('active')
    expect(buttons[3].className).not.toContain('active')
  })

  it('switches the active view on click', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[3])
    expect(useEditor.getState().sideView).toBe('ai')
    fireEvent.click(screen.getAllByRole('button')[1])
    expect(useEditor.getState().sideView).toBe('debug')
  })

  it('updates the active class after a click', () => {
    render(<ActivityBar />)
    fireEvent.click(screen.getAllByRole('button')[2])
    const buttons = screen.getAllByRole('button')
    expect(buttons[2].className).toContain('active')
    expect(buttons[0].className).not.toContain('active')
  })
})
