// @vitest-environment jsdom
import { describe, it, expect, beforeEach, vi } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { BuildManagerView } from './BuildManagerView'
import { useEditor } from '../store'
import type { BuildReply, EngineClient } from '../lib/rpc'

function makeClient(over: Partial<EngineClient> = {}): EngineClient {
  return {
    buildCarc: vi.fn(async (): Promise<BuildReply> => ({
      status: 'ok',
      path: 'build/game.carc',
      size: 2048,
      files: 12,
    })),
    ...over,
  } as unknown as EngineClient
}

beforeEach(() => {
  cleanup()
  useEditor.setState({ sideView: 'explorer' })
})

describe('BuildManagerView (component)', () => {
  it('renders the CARC inputs with engine defaults and the honest script-only note', () => {
    render(<BuildManagerView client={makeClient()} />)
    expect(
      (screen.getByLabelText('CARC output path') as HTMLInputElement).value,
    ).toBe('build/game.carc')
    expect(
      (screen.getByLabelText('CARC key path') as HTMLInputElement).value,
    ).toBe('build/game.key')
    // Task book §18.4: never pretend one-click web packaging — the panel
    // must state that only the repo script provides it.
    expect(screen.getByText(/SCRIPT-ONLY/)).toBeTruthy()
    expect(screen.getByText(/scripts\/package_game\.sh/)).toBeTruthy()
    expect(
      (screen.getByLabelText('Web package command') as HTMLInputElement).value,
    ).toContain('bash scripts/package_game.sh')
  })

  it('clicking Build calls buildCarc with the entered paths and shows the result', async () => {
    const client = makeClient()
    render(<BuildManagerView client={client} />)
    fireEvent.change(screen.getByLabelText('CARC output path'), {
      target: { value: 'build/my.carc' },
    })
    fireEvent.click(screen.getByRole('button', { name: 'Build CARC' }))
    await new Promise((r) => setTimeout(r, 0))
    expect(client.buildCarc).toHaveBeenCalledWith('build/my.carc', 'build/game.key')
    expect(screen.getByText(/Built build\/game\.carc/)).toBeTruthy()
    expect(screen.getByText(/2048 bytes/)).toBeTruthy()
    expect(screen.getByText(/12 files/)).toBeTruthy()
  })

  it('blanking the inputs falls back to undefined args (engine defaults)', async () => {
    const client = makeClient()
    render(<BuildManagerView client={client} />)
    fireEvent.change(screen.getByLabelText('CARC output path'), {
      target: { value: '   ' },
    })
    fireEvent.change(screen.getByLabelText('CARC key path'), {
      target: { value: '' },
    })
    fireEvent.click(screen.getByRole('button', { name: 'Build CARC' }))
    await new Promise((r) => setTimeout(r, 0))
    expect(client.buildCarc).toHaveBeenCalledWith(undefined, undefined)
  })

  it('shows the engine error when the reply is not ok or the RPC rejects', async () => {
    const failing = makeClient({
      buildCarc: vi.fn(async (): Promise<BuildReply> => ({
        status: 'error',
        error: 'No files to package',
      })),
    })
    const { unmount } = render(<BuildManagerView client={failing} />)
    fireEvent.click(screen.getByRole('button', { name: 'Build CARC' }))
    await new Promise((r) => setTimeout(r, 0))
    expect(screen.getByText('No files to package')).toBeTruthy()
    unmount()

    const rejected = makeClient()
    ;(rejected.buildCarc as ReturnType<typeof vi.fn>).mockRejectedValue(
      new Error('HTTP 400 on /build'),
    )
    render(<BuildManagerView client={rejected} />)
    fireEvent.click(screen.getByRole('button', { name: 'Build CARC' }))
    await new Promise((r) => setTimeout(r, 0))
    expect(screen.getByText('HTTP 400 on /build')).toBeTruthy()
  })

  it('disables Build while running and re-enables afterwards', async () => {
    let resolve!: (r: BuildReply) => void
    const client = {
      buildCarc: vi.fn(
        () =>
          new Promise<BuildReply>((res) => {
            resolve = res
          }),
      ),
    } as unknown as EngineClient
    render(<BuildManagerView client={client} />)
    fireEvent.click(screen.getByRole('button', { name: 'Build CARC' }))
    const building = screen.getByRole('button', { name: /Building/ }) as HTMLButtonElement
    expect(building.disabled).toBe(true)
    resolve({ status: 'ok', path: 'build/game.carc' })
    await new Promise((r) => setTimeout(r, 0))
    const done = screen.getByRole('button', { name: 'Build CARC' }) as HTMLButtonElement
    expect(done.disabled).toBe(false)
  })

  it('points Run at the Debug panel without duplicating it', () => {
    render(<BuildManagerView client={makeClient()} />)
    expect(screen.getByText(/Run Current Scene/)).toBeTruthy()
    fireEvent.click(screen.getByRole('button', { name: 'Open Debug Panel' }))
    expect(useEditor.getState().sideView).toBe('debug')
  })
})
