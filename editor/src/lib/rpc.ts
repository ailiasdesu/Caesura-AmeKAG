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
  /** Coarse category: "image" | "audio" | "script". */
  type: string
  /** Per-directory slot: "bg" | "fg" | "char" | "ui" | "bgm" | "voice" | "se" | "scripts". */
  kind?: string
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
  /** Current execution element, e.g. "[ch]", "*start", "text" (round 28). */
  current_cmd?: string
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

// Engine runtime state for the preview panel (GET /api/state, round 18).
export interface StateReply {
  status: string
  scene?: string
  token_index?: number
  nvl_mode?: boolean
  language?: string
  backlog_count?: number
  layer_count?: number
  /** Current execution element, e.g. "[ch]", "*start", "text" (round 28). */
  current_cmd?: string
  error?: string
}

// Engine stats (GET /api/stats). Render/asset/job/Lua introspection.
export interface StatsReply {
  status: string
  texture_budget_mb?: number
  texture_tier?: number
  texture_tier_name?: string
  mesh_count?: number
  job_workers?: number
  job_pending?: number
  lua_kb?: number
  error?: string
}

// Preview-frame hit test (GET /api/pick, round 23).
export interface PickReply {
  status: string
  hits: string // JSON array text: [{id,name,z,depth,opacity,x,y,w,h}]
  error?: string
}

export interface PickHit {
  id: string
  name: string
  z: number
  depth: number
  opacity: number
  x: number
  y: number
  w: number
  h: number
}

// SMA asset validation (GET /api/sma/validate, round 19).
export interface SmaValidateReply {
  status: string
  ok: boolean
  errors: string[]
  /** JSON object text: {bones, anims, parts, verts, tris} (parse client-side). */
  meta: string
  error?: string
}

export interface SmaMeta {
  bones: number
  anims: string[]
  parts: number
  verts: number
  tris: number
}
export interface SmaSaveReply {
  status: string
  ok: boolean
  errors: string[]
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

// A project template discoverable via GET /api/project/templates.
export interface ProjectTemplate {
  id: string
  name: string
  description: string
  /** Directory name under tools/project_templates (== id in practice). */
  dir: string
}

// A managed project under ./projects/ (GET /api/project/list).
export interface ProjectInfo {
  /** Directory path relative to the engine cwd, e.g. "projects/demo". */
  path: string
  name: string
  template: string
  /** File-system mtime as a raw integer string (opaque; for sorting). */
  modified?: string
}

// Shared reply for POST /api/project/create and /api/project/duplicate.
export interface ProjectOpReply {
  ok: boolean
  path?: string
  error?: string
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

  /** Engine runtime state (scene/token/language/backlog/layers). */
  state(): Promise<StateReply> {
    return this.request<StateReply>('/state')
  }

  /** Engine stats (texture budget/tier, meshes, job workers/pending, Lua heap). */
  stats(): Promise<StatsReply> {
    return this.request<StatsReply>('/stats')
  }

  /** Hit-test the preview frame at a window pixel (1280x720 space). */
  pick(x: number, y: number): Promise<PickReply> {
    return this.request<PickReply>(
      `/pick?x=${x}&y=${y}`,
    )
  }

  /** Validate an SMA asset through the engine's shared checker. */
  smaValidate(path: string): Promise<SmaValidateReply> {
    return this.request<SmaValidateReply>(
      `/sma/validate?path=${encodeURIComponent(path)}`,
    )
  }

  /** Save an SMA asset through the engine shared checker (round 26). */
  smaSave(path: string, content: string): Promise<SmaSaveReply> {
    return this.request<SmaSaveReply>('/sma/save', {
      method: 'POST',
      body: JSON.stringify({ path, content }),
    })
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

  /** Execute a Lua expression via /api/eval; returns the raw result
   *  string (the engine JSON-escapes it as "result":"..." — parse twice
   *  when the Lua side returns JSON). The engine reads the request body
   *  as raw Lua code (not JSON-wrapped). */
  async evalRaw(code: string): Promise<string> {
    const res = await this.fetchImpl(this.base + '/eval', {
      method: 'POST',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'text/plain',
        ...(this.token ? { Authorization: `Bearer ${this.token}` } : {}),
      },
      body: code,
    })
    if (!res.ok) {
      throw new RpcError(`HTTP ${res.status} on /eval`, res.status, null)
    }
    const body = (await res.json()) as { result?: string; error?: string }
    if (body.error) throw new RpcError(body.error, res.status, body)
    return body.result ?? ''
  }

  stop(): Promise<{ status: string }> {
    return this.request<{ status: string }>('/stop', { method: 'POST' })
  }

  reload(): Promise<{ status: string }> {
    return this.request<{ status: string }>('/reload', { method: 'POST' })
  }

  // -- introspection ------------------------------------------------------

  /**
   * List project assets. The optional filter accepts either the coarse
   * category ("image" | "audio" | "script") or a per-directory slot
   * ("bg" | "fg" | "bgm" | "se" | "voice" | ...) so Scene Builder can pull
   * exactly the assets for a slot it is painting.
   */
  assets(
    type?:
      | 'image'
      | 'audio'
      | 'script'
      | 'bg'
      | 'fg'
      | 'char'
      | 'ui'
      | 'bgm'
      | 'voice'
      | 'se'
      | 'scripts',
  ): Promise<AssetEntry[]> {
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

  /** Single-step controls (Sprint 4b): same contract as /continue. */
  debugStep(mode: 'into' | 'over' | 'out'): Promise<{ status: string }> {
    const cap = mode === 'into' ? 'Into' : mode === 'over' ? 'Over' : 'Out'
    return this.request<{ status: string }>(`/debug/step${cap}`, {
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

  // -- Project Manager ----------------------------------------------------

  /** List discoverable project templates (GET /api/project/templates). */
  projectTemplates(): Promise<ProjectTemplate[]> {
    return this.request<ProjectTemplate[]>('/project/templates')
  }

  /** List managed projects under ./projects/ (GET /api/project/list). */
  projectList(): Promise<ProjectInfo[]> {
    return this.request<ProjectInfo[]>('/project/list')
  }

  /** Create a new project from a template (POST /api/project/create). */
  projectCreate(template: string, name: string): Promise<ProjectOpReply> {
    return this.request<ProjectOpReply>('/project/create', {
      method: 'POST',
      body: JSON.stringify({ template, name }),
    })
  }

  /** Duplicate an existing source project under a new name
   *  (POST /api/project/duplicate). srcPath is the ProjectInfo.path. */
  projectDuplicate(srcPath: string, name: string): Promise<ProjectOpReply> {
    return this.request<ProjectOpReply>('/project/duplicate', {
      method: 'POST',
      body: JSON.stringify({ srcPath, name }),
    })
  }
}