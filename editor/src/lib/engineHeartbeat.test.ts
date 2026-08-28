// @vitest-environment jsdom
// t45: useEngineHeartbeat — vi.useFakeTimers driven (NO real timer waits; a
// real-interval test would hang vitest or flake on wall-clock drift).
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
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

  it('clears engineConnected on ping failure and keeps it false on later beats', async () => {
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
    await vi.advanceTimersByTimeAsync(10000)
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
