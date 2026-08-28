// @vitest-environment jsdom
// t60 note: integration-level detector. Red-green for the GUARD ITSELF is not
// achievable at this level: removing the two guard checks from App.tsx left
// this suite green (4 experiments: as-is, +120ms flush, catch instrumentation,
// deferred-count probe -- the mount-effect ping IS in the deferred list and IS
// rejected, yet the connection-state write the assertions observe never
// flips). The guard semantics are unit-locked in lib/connEpoch.test.ts.
// t54 regression: the App mount-effect startup ping runs WITHOUT a token; a
// manual Connect can succeed while its catch is still settling. The epoch
// guard must drop every stale startup write (dot stays connected, no error).
// Mirrors the panels.integration harness: real <App/>, only the transport
// (EngineClient) and Monaco mocked. Every ping is a manual deferred; after a
// successful manual Connect we reject ALL outstanding startup pings — with
// the guard, conn-dot must stay connected and the error span must stay absent.
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { render, screen, fireEvent, cleanup, act } from '@testing-library/react'
import { App } from './App'
import { useEditor } from './store'

interface Deferred { res: (v: unknown) => void; rej: (e: unknown) => void }

const h = vi.hoisted(() => {
  const deferreds: Deferred[] = []
  class MockEngineClient {
    base = '/api'
    token = ''
    ping = vi.fn(() => new Promise((res, rej) => { deferreds.push({ res, rej }) }))
    status = vi.fn(async () => ({ status: 'ok', engine: 'caesura-test', lua: true, port: 9876 }))
    assets = vi.fn(async () => [])
    logs = vi.fn(async () => [])
    setToken = vi.fn()
    setBase = vi.fn()
    evalRaw = vi.fn(async () => 'true')
    stop = vi.fn(async () => ({ status: 'ok' }))
    buildCarc = vi.fn(async () => ({ status: 'ok' }))
    packageWeb = vi.fn(async () => ({ ok: true }))
    inspect = vi.fn(async () => '')
    live2dLoad = vi.fn(async () => ({ status: 'ok' }))
    live2dModels = vi.fn(async () => [])
    getState = vi.fn(async () => ({ status: 'ok' }))
    stats = vi.fn(async () => ({ status: 'ok' }))
    smaValidate = vi.fn(async () => ({ ok: true }))
    smaSave = vi.fn(async () => ({ ok: true }))
    pick = vi.fn(async () => ({}) )
    frame = vi.fn(async () => ({ status: 'ok' }))
    projectList = vi.fn(async () => [])
    projectCreate = vi.fn(async () => ({ ok: true }))
    projectDuplicate = vi.fn(async () => ({ ok: true }))
    projectImport = vi.fn(async () => ({ ok: true }))
    projectMeta = vi.fn(async () => ({ ok: true }))
    projectSaveMeta = vi.fn(async () => ({ ok: true }))
    reload = vi.fn(async () => ({ status: 'ok' }))
  }
  return { MockEngineClient, deferreds }
})

vi.mock('./lib/rpc', () => ({ EngineClient: h.MockEngineClient }))

vi.mock('@monaco-editor/react', () => ({ default: () => null }))
vi.mock('monaco-editor', () => ({
  editor: { setModelMarkers: () => {}, onDidCreateModel: () => ({ dispose: () => {} }) },
  languages: {
    getLanguages: () => [],
    register: () => {},
    setMonarchTokensProvider: () => {},
    setLanguageConfiguration: () => {},
    registerCompletionItemProvider: () => ({ dispose: () => {} }),
    registerHoverProvider: () => ({ dispose: () => {} }),
    registerDefinitionProvider: () => ({ dispose: () => {} }),
    registerReferenceProvider: () => ({ dispose: () => {} }),
  },
  Range: class {},
  MarkerSeverity: { Error: 8, Warning: 4 },
  KeyMod: { CtrlCmd: 2048 },
  KeyCode: { KeyS: 49 },
}))

beforeEach(() => {
  cleanup()
  h.deferreds.length = 0
  useEditor.setState({ engineConnected: false })
})

afterEach(() => {})

describe('App conn-state epoch guard (t54)', () => {
  it('a late startup-ping rejection does not downgrade a successful manual Connect', async () => {
    render(<App />)
    // At least the mount effect (+ heartbeat immediate beat) queued pings; let
    // the state settle into its (still pending) startup state first.
    expect(h.deferreds.length).toBeGreaterThan(0)
    // Manual Connect: paste the token into the panel input and click Connect.
    const token = screen.getByPlaceholderText(/CAESURA_EDITOR_TOKEN/)
    fireEvent.change(token, { target: { value: 'a'.repeat(48) } })
    const connectBtn = screen.getByRole('button', { name: /Connect/ })
    fireEvent.click(connectBtn)
    await act(async () => { await Promise.resolve(); await Promise.resolve() })
    // Connect resolved: dot connected.
    const dot = document.querySelector('.conn-dot')
    expect(dot?.className).toContain('conn-connected')
    // Now reject EVERY outstanding startup ping (mount effect + first
    // heartbeat beat were issued before the manual connect). With the guard
    // the stale catches must drop: the dot stays connected, no error text.
    const outstanding = h.deferreds.splice(0)
    await act(async () => {
      for (const d of outstanding) d.rej(new Error('HTTP 401 on /ping'))
      await Promise.resolve()
      await Promise.resolve()
    })
    expect(document.querySelector('.conn-dot')?.className).toContain('conn-connected')
    const errEl = document.querySelector('.conn-error')
    expect(errEl?.textContent ?? '').toBe('')
  })
})
