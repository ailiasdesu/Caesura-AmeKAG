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
    // t35 RUN block: same client surface DebugView uses (evalRaw + stop).
    evalRaw: vi.fn(async (): Promise<string> => 'true'),
    stop: vi.fn(async (): Promise<{ status: string }> => ({ status: 'ok' })),
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

  // ------------------------------------------------------------------
  // t35: RUN block (project entry scene over the DebugView convention)
  // ------------------------------------------------------------------
  it('RUN renders enabled with a connected badge when the engine is connected', () => {
    useEditor.setState({ engineConnected: true })
    render(<BuildManagerView client={makeClient()} />)
    const run = screen.getByRole('button', { name: /^Run$/ }) as HTMLButtonElement
    const stop = screen.getByRole('button', { name: 'Stop' }) as HTMLButtonElement
    expect(run.disabled).toBe(false)
    expect(stop.disabled).toBe(false)
    expect(screen.getByText('connected')).toBeTruthy()
    expect(
      (screen.getByLabelText('Run story path') as HTMLInputElement).value,
    ).toBe('demo/example_game/story.ks')
  })

  it('RUN disables the controls and shows the honest hint when the engine is disconnected', () => {
    useEditor.setState({ engineConnected: false })
    render(<BuildManagerView client={makeClient()} />)
    const run = screen.getByRole('button', { name: /^Run$/ }) as HTMLButtonElement
    const stop = screen.getByRole('button', { name: 'Stop' }) as HTMLButtonElement
    expect(run.disabled).toBe(true)
    expect(stop.disabled).toBe(true)
    expect(screen.getByText('no engine')).toBeTruthy()
    expect(screen.getByText(/先以 --editor 启动引擎并 Connect/)).toBeTruthy()
  })

  it('clicking Run issues the kag_runner eval snippet for the entered story path', async () => {
    useEditor.setState({ engineConnected: true })
    const client = makeClient()
    render(<BuildManagerView client={client} />)
    fireEvent.change(screen.getByLabelText('Run story path'), {
      target: { value: 'projects/my_vn/story.ks' },
    })
    fireEvent.click(screen.getByRole('button', { name: /^Run$/ }))
    await new Promise((r) => setTimeout(r, 0))
    expect(client.evalRaw).toHaveBeenCalledTimes(1)
    const code = (client.evalRaw as ReturnType<typeof vi.fn>).mock.calls[0][0] as string
    expect(code).toContain('require("kag_runner")')
    expect(code).toContain('kr.start("projects/my_vn/story.ks")')
    expect(screen.getByText(/Scene: projects\/my_vn\/story\.ks/)).toBeTruthy()
  })

  it('clicking Stop issues client.stop() and surfaces the request', async () => {
    useEditor.setState({ engineConnected: true })
    const client = makeClient()
    render(<BuildManagerView client={client} />)
    fireEvent.click(screen.getByRole('button', { name: 'Stop' }))
    await new Promise((r) => setTimeout(r, 0))
    expect(client.stop).toHaveBeenCalledTimes(1)
    expect(screen.getByText('Stop requested')).toBeTruthy()
  })

  it('RUN surfaces the engine error and re-enables after evalRaw rejects (no unhandled rejection)', async () => {
    useEditor.setState({ engineConnected: true })
    const client = makeClient({
      evalRaw: vi.fn(async () => {
        throw new Error('kag_runner: bad path')
      }),
    })
    render(<BuildManagerView client={client} />)
    fireEvent.click(screen.getByRole('button', { name: /^Run$/ }))
    await new Promise((r) => setTimeout(r, 0))
    expect(screen.getByText('kag_runner: bad path')).toBeTruthy()
    const run = screen.getByRole('button', { name: /^Run$/ }) as HTMLButtonElement
    expect(run.disabled).toBe(false)
  })

  it('a Stop before the run completes keeps the Stop message (run-id guard)', async () => {
    useEditor.setState({ engineConnected: true })
    let resolveRun!: (v: string) => void
    const client = makeClient({
      evalRaw: vi.fn(
        () =>
          new Promise<string>((res) => {
            resolveRun = res
          }),
      ),
    })
    render(<BuildManagerView client={client} />)
    fireEvent.click(screen.getByRole('button', { name: /^Run$/ }))
    fireEvent.click(screen.getByRole('button', { name: 'Stop' }))
    await new Promise((r) => setTimeout(r, 0))
    expect(screen.getByText('Stop requested')).toBeTruthy()
    // Late run resolve must NOT overwrite the Stop message…
    resolveRun('true')
    await new Promise((r) => setTimeout(r, 0))
    expect(screen.queryByText(/Scene:/)).toBeNull()
    expect(screen.getByText('Stop requested')).toBeTruthy()
    // …while the busy flag still resets (finally stays unguarded).
    const run = screen.getByRole('button', { name: /^Run$/ }) as HTMLButtonElement
    expect(run.disabled).toBe(false)
  })

  it('RUN disables while a run is in flight and re-enables afterwards', async () => {
    useEditor.setState({ engineConnected: true })
    let resolve!: (v: string) => void
    const client = makeClient({
      evalRaw: vi.fn(
        () =>
          new Promise<string>((res) => {
            resolve = res
          }),
      ),
    })
    render(<BuildManagerView client={client} />)
    fireEvent.click(screen.getByRole('button', { name: /^Run$/ }))
    const running = screen.getByRole('button', { name: /Running…/ }) as HTMLButtonElement
    expect(running.disabled).toBe(true)
    resolve('true')
    await new Promise((r) => setTimeout(r, 0))
    const done = screen.getByRole('button', { name: /^Run$/ }) as HTMLButtonElement
    expect(done.disabled).toBe(false)
  })
})
