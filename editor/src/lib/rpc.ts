// Caesura Editor — engine HTTP RPC client.
// Mirrors the 18 routes of src/rpc/EditorServer.cpp (docs/api/editor-api-reference.md).
// All calls go through /api/* (Vite dev proxy → localhost:9876, or same-origin
// in production). Every request carries the optional bearer token from the
// engine's CAESURA_EDITOR_TOKEN env var (set via the connection panel).

export interface PingReply {
  status: string
  engine: string
}

export interface StatusReply {
  status: string
  engine: string
  lua: boolean
  port: number
}

export interface AssetEntry {
  path: string
  name: string
  type: string
}

export interface LogEntry {
  level: 'info' | 'warn' | 'error'
  message: string
  time: string
}

export interface DebugStateReply {
  status: string
  scene?: string
  paused?: boolean
  token_index?: number
  [key: string]: unknown
}

export interface BreakpointSpec {
  source: string
  line: number
}

export interface FrameReply {
  status: string
  width?: number
  height?: number
  png?: string // base64
  error?: string
}

export interface BuildReply {
  status: string
  output?: string
  error?: string
}

export interface Live2DModel {
  path: string
  name: string
}

export interface ApiError {
  status: string
  code?: string
  message?: string
  error?: string
}

export class RpcError extends Error {
  constructor(
    message: string,
    public readonly status?: number,
    public readonly body?: unknown,
  ) {
    super(message)
    this.name = 'RpcError'
  }
}

export class EngineClient {
  private token = ''

  constructor(
    private base = '/api',
    private fetchImpl: typeof fetch = fetch,
  ) {}

  setToken(token: string) {
    this.token = token
  }

  /** Configure a different engine base URL (production: same origin). */
  setBase(base: string) {
    this.base = base
  }

  private async request<T>(path: string, init?: RequestInit): Promise<T> {
    const headers: Record<string, string> = {
      Accept: 'application/json',
    }
    if (this.token) headers['Authorization'] = `Bearer ${this.token}`
    if (init?.body) headers['Content-Type'] = 'application/json'

    const res = await this.fetchImpl(this.base + path, { ...init, headers })
    if (!res.ok) {
      let body: unknown = null
      try {
        body = await res.json()
      } catch {
        body = await res.text().catch(() => null)
      }
      throw new RpcError(`HTTP ${res.status} on ${path}`, res.status, body)
    }
    return (await res.json()) as T
  }

  // -- engine control -----------------------------------------------------

  ping(): Promise<PingReply> {
    return this.request<PingReply>('/ping')
  }

  status(): Promise<StatusReply> {
    return this.request<StatusReply>('/status')
  }

  run(script: string): Promise<{ status: string }> {
    return this.request<{ status: string }>('/run', {
      method: 'POST',
      body: JSON.stringify({ script }),
    })
  }

  stop(): Promise<{ status: string }> {
    return this.request<{ status: string }>('/stop', { method: 'POST' })
  }

  reload(): Promise<{ status: string }> {
    return this.request<{ status: string }>('/reload', { method: 'POST' })
  }

  // -- introspection ------------------------------------------------------

  assets(type?: 'image' | 'audio' | 'script'): Promise<AssetEntry[]> {
    const q = type ? `?type=${type}` : ''
    return this.request<AssetEntry[]>(`/assets${q}`)
  }

  logs(): Promise<LogEntry[]> {
    return this.request<LogEntry[]>('/logs')
  }

  live2dModels(): Promise<Live2DModel[]> {
    return this.request<Live2DModel[]>('/live2d/models')
  }

  build(outputPath?: string, keyPath?: string): Promise<BuildReply> {
    return this.request<BuildReply>('/build', {
      method: 'POST',
      body: JSON.stringify({ outputPath, keyPath }),
    })
  }

  // -- KAG scene debugger -------------------------------------------------

  debugState(): Promise<DebugStateReply> {
    return this.request<DebugStateReply>('/debug/getState')
  }

  frame(w = 640, h = 360): Promise<FrameReply> {
    return this.request<FrameReply>(`/debug/getFrame?w=${w}&h=${h}`)
  }

  setBreakpoint(source: string, line: number): Promise<{ status: string }> {
    return this.request<{ status: string }>('/debug/setBreakpoint', {
      method: 'POST',
      body: JSON.stringify({ source, line }),
    })
  }

  removeBreakpoint(source: string, line: number): Promise<{ status: string }> {
    return this.request<{ status: string }>('/debug/removeBreakpoint', {
      method: 'POST',
      body: JSON.stringify({ source, line }),
    })
  }

  clearBreakpoints(): Promise<{ status: string }> {
    return this.request<{ status: string }>('/debug/clearBreakpoints', {
      method: 'POST',
    })
  }

  debugContinue(): Promise<{ status: string }> {
    return this.request<{ status: string }>('/debug/continue', {
      method: 'POST',
    })
  }

  inspect(name: string, frame = 0, global = false): Promise<unknown> {
    const g = global ? '&global=1' : ''
    return this.request<unknown>(
      `/debug/inspect?name=${encodeURIComponent(name)}&frame=${frame}${g}`,
    )
  }

  // -- Live2D -------------------------------------------------------------

  live2dLoad(path: string): Promise<{ status: string; modelId?: number }> {
    return this.request<{ status: string; modelId?: number }>('/live2d/load', {
      method: 'POST',
      body: JSON.stringify({ path }),
    })
  }
}
