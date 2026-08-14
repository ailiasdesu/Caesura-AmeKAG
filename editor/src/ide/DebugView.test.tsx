// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup, waitFor } from '@testing-library/react'
import { DebugView } from './DebugView'
import type { EngineClient } from '../lib/rpc'
import { useEditor } from '../store'

type Client = Pick<
  EngineClient,
  'debugState' | 'run' | 'stop' | 'setBreakpoint' | 'clearBreakpoints' | 'debugContinue'
>

const makeClient = (overrides: Partial<Client> = {}): Client => ({
  debugState: vi.fn(async () => ({ status: 'ok', scene: 'prologue', token_index: 12, paused: false })),
  run: vi.fn(async () => ({ status: 'ok' } as { status: string })),
  stop: vi.fn(async () => ({ status: 'ok' })),
  setBreakpoint: vi.fn(async () => ({ status: 'ok' })),
  clearBreakpoints: vi.fn(async () => ({ status: 'ok' })),
  debugContinue: vi.fn(async () => ({ status: 'ok' })),
  ...overrides,
})

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
    render(<DebugView client={client as unknown as EngineClient} />)
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
    render(<DebugView client={client as unknown as EngineClient} />)
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
    render(<DebugView client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByText('Run'))
    const btn = await screen.findByText('Running…')
    expect((btn as HTMLButtonElement).disabled).toBe(true)
    release({ status: 'ok' })
    await screen.findByText('Script completed')
    expect(screen.getByText('Run').closest('button')?.disabled).toBe(false)
  })

  it('requests stop', async () => {
    const client = makeClient()
    render(<DebugView client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByText('Stop'))
    await screen.findByText('Stop requested')
    expect(client.stop).toHaveBeenCalled()
  })

  it('sets a breakpoint from the scene/line inputs', async () => {
    const client = makeClient()
    render(<DebugView client={client as unknown as EngineClient} />)
    fireEvent.change(screen.getByPlaceholderText('assets/script/main.ks'), {
      target: { value: 'assets/script/ch2.ks' },
    })
    fireEvent.change(screen.getByPlaceholderText('line'), {
      target: { value: '17' },
    })
    fireEvent.click(screen.getByText('Set Breakpoint'))
    await screen.findByText('Breakpoint: assets/script/ch2.ks:17')
    expect(client.setBreakpoint).toHaveBeenCalledWith('assets/script/ch2.ks', 17)
  })

  it('falls back to line 1 for a non-numeric breakpoint line', async () => {
    const client = makeClient()
    render(<DebugView client={client as unknown as EngineClient} />)
    fireEvent.change(screen.getByPlaceholderText('line'), {
      target: { value: 'abc' },
    })
    fireEvent.click(screen.getByText('Set Breakpoint'))
    await screen.findByText('Breakpoint: assets/script/main.ks:abc')
    expect(client.setBreakpoint).toHaveBeenCalledWith('assets/script/main.ks', 1)
  })

  it('clears all breakpoints', async () => {
    const client = makeClient()
    render(<DebugView client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByText('Clear All'))
    await screen.findByText('Breakpoints cleared')
    expect(client.clearBreakpoints).toHaveBeenCalled()
  })

  it('continues the debugger', async () => {
    const client = makeClient()
    render(<DebugView client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByText('Continue'))
    await waitFor(() => expect(client.debugContinue).toHaveBeenCalled())
  })
})
