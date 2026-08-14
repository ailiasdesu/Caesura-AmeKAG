import { describe, it, expect, vi } from 'vitest'
import { EngineClient, RpcError } from './rpc'

/** Build a mock fetch returning a canned Response-like object. */
type FetchFn = (input: string | URL | Request, init?: RequestInit) => Promise<Response>

function mockFetch(status: number, body: unknown, _headers?: Record<string, string>) {
  return vi.fn<FetchFn>(async (_input: string | URL | Request, _init?: RequestInit) => {
    return {
      ok: status >= 200 && status < 300,
      status,
      json: async () => body,
      text: async () => JSON.stringify(body),
    } as Response
  })
}

describe('EngineClient', () => {
  it('GET request hits base + path with Accept header', async () => {
    const fetchMock = mockFetch(200, { status: 'ok' })
    const client = new EngineClient('/api', fetchMock)
    await client.ping()
    expect(fetchMock).toHaveBeenCalledWith(
      '/api/ping',
      expect.objectContaining({
        headers: expect.objectContaining({ Accept: 'application/json' }),
      }),
    )
  })

  it('includes Bearer token when set', async () => {
    const fetchMock = mockFetch(200, { status: 'ok' })
    const client = new EngineClient('/api', fetchMock)
    client.setToken('secret-token')
    await client.ping()
    const [, init] = fetchMock.mock.calls[0] as [string, RequestInit]
    expect((init.headers as Record<string, string>).Authorization).toBe(
      'Bearer secret-token',
    )
  })

  it('sets Content-Type when a body is sent', async () => {
    const fetchMock = mockFetch(200, { status: 'ok' })
    const client = new EngineClient('/api', fetchMock)
    await client.smaSave('demo/x.ks', 'hello')
    const [, init] = fetchMock.mock.calls[0] as [string, RequestInit]
    expect((init.headers as Record<string, string>)['Content-Type']).toBe(
      'application/json',
    )
    // body is the serialized {path, content}
    expect(JSON.parse(init.body as string)).toEqual({
      path: 'demo/x.ks',
      content: 'hello',
    })
  })

  it('encodes query params in pick()', async () => {
    const fetchMock = mockFetch(200, { hits: '[]' })
    const client = new EngineClient('/api', fetchMock)
    await client.pick(320, 180)
    expect(fetchMock.mock.calls[0][0]).toBe('/api/pick?x=320&y=180')
  })

  it('parses JSON replies into typed objects', async () => {
    const fetchMock = mockFetch(200, { status: 'ok', engine: 'caesura', lua: true, port: 9876 })
    const client = new EngineClient('/api', fetchMock)
    const reply = await client.status()
    expect(reply).toEqual({ status: 'ok', engine: 'caesura', lua: true, port: 9876 })
  })

  it('throws RpcError with status and parsed body on HTTP error', async () => {
    const fetchMock = mockFetch(500, { status: 'error', message: 'boom' })
    const client = new EngineClient('/api', fetchMock)
    await expect(client.ping()).rejects.toMatchObject({
      name: 'RpcError',
      status: 500,
      body: { status: 'error', message: 'boom' },
    })
  })

  it('RpcError is an instanceof Error with HTTP detail in message', async () => {
    const fetchMock = mockFetch(404, { error: 'not found' })
    const client = new EngineClient('/api', fetchMock)
    try {
      await client.ping()
      expect.unreachable('should have thrown')
    } catch (e) {
      expect(e).toBeInstanceOf(RpcError)
      expect((e as Error).message).toContain('404')
      expect((e as Error).message).toContain('/ping')
    }
  })

  it('setBase redirects all requests', async () => {
    const fetchMock = mockFetch(200, { status: 'ok' })
    const client = new EngineClient('/api', fetchMock)
    client.setBase('/engine')
    await client.ping()
    expect(fetchMock.mock.calls[0][0]).toBe('/engine/ping')
  })
})