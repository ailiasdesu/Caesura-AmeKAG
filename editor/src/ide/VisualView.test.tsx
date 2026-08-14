// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { VisualView } from './VisualView'
import type { EngineClient } from '../lib/rpc'
import { useEditor } from '../store'

// SmaSkeletonCanvas uses canvas 2D — not available in jsdom; stub it.
vi.mock('./SmaSkeletonCanvas', () => ({
  SmaSkeletonCanvas: () => <div data-testid="sma-canvas" />,
}))

type Client = Pick<
  EngineClient,
  | 'frame' | 'state' | 'stats' | 'smaValidate' | 'smaSave'
  | 'evalRaw' | 'live2dModels' | 'live2dLoad' | 'pick'
>

const OK_JSON = JSON.stringify({
  name: 'hero',
  bones: 3,
  anims: ['idle', 'walk'],
  parts: 2,
  verts: 120,
  tris: 180,
  boneTree: [
    { id: 0, parent: -1 },
    { id: 1, parent: 0 },
    { id: 2, parent: 1 },
  ],
  animDetails: [
    { name: 'idle', duration: 2.0, tracks: [0, 1] },
    { name: 'walk', duration: 1.2, tracks: [0, 1, 2] },
  ],
})

const makeClient = (overrides: Partial<Client> = {}): Client => ({
  frame: vi.fn(async () => ({ status: 'ok', png: 'AAAA' })),
  state: vi.fn(async () => ({ status: 'ok', scene: 'prologue', token_index: 5, nvl_mode: true, language: 'zh', backlog_count: 7, layer_count: 6 })),
  stats: vi.fn(async () => ({ status: 'ok', texture_budget_mb: 128, texture_tier_name: 'tier2', mesh_count: 42, job_workers: 2, job_pending: 1, lua_kb: 256 })),
  smaValidate: vi.fn(async () => ({ status: 'ok', ok: true, errors: [], meta: OK_JSON })),
  smaSave: vi.fn(async () => ({ status: 'ok', ok: true, errors: [] })),
  evalRaw: vi.fn(async () => ''),
  live2dModels: vi.fn(async () => []),
  live2dLoad: vi.fn(async () => ({ status: 'ok', modelId: 7 })),
  pick: vi.fn(async () => ({ status: 'ok', hits: '[]' })),
  ...overrides,
})

beforeEach(() => {
  cleanup()
  useEditor.setState({
    docs: [],
    activePath: null,
    sideView: 'visual',
    engineConnected: false,
    engineScene: '',
    engineToken: 0,
    enginePaused: false,
    revealRequest: null,
  })
})

describe('VisualView (component)', () => {
  it('renders the captured frame as a data-URL image', async () => {
    render(<VisualView client={makeClient() as unknown as EngineClient} />)
    await screen.findByAltText('Engine frame')
    const img = document.querySelector('img.frame-img') as HTMLImageElement
    expect(img.src).toContain('data:image/png;base64,AAAA')
  })

  it('shows the error message when frame capture fails', async () => {
    const client = makeClient({
      frame: vi.fn(async () => ({ status: 'error', error: 'GPU busy' })),
    })
    render(<VisualView client={client as unknown as EngineClient} />)
    await screen.findByText('GPU busy')
    expect(screen.queryByAltText('Engine frame')).toBeNull()
  })

  it('renders engine state rows', async () => {
    render(<VisualView client={makeClient() as unknown as EngineClient} />)
    await screen.findByText('prologue')
    expect(screen.getByText('5')).toBeTruthy()
    expect(screen.getByText('zh')).toBeTruthy()
    expect(screen.getByText('on')).toBeTruthy() // nvl
    expect(screen.getByText('7')).toBeTruthy() // backlog
    expect(screen.getByText('6')).toBeTruthy() // layers
  })

  it('renders engine stats rows with tier label and units', async () => {
    render(<VisualView client={makeClient() as unknown as EngineClient} />)
    await screen.findByText(/tier2/)
    expect(screen.getByText('128 MB · tier2')).toBeTruthy()
    expect(screen.getByText('256 KB')).toBeTruthy()
    expect(screen.getByText('42')).toBeTruthy()
  })

  it('validates an SMA asset and renders meta + skeleton tree + animations', async () => {
    render(<VisualView client={makeClient() as unknown as EngineClient} />)
    await screen.findByText('Validate')
    fireEvent.click(screen.getByText('Validate'))
    await screen.findByText('✓ valid')
    expect(screen.getByText('bones')).toBeTruthy()
    expect(screen.getByText('3')).toBeTruthy()
    expect(screen.getByText('idle, walk')).toBeTruthy()
    expect(screen.getByText('120/180')).toBeTruthy()
    await screen.findByText('Skeleton')
    expect(screen.getByText('◇ bone 0')).toBeTruthy()
    expect(screen.getAllByText(/1 child/)).toHaveLength(2) // bones 0 and 1 each have one child
    await screen.findByText('Animations')
    expect(screen.getByText('idle (2s)')).toBeTruthy()
    expect(screen.getByText('tracks: 0, 1')).toBeTruthy()
  })

  it('shows SMA validation errors when invalid', async () => {
    const client = makeClient({
      smaValidate: vi.fn(async () => ({ status: 'ok', ok: false, errors: ['bone 3 missing parent', 'verts mismatch'], meta: '' })),
    })
    render(<VisualView client={client as unknown as EngineClient} />)
    fireEvent.click(await screen.findByText('Validate'))
    await screen.findByText('✗ invalid')
    expect(screen.getByText('bone 3 missing parent')).toBeTruthy()
    expect(screen.getByText('verts mismatch')).toBeTruthy()
  })

  it('loads a draft via evalRaw and shows the success message', async () => {
    const client = makeClient({
      evalRaw: vi.fn(async () => JSON.stringify(OK_JSON)),
    })
    render(<VisualView client={client as unknown as EngineClient} />)
    fireEvent.click(await screen.findByText('Load for editing'))
    await screen.findByText(/Loaded demo\/assets\/sma\/hero\.json from the engine/)
    const ta = document.querySelector('textarea.sma-editor') as HTMLTextAreaElement
    expect(JSON.parse(ta.value).name).toBe('hero')
  })

  it('falls back to the sample draft when the engine read fails', async () => {
    const client = makeClient({
      evalRaw: vi.fn(async () => {
        throw new Error('sandbox denied')
      }),
    })
    render(<VisualView client={client as unknown as EngineClient} />)
    fireEvent.click(await screen.findByText('Load for editing'))
    await screen.findByText(/editing a sample draft/)
    const ta = document.querySelector('textarea.sma-editor') as HTMLTextAreaElement
    expect(ta.value).toContain('"hero"')
  })

  it('rejects saving invalid JSON locally without hitting the engine', async () => {
    const client = makeClient()
    render(<VisualView client={client as unknown as EngineClient} />)
    const ta = document.querySelector('textarea.sma-editor') as HTMLTextAreaElement
    fireEvent.change(ta, { target: { value: '{ broken' } })
    fireEvent.click(screen.getByText('Save'))
    await screen.findByText(/Invalid JSON/)
    expect(client.smaSave).not.toHaveBeenCalled()
  })

  it('saves a valid draft via smaSave', async () => {
    const client = makeClient({
      evalRaw: vi.fn(async () => JSON.stringify(OK_JSON)),
    })
    render(<VisualView client={client as unknown as EngineClient} />)
    fireEvent.click(await screen.findByText('Load for editing'))
    await screen.findByText(/Loaded/)
    fireEvent.click(screen.getByText('Save'))
    await screen.findByText('✓ saved')
    expect(client.smaSave).toHaveBeenCalledWith('demo/assets/sma/hero.json', expect.stringContaining('"hero"'))
  })

  it('lists Live2D models and loads the selected one', async () => {
    const client = makeClient({
      live2dModels: vi.fn(async () => [
        { path: 'models/hero.model3.json', name: 'hero' },
        { path: 'models/maid.model3.json', name: 'maid' },
      ]),
    })
    render(<VisualView client={client as unknown as EngineClient} />)
    await screen.findByRole('option', { name: 'hero' })
    const sel = document.querySelector('select.model-select') as HTMLSelectElement
    fireEvent.change(sel, { target: { value: 'models/maid.model3.json' } })
    fireEvent.click(screen.getByText('Load'))
    await screen.findByText('Model loaded (id 7)')
    expect(client.live2dLoad).toHaveBeenCalledWith('models/maid.model3.json')
  })

  it('reports pick hits from a frame click', async () => {
    const client = makeClient({
      pick: vi.fn(async () => ({ status: 'ok', hits: JSON.stringify([{ id: 'layer1', name: 'bg', z: 0, depth: 1, opacity: 1, x: 10, y: 20, w: 100, h: 50 }]) })),
    })
    render(<VisualView client={client as unknown as EngineClient} />)
    const img = (await screen.findByAltText('Engine frame')) as HTMLImageElement
    Object.defineProperty(img, 'getBoundingClientRect', { value: () => ({ left: 0, top: 0, width: 640, height: 360 }) })
    fireEvent.click(img, { clientX: 320, clientY: 180 })
    await screen.findByText('bg (z=0)')
    expect(screen.getByText('x10,y20 100×50')).toBeTruthy()
    expect(client.pick).toHaveBeenCalledWith(640, 360)
  })
})
