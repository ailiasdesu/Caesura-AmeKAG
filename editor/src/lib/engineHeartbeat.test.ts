// @vitest-environment jsdom
// t45: useEngineHeartbeat — vi.useFakeTimers driven (NO real timer waits; a
// real-interval test would hang vitest or flake on wall-clock drift).
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { StrictMode, createElement } from 'react'
import { renderHook, cleanup } from '@testing-library/react'
import { useEngineHeartbeat } from './engineHeartbeat'
import { useEditor } from '../store'
import type { EngineClient } from './rpc'

function makeClient(over: Partial<EngineClient> = {}): EngineClient {
  return {
    ping: vi.fn(async () => ({ status: 'ok', engine: 'CaesuraAmeKAG' })),
    ...over,
  } as unknown as EngineClient
}

beforeEach(() => {
  cleanup()
  useEditor.setState({ engineConnected: false })
})

afterEach(() => {
  vi.useRealTimers()
})

describe('useEngineHeartbeat', () => {
  it('sets engineConnected true when ping succeeds (immediate first beat)', async () => {
    vi.useFakeTimers()
    const client = makeClient()
    renderHook(() => useEngineHeartbeat(client, 5000))
    await vi.advanceTimersByTimeAsync(0)
    expect(useEditor.getState().engineConnected).toBe(true)
    expect(client.ping).toHaveBeenCalledTimes(1)
  })

  it('clears engineConnected on ping failure and keeps polling on later beats', async () => {
    useEditor.setState({ engineConnected: true })
    vi.useFakeTimers()
    const client = makeClient({
      ping: vi.fn(async () => {
        throw new Error('no engine')
      }),
    })
    renderHook(() => useEngineHeartbeat(client, 5000))
    await vi.advanceTimersByTimeAsync(0)
    expect(useEditor.getState().engineConnected).toBe(false)
    const callsAtFailure = (client.ping as ReturnType<typeof vi.fn>).mock.calls.length
    await vi.advanceTimersByTimeAsync(10000)
    // state stays false BECAUSE live beats keep failing -- prove the interval
    // is actually still running (mock.calls grew), not a parked/stale timer.
    expect((client.ping as ReturnType<typeof vi.fn>).mock.calls.length)
      .toBeGreaterThan(callsAtFailure)
    expect(useEditor.getState().engineConnected).toBe(false)
  })

  it('stops polling after unmount (interval cleared)', async () => {
    vi.useFakeTimers()
    const client = makeClient()
    const { unmount } = renderHook(() => useEngineHeartbeat(client, 5000))
    await vi.advanceTimersByTimeAsync(0)
    const callsBefore = (client.ping as ReturnType<typeof vi.fn>).mock.calls.length
    unmount()
    await vi.advanceTimersByTimeAsync(20000)
    expect((client.ping as ReturnType<typeof vi.fn>).mock.calls.length).toBe(callsBefore)
  })

  it('StrictMode double mount leaves exactly one interval (cleanup+stopped)', async () => {
    vi.useFakeTimers()
    const client = makeClient()
    const { unmount } = renderHook(() => useEngineHeartbeat(client, 5000), {
      wrapper: ({ children }) => createElement(StrictMode, null, children),
    })
    await vi.advanceTimersByTimeAsync(0)
    // StrictMode (dev) = mount -> cleanup -> mount: two immediate beats.
    // If the cleanup failed to clear the first timer, the double mount would
    // leave TWO intervals running and the pacing below doubles.
    const afterMount = (client.ping as ReturnType<typeof vi.fn>).mock.calls.length
    expect(afterMount).toBe(2)
    await vi.advanceTimersByTimeAsync(10000)
    const afterTen = (client.ping as ReturnType<typeof vi.fn>).mock.calls.length
    // Single surviving timer over two 5s periods = exactly +2 beats;
    // a leaked second timer would add +2 more -> test goes red.
    expect(afterTen - afterMount).toBe(2)
    unmount()
    await vi.advanceTimersByTimeAsync(20000)
    expect((client.ping as ReturnType<typeof vi.fn>).mock.calls.length).toBe(afterTen)
  })

  it('recovers: connected -> failure -> connected across beats (badge roundtrip)', async () => {
    vi.useFakeTimers()
    let fail = false
    const client = makeClient({
      ping: vi.fn(async () => {
        if (fail) throw new Error('engine down')
        return { status: 'ok', engine: 'CaesuraAmeKAG' }
      }),
    })
    renderHook(() => useEngineHeartbeat(client, 5000))
    await vi.advanceTimersByTimeAsync(0)
    expect(useEditor.getState().engineConnected).toBe(true)
    fail = true
    await vi.advanceTimersByTimeAsync(5000)
    expect(useEditor.getState().engineConnected).toBe(false)
    fail = false
    await vi.advanceTimersByTimeAsync(5000)
    expect(useEditor.getState().engineConnected).toBe(true)
  })
})
