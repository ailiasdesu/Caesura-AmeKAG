// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup, waitFor } from '@testing-library/react'
import { DebugView } from './DebugView'
import type { EngineClient } from '../lib/rpc'
import { useEditor } from '../store'

type Client = Pick<
  EngineClient,
  | 'debugState'
  | 'run'
  | 'stop'
  | 'setBreakpoint'
  | 'removeBreakpoint'
  | 'clearBreakpoints'
  | 'debugContinue'
  | 'inspect'
  | 'frame'
>

const makeClient = (overrides: Partial<Client> = {}): Client => ({
  debugState: vi.fn(async () => ({ status: 'ok', scene: 'prologue', token_index: 12, paused: false })),
  run: vi.fn(async () => ({ status: 'ok' } as { status: string })),
  stop: vi.fn(async () => ({ status: 'ok' })),
  setBreakpoint: vi.fn(async () => ({ status: 'ok' })),
  removeBreakpoint: vi.fn(async () => ({ status: 'ok' })),
  clearBreakpoints: vi.fn(async () => ({ status: 'ok' })),
  debugContinue: vi.fn(async () => ({ status: 'ok' })),
  inspect: vi.fn(async () => 42),
  frame: vi.fn(async () => ({ status: 'ok', width: 640, height: 360, png: 'aGVsbG8=' })),
  ...overrides,
})

/** Render with a connected engine and wait until the first poll lands. */
async function renderConnected(client: Client) {
  render(<DebugView client={client as unknown as EngineClient} />)
  await screen.findByText('prologue')
}

beforeEach(() => {
  cleanup()
  useEditor.setState({
    docs: [],
    activePath: null,
    sideView: 'debug',
    engineConnected: false,
    engineScene: '',
    engineToken: 0,
    enginePaused: false,
    revealRequest: null,
  })
})

describe('DebugView (component)', () => {
  it('polls debugState and mirrors scene/token/paused into the store', async () => {
    render(<DebugView client={makeClient() as unknown as EngineClient} />)
    await screen.findByText('prologue')
    expect(screen.getByText('12')).toBeTruthy()
    expect(screen.getByText('running')).toBeTruthy()
    await waitFor(() => {
      const s = useEditor.getState()
      expect(s.engineConnected).toBe(true)
      expect(s.engineScene).toBe('prologue')
      expect(s.engineToken).toBe(12)
      expect(s.enginePaused).toBe(false)
    })
  })

  it('mirrors current_cmd into the store', async () => {
    const client = makeClient({
      debugState: vi.fn(async () => ({ status: 'ok', scene: 'ep2', token_index: 3, paused: false, current_cmd: '[ch]' })),
    })
    render(<DebugView client={client as unknown as EngineClient} />)
    await screen.findByText('ep2')
    await waitFor(() => {
      expect(useEditor.getState().engineCmd).toBe('[ch]')
    })
  })

  it('shows paused badge and mirrors paused state', async () => {
    const client = makeClient({
      debugState: vi.fn(async () => ({ status: 'ok', scene: 'ep2', token_index: 3, paused: true })),
    })
    render(<DebugView client={client as unknown as EngineClient} />)
    await screen.findByText('ep2')
    expect(screen.getByText('paused')).toBeTruthy()
    await waitFor(() => {
      expect(useEditor.getState().enginePaused).toBe(true)
    })
  })

  it('marks the engine offline when debugState fails', async () => {
    const client = makeClient({
      debugState: vi.fn(async () => {
        throw new Error('connection refused')
      }),
    })
    render(<DebugView client={client as unknown as EngineClient} />)
    await waitFor(() => {
      expect(useEditor.getState().engineConnected).toBe(false)
    })
  })

  it('runs the script and reports completion', async () => {
    const client = makeClient()
    await renderConnected(client)
    fireEvent.change(screen.getByPlaceholderText('-- Lua to run in the engine (optional)'), {
      target: { value: 'print(42)' },
    })
    fireEvent.click(screen.getByText('Run'))
    await screen.findByText('Script completed')
    expect(client.run).toHaveBeenCalledWith('print(42)')
  })

  it('reports run errors in the message line', async () => {
    const client = makeClient({
      run: vi.fn(async () => {
        throw new Error('Lua syntax error')
      }),
    })
    await renderConnected(client)
    fireEvent.click(screen.getByText('Run'))
    await screen.findByText('Lua syntax error')
  })

  it('disables Run while a script is executing', async () => {
    let release: (v: { status: string }) => void = () => {}
    const gate = new Promise<{ status: string }>((res) => {
      release = res
    })
    const client = makeClient({
      run: vi.fn(() => gate),
    })
    await renderConnected(client)
    fireEvent.click(screen.getByText('Run'))
    const btn = await screen.findByText('Running…')
    expect((btn as HTMLButtonElement).disabled).toBe(true)
    release({ status: 'ok' })
    await screen.findByText('Script completed')
    expect(screen.getByText('Run').closest('button')?.disabled).toBe(false)
  })

  it('requests stop', async () => {
    const client = makeClient()
    await renderConnected(client)
    fireEvent.click(screen.getByText('Stop'))
    await screen.findByText('Stop requested')
    expect(client.stop).toHaveBeenCalled()
  })

  it('sets a breakpoint from the scene/line inputs and lists it', async () => {
    const client = makeClient()
    await renderConnected(client)
    fireEvent.change(screen.getByPlaceholderText('assets/script/main.ks'), {
      target: { value: 'assets/script/ch2.ks' },
    })
    fireEvent.change(screen.getByPlaceholderText('line'), {
      target: { value: '17' },
    })
    fireEvent.click(screen.getByText('Set Breakpoint'))
    await screen.findByText('Breakpoint: assets/script/ch2.ks:17')
    expect(client.setBreakpoint).toHaveBeenCalledWith('assets/script/ch2.ks', 17)
    await waitFor(() => {
      expect(document.querySelector('.bp-item-src')?.textContent).toContain('ch2.ks')
    })
  })

  it('falls back to line 1 for a non-numeric breakpoint line and shows the resolved line', async () => {
    const client = makeClient()
    await renderConnected(client)
    fireEvent.change(screen.getByPlaceholderText('line'), {
      target: { value: 'abc' },
    })
    fireEvent.click(screen.getByText('Set Breakpoint'))
    await screen.findByText('Breakpoint: assets/script/main.ks:1')
    expect(client.setBreakpoint).toHaveBeenCalledWith('assets/script/main.ks', 1)
  })

  it('dedupes and line-sorts breakpoints in the list', async () => {
    const client = makeClient()
    await renderConnected(client)
    fireEvent.change(screen.getByPlaceholderText('line'), { target: { value: '30' } })
    fireEvent.click(screen.getByText('Set Breakpoint'))
    await screen.findByText('Breakpoint: assets/script/main.ks:30')
    fireEvent.change(screen.getByPlaceholderText('line'), { target: { value: '5' } })
    fireEvent.click(screen.getByText('Set Breakpoint'))
    await screen.findByText('Breakpoint: assets/script/main.ks:5')
    fireEvent.click(screen.getByText('Set Breakpoint')) // re-set 5 -> dedupe
    await screen.findByText('Breakpoint: assets/script/main.ks:5')
    const itemRows = document.querySelectorAll('.bp-item')
    expect(itemRows.length).toBe(2)
    expect(client.setBreakpoint).toHaveBeenCalledTimes(3)
  })

  it('removes an individual breakpoint via RPC', async () => {
    const client = makeClient()
    await renderConnected(client)
    fireEvent.change(screen.getByPlaceholderText('line'), { target: { value: '17' } })
    fireEvent.click(screen.getByText('Set Breakpoint'))
    await screen.findByText('Breakpoint: assets/script/main.ks:17')
    const rmBtn = screen.getByLabelText('Remove assets/script/main.ks:17')
    fireEvent.click(rmBtn)
    await waitFor(() => {
      expect(client.removeBreakpoint).toHaveBeenCalledWith('assets/script/main.ks', 17)
    })
    await waitFor(() => {
      expect(document.querySelectorAll('.bp-item').length).toBe(0)
    })
  })

  it('clears all breakpoints (list + RPC)', async () => {
    const client = makeClient()
    await renderConnected(client)
    fireEvent.change(screen.getByPlaceholderText('line'), { target: { value: '17' } })
    fireEvent.click(screen.getByText('Set Breakpoint'))
    await screen.findByText('Breakpoint: assets/script/main.ks:17')
    fireEvent.click(screen.getByText('Clear All'))
    await screen.findByText('Breakpoints cleared')
    expect(client.clearBreakpoints).toHaveBeenCalled()
    await waitFor(() => {
      expect(document.querySelectorAll('.bp-item').length).toBe(0)
    })
  })

  it('disables breakpoint/inspect/frame controls and shows a hint while offline', async () => {
    const client = makeClient({
      debugState: vi.fn(async () => {
        throw new Error('disconnected')
      }),
    })
    render(<DebugView client={client as unknown as EngineClient} />)
    await screen.findByText(/Engine disconnected/)
    expect(screen.getByText('Run').closest('button')?.disabled).toBe(true)
    expect(screen.getByText('Stop').closest('button')?.disabled).toBe(true)
    expect(screen.getByText('Continue').closest('button')?.disabled).toBe(true)
    expect((screen.getByPlaceholderText('assets/script/main.ks') as HTMLInputElement).disabled).toBe(true)
    expect((screen.getByPlaceholderText('line') as HTMLInputElement).disabled).toBe(true)
    expect(screen.getByText('Set Breakpoint').closest('button')?.disabled).toBe(true)
    expect((screen.getByPlaceholderText('variable name') as HTMLInputElement).disabled).toBe(true)
    expect(screen.getByText('Inspect').closest('button')?.disabled).toBe(true)
    expect(screen.getByText('Capture Frame').closest('button')?.disabled).toBe(true)
  })

  it('continues the debugger when paused', async () => {
    const client = makeClient({
      debugState: vi.fn(async () => ({ status: 'ok', scene: 'ep2', token_index: 3, paused: true })),
    })
    render(<DebugView client={client as unknown as EngineClient} />)
    await screen.findByText('paused')
    await waitFor(() => expect(screen.getByText('Continue').closest('button')?.disabled).toBe(false))
    fireEvent.click(screen.getByText('Continue'))
    await waitFor(() => expect(client.debugContinue).toHaveBeenCalled())
  })

  it('disables Continue while running and re-enables it on pause', async () => {
    const client = makeClient() // paused:false
    await renderConnected(client)
    await waitFor(() => expect(screen.getByText('Continue').closest('button')?.disabled).toBe(true))
    ;(client.debugState as ReturnType<typeof vi.fn>).mockResolvedValue({
      status: 'ok',
      scene: 'prologue',
      token_index: 12,
      paused: true,
    })
    fireEvent.click(screen.getByText('↻')) // force a re-poll
    await waitFor(() => expect(screen.getByText('Continue').closest('button')?.disabled).toBe(false))
  })

  it('inspects a variable and renders its type-tagged string value', async () => {
    const client = makeClient({
      inspect: vi.fn(async () => 'hello world'),
    })
    await renderConnected(client)
    fireEvent.change(screen.getByPlaceholderText('variable name'), { target: { value: 'msg' } })
    fireEvent.click(screen.getByText('Inspect'))
    await screen.findByText('hello world')
    expect(screen.getByText('string')).toBeTruthy()
    expect(screen.getByText('msg')).toBeTruthy()
    expect(client.inspect).toHaveBeenCalledWith('msg', 0, false)
  })

  it('renders numeric and table inspect results with their type tags', async () => {
    const client = makeClient({
      inspect: vi.fn(async (name: string) => (name === 'count' ? 3 : { hp: 10, mp: 5 })),
    })
    await renderConnected(client)
    fireEvent.change(screen.getByPlaceholderText('variable name'), { target: { value: 'count' } })
    fireEvent.click(screen.getByText('Inspect'))
    await screen.findByText('3')
    expect(screen.getByText('number')).toBeTruthy()

    fireEvent.change(screen.getByPlaceholderText('variable name'), { target: { value: 'stats' } })
    fireEvent.click(screen.getByText('Inspect'))
    await screen.findByText('{"hp":10,"mp":5}')
    expect(screen.getAllByText('table', { exact: false }).length).toBeGreaterThan(0)
  })

  it('surfaces an inspect rejection as an error line', async () => {
    const client = makeClient({
      inspect: vi.fn(async () => {
        throw new Error('Engine rejected inspect')
      }),
    })
    await renderConnected(client)
    fireEvent.change(screen.getByPlaceholderText('variable name'), { target: { value: 'secret' } })
    fireEvent.click(screen.getByText('Inspect'))
    await screen.findByText('Engine rejected inspect')
  })

  it('shows a validation message when inspect has no variable name', async () => {
    const client = makeClient()
    await renderConnected(client)
    fireEvent.click(screen.getByText('Inspect'))
    await screen.findByText('Enter a variable name to inspect')
    expect(client.inspect).not.toHaveBeenCalled()
  })

  it('disables the Inspect button while a request is pending', async () => {
    let release: (v: number) => void = () => {}
    const gate = new Promise<number>((res) => {
      release = res
    })
    const client = makeClient({
      inspect: vi.fn(() => gate),
    })
    await renderConnected(client)
    fireEvent.change(screen.getByPlaceholderText('variable name'), { target: { value: 'x' } })
    fireEvent.click(screen.getByText('Inspect'))
    const btn = await screen.findByText('Inspecting…')
    expect((btn as HTMLButtonElement).disabled).toBe(true)
    release(7)
    await screen.findByText('7')
    expect(screen.getByText('Inspect').closest('button')?.disabled).toBe(false)
  })

  it('captures a frame and renders the png with a size summary', async () => {
    const client = makeClient()
    await renderConnected(client)
    fireEvent.click(screen.getByText('Capture Frame'))
    const img = await screen.findByAltText('debug frame') as HTMLImageElement
    expect(img.src).toContain('data:image/png;base64,aGVsbG8=')
    expect(screen.getByText('640×360')).toBeTruthy()
    expect(client.frame).toHaveBeenCalledWith(640, 360)
  })

  it('shows a placeholder when capture returns no pixels', async () => {
    const client = makeClient({
      frame: vi.fn(async () => ({ status: 'ok', width: 640, height: 360 })),
    })
    await renderConnected(client)
    fireEvent.click(screen.getByText('Capture Frame'))
    await screen.findByText(/no pixels/)
  })

  it('shows an empty-frame hint before any capture', async () => {
    await renderConnected(makeClient())
    expect(screen.getByText('No frame captured yet')).toBeTruthy()
  })

  it('shows a capture failure error', async () => {
    const client = makeClient({
      frame: vi.fn(async () => {
        throw new Error('frame backend unavailable')
      }),
    })
    await renderConnected(client)
    fireEvent.click(screen.getByText('Capture Frame'))
    await screen.findByText('frame backend unavailable')
  })

  it('surfaces a frame-level error reply field', async () => {
    const client = makeClient({
      frame: vi.fn(async () => ({ status: 'error', error: 'renderer busy' })),
    })
    await renderConnected(client)
    fireEvent.click(screen.getByText('Capture Frame'))
    await screen.findByText('renderer busy')
  })
})