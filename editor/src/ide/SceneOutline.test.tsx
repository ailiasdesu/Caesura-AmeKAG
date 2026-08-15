// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { SceneOutline } from './SceneOutline'

const KS_SOURCE = [
  '*start',
  '[bg storage="room.png"]',
  'It was a quiet morning.',
  '[ch name="Hero" text="Good morning"]',
  '*next',
  '[playbgm file="bgm.ogg" loop=1]',
].join('\n')

function commandTexts(root: HTMLElement): string[] {
  return Array.from(root.querySelectorAll('.outline-cmd')).map((el) => el.textContent ?? '')
}

function containerJumpButtons(): number {
  return document.querySelectorAll('.outline-jump').length
}

beforeEach(() => cleanup())

describe('SceneOutline (component)', () => {
  it('renders *labels as headings and commands/text as rows', () => {
    const { container } = render(<SceneOutline source={KS_SOURCE} />)

    // two label headings
    expect(screen.getAllByText(/^\*start$/)).toHaveLength(1)
    expect(screen.getAllByText(/^\*next$/)).toHaveLength(1)

    // command rows carry the command name and key params
    expect(commandTexts(container)).toEqual(['[bg]', '[ch]', '[playbgm]'])
    expect(screen.getByText(/storage=room\.png/)).toBeTruthy()

    // text dialogue line is shown as content
    expect(screen.getByText('It was a quiet morning.')).toBeTruthy()
  })

  it('shows an empty hint for an empty / comment-only scene', () => {
    const { rerender } = render(<SceneOutline source="; nothing here" />)
    expect(screen.getByText('No scene outline (empty script)')).toBeTruthy()

    rerender(<SceneOutline source="" />)
    expect(screen.getByText('No scene outline (empty script)')).toBeTruthy()
  })

  it('does not crash on a malformed unclosed [tag', () => {
    const { container } = render(
      <SceneOutline source={'*start\n[ch text="a\n---[unclosed\n[bg]'} />,
    )
    expect(container.querySelectorAll('.outline-row').length).toBeGreaterThanOrEqual(3)
  })

  it('fires onSelectLabel with the label name when a heading is clicked', () => {
    const onSelectLabel = vi.fn()
    render(<SceneOutline source={KS_SOURCE} onSelectLabel={onSelectLabel} />)

    fireEvent.click(screen.getByText(/^\*start$/))
    expect(onSelectLabel).toHaveBeenCalledWith('start', 1)

    fireEvent.click(screen.getByText(/^\*next$/))
    expect(onSelectLabel).toHaveBeenCalledWith('next', 5)
    expect(onSelectLabel).toHaveBeenCalledTimes(2)
  })

  it('is a no-op when onSelectLabel is not provided', () => {
    expect(() => {
      const { container } = render(<SceneOutline source={KS_SOURCE} />)
      fireEvent.click(screen.getByText(/^\*start$/))
      expect(container.querySelectorAll('.outline-label-row').length).toBe(2)
    }).not.toThrow()
  })

  it('shows the section count in the panel title', () => {
    render(<SceneOutline source={KS_SOURCE} />)
    const title = screen.getByText('Scene Outline').parentElement
    expect(title?.textContent).toContain('2 sections')
  })

  it('does not render a jump button when onJumpToLabel is not provided', () => {
    render(<SceneOutline source={KS_SOURCE} />)
    expect(containerJumpButtons()).toBe(0)
  })

  it('renders a per-label ▶ jump button when onJumpToLabel is provided', () => {
    const onJumpToLabel = vi.fn()
    render(<SceneOutline source={KS_SOURCE} onJumpToLabel={onJumpToLabel} />)
    expect(containerJumpButtons()).toBe(2)

    fireEvent.click(screen.getAllByTitle('Jump running scene to *next')[0])
    expect(onJumpToLabel).toHaveBeenCalledWith('next', 5)
    expect(onJumpToLabel).toHaveBeenCalledTimes(1)
  })

  it('fires onJumpToLabel on a label heading secondary click (right-click)', () => {
    const onJumpToLabel = vi.fn()
    const onSelectLabel = vi.fn()
    render(
      <SceneOutline
        source={KS_SOURCE}
        onSelectLabel={onSelectLabel}
        onJumpToLabel={onJumpToLabel}
      />,
    )

    fireEvent.contextMenu(screen.getByText(/^\*start$/))
    expect(onJumpToLabel).toHaveBeenCalledWith('start', 1)
    // secondary click must not also select (no onSelectLabel call)
    expect(onSelectLabel).not.toHaveBeenCalled()
  })

  it('jump button click does not bubble into the row select handler', () => {
    const onJumpToLabel = vi.fn()
    const onSelectLabel = vi.fn()
    render(
      <SceneOutline
        source={KS_SOURCE}
        onSelectLabel={onSelectLabel}
        onJumpToLabel={onJumpToLabel}
      />,
    )

    fireEvent.click(screen.getAllByTitle('Jump running scene to *next')[0])
    expect(onJumpToLabel).toHaveBeenCalledTimes(1)
    expect(onSelectLabel).not.toHaveBeenCalled()
  })

  it('jump affordances are no-ops when onJumpToLabel is absent', () => {
    render(<SceneOutline source={KS_SOURCE} onSelectLabel={() => {}} />)
    expect(() => {
      fireEvent.contextMenu(screen.getByText(/^\*start$/))
    }).not.toThrow()
  })
})
