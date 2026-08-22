// @vitest-environment jsdom
import { describe, it, expect, beforeEach, vi } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { BuildManagerView } from './BuildManagerView'
import { useEditor } from '../store'
import {
  RpcError,
  type BuildReply,
  type EngineClient,
  type WebPackageReply,
} from '../lib/rpc'

function makeClient(over: Partial<EngineClient> = {}): EngineClient {
  return {
    buildCarc: vi.fn(async (): Promise<BuildReply> => ({
      status: 'ok',
      path: 'build/game.carc',
      size: 2048,
      files: 12,
    })),
    packageWeb: vi.fn(async (): Promise<WebPackageReply> => ({
      ok: true,
      outputDir: 'dist/example_game',
      logTail: '  PACKAGE COMPLETE -> dist/example_game\n',
    })),
    ...over,
  } as unknown as EngineClient
}

beforeEach(() => {
  cleanup()
  useEditor.setState({ sideView: 'explorer' })
})

describe('BuildManagerView (component)', () => {
  it('renders the CARC inputs with engine defaults and the engine-executed web note', () => {
    render(<BuildManagerView client={makeClient()} />)
    expect(
      (screen.getByLabelText('CARC output path') as HTMLInputElement).value,
    ).toBe('build/game.carc')
    expect(
      (screen.getByLabelText('CARC key path') as HTMLInputElement).value,
    ).toBe('build/game.key')
    // Web packaging is executed by POST /api/package/web engine-side; the
    // panel must name both the endpoint and the script it wraps.
    expect(screen.getByText(/WEB PACKAGE/)).toBeTruthy()
    expect(screen.getByText(/api\/package\/web/)).toBeTruthy()
    expect(screen.getByText(/scripts\/package_game\.sh/)).toBeTruthy()
    expect(
      (screen.getByLabelText('Web package story path') as HTMLInputElement)
        .value,
    ).toBe('demo/example_game/story.ks')
    expect(
      (screen.getByLabelText('Web package output name') as HTMLInputElement)
        .value,
    ).toBe('example_game')
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

  it('clicking Package Web calls packageWeb with the entered story and name', async () => {
    const client = makeClient()
    render(<BuildManagerView client={client} />)
    fireEvent.change(screen.getByLabelText('Web package story path'), {
      target: { value: 'projects/demo/story.ks' },
    })
    fireEvent.change(screen.getByLabelText('Web package output name'), {
      target: { value: 'my_game' },
    })
    fireEvent.click(screen.getByRole('button', { name: 'Package Web' }))
    await new Promise((r) => setTimeout(r, 0))
    expect(client.packageWeb).toHaveBeenCalledWith('projects/demo/story.ks', 'my_game')
    expect(screen.getByText(/Packaged dist\/example_game/)).toBeTruthy()
    expect(screen.getByText(/PACKAGE COMPLETE/)).toBeTruthy()
  })

  it('blanking the web inputs falls back to undefined args (engine defaults)', async () => {
    const client = makeClient()
    render(<BuildManagerView client={client} />)
    fireEvent.change(screen.getByLabelText('Web package story path'), {
      target: { value: '  ' },
    })
    fireEvent.change(screen.getByLabelText('Web package output name'), {
      target: { value: '' },
    })
    fireEvent.click(screen.getByRole('button', { name: 'Package Web' }))
    await new Promise((r) => setTimeout(r, 0))
    expect(client.packageWeb).toHaveBeenCalledWith(undefined, undefined)
  })

  it('web failure surfaces the engine error and log tail from the RpcError body', async () => {
    const client = makeClient()
    ;(client.packageWeb as ReturnType<typeof vi.fn>).mockRejectedValue(
      new RpcError('HTTP 500 on /package/web', 500, {
        ok: false,
        error: 'package_game.sh failed with exit code 1',
        outputDir: 'dist/example_game',
        logTail: '[package] FAIL: contract check failed for demo/x.ks',
      }),
    )
    render(<BuildManagerView client={client} />)
    fireEvent.click(screen.getByRole('button', { name: 'Package Web' }))
    await new Promise((r) => setTimeout(r, 0))
    expect(screen.getByText('package_game.sh failed with exit code 1')).toBeTruthy()
    expect(screen.getByText(/contract check failed/)).toBeTruthy()
  })

  it('web failure without a log tail still shows the error only', async () => {
    const client = makeClient({
      packageWeb: vi.fn(async (): Promise<WebPackageReply> => ({
        ok: false,
        error: 'storyPath must live under assets/, demo/, tests/projects/ or projects/',
      })),
    })
    render(<BuildManagerView client={client} />)
    fireEvent.click(screen.getByRole('button', { name: 'Package Web' }))
    await new Promise((r) => setTimeout(r, 0))
    expect(
      screen.getByText(/storyPath must live under assets\//),
    ).toBeTruthy()
  })

  it('disables Package Web while running', async () => {
    let resolve!: (r: WebPackageReply) => void
    const client = {
      packageWeb: vi.fn(
        () =>
          new Promise<WebPackageReply>((res) => {
            resolve = res
          }),
      ),
    } as unknown as EngineClient
    render(<BuildManagerView client={client} />)
    fireEvent.click(screen.getByRole('button', { name: 'Package Web' }))
    const packaging = screen.getByRole('button', {
      name: /Packaging/,
    }) as HTMLButtonElement
    expect(packaging.disabled).toBe(true)
    resolve({ ok: true, outputDir: 'dist/example_game' })
    await new Promise((r) => setTimeout(r, 0))
    const done = screen.getByRole('button', { name: 'Package Web' }) as HTMLButtonElement
    expect(done.disabled).toBe(false)
  })

  it('points Run at the Debug panel without duplicating it', () => {
    render(<BuildManagerView client={makeClient()} />)
    expect(screen.getByText(/Run Current Scene/)).toBeTruthy()
    fireEvent.click(screen.getByRole('button', { name: 'Open Debug Panel' }))
    expect(useEditor.getState().sideView).toBe('debug')
  })
})
