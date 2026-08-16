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
describe('SceneOutline (virtualized long scenes)', () => {
  const ROW = 20

  // Build a long .ks source: one label per 5 lines so total rows = sections + items.
  function longSource(lines: number): string {
    const out: string[] = []
    for (let i = 1; i <= lines; i++) {
      // every 5th line is a label; the rest are text/content lines
      if (i % 5 === 0) out.push('*mark' + i)
      else out.push('line ' + i)
    }
    return out.join('\n')
  }

  /** Scroll the outline's own (virtualized) container and then re-query. */
  function scrollOutlineTo(container: HTMLElement, scrollTop: number) {
    const scroller = container.querySelector('.outline-body') as HTMLElement
    fireEvent.scroll(scroller, { target: { scrollTop } })
  }

  it('renders only a window of rows for a long scene (fewer than total)', () => {
    // 1000 source lines -> ~800 non-label items + 200 label rows = 1000 rows.
    const { container } = render(
      <SceneOutline source={longSource(1000)} rowHeight={ROW} viewportHeight={200} />,
    )
    // visible window ≈ 200/20 + 2*overscan(8) = ~18 rows, far less than 1000.
    const renderedRows = container.querySelectorAll('.outline-row').length
    const renderedLabels = container.querySelectorAll('.outline-label-row').length
    const total = renderedRows + renderedLabels
    expect(total).toBeGreaterThan(0)
    expect(total).toBeLessThan(1000)
    // the first label/row (line 5 -> label *mark5) is visible at scroll 0
    expect(screen.getByText('*mark5')).toBeTruthy()
  })

  it('changes the rendered window when the scroll position moves', () => {
    const { container } = render(
      <SceneOutline source={longSource(1000)} rowHeight={ROW} viewportHeight={200} />,
    )
    // At scroll 0 a later label (*mark500) must NOT be mounted yet.
    expect(screen.queryByText('*mark500')).toBeNull()

    // scroll down far enough that *mark500 lands in the window
    scrollOutlineTo(container, 500 * ROW - 60)
    // the earlier label is now virtualized away
    expect(screen.queryByText('*mark5')).toBeNull()
    // and the label near the new scroll position is mounted
    expect(screen.getByText('*mark500')).toBeTruthy()
  })

  it('shows outline-current highlight on the matching row when it is visible', () => {
    const { container } = render(
      <SceneOutline
        source={longSource(1000)}
        currentLine={6}
        rowHeight={ROW}
        viewportHeight={200}
      />,
    )
    const highlight = container.querySelectorAll('.outline-current')
    expect(highlight.length).toBe(1)
    // row for source line 6 (a text line after the *mark5 label)
    expect(highlight[0].textContent).toContain('6')
  })

  it('auto-scrolls so a highlight row outside the initial window becomes visible', () => {
    const { container } = render(
      <SceneOutline
        source={longSource(1000)}
        currentLine={505}
        rowHeight={ROW}
        viewportHeight={200}
      />,
    )
    // The reveal scrolled the window down; the first label should no longer be present.
    expect(screen.queryByText('*mark5')).toBeNull()
    // the highlight row (currentLine=505 -> a text row) is now rendered
    const highlight = Array.from(container.querySelectorAll('.outline-current'))
    expect(highlight.length).toBe(1)
    expect(highlight[0].textContent).toContain('505')
  })
})
