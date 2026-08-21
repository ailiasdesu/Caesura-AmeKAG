// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup, waitFor } from '@testing-library/react'
import { ExplorerView } from './ExplorerView'
import type { EngineClient, AssetEntry } from '../lib/rpc'
import { useEditor, type OpenDoc } from '../store'

type Client = Pick<EngineClient, 'assets' | 'inspect'>
const makeClient = (overrides: Partial<Client> = {}): Client => ({
  assets: vi.fn(async () => [] as AssetEntry[]),
  inspect: vi.fn(async () => ''),
  ...overrides,
})

const ASSETS: AssetEntry[] = [
  { path: 'assets/script/main.ks', name: 'main.ks', type: 'script' },
  { path: 'assets/script/start.ks', name: 'start.ks', type: 'script' },
  { path: 'assets/image/room.png', name: 'room.png', type: 'image' },
  { path: 'assets/audio/bgm.ogg', name: 'bgm.ogg', type: 'audio' },
]

beforeEach(() => {
  cleanup()
  useEditor.setState({
    docs: [],
    activePath: null,
    sideView: 'explorer',
    engineConnected: false,
    engineScene: '',
    engineToken: 0,
    enginePaused: false,
    revealRequest: null,
  })
})

describe('ExplorerView (component)', () => {
  it('lists assets grouped by type', async () => {
    const client = makeClient({ assets: vi.fn(async () => ASSETS) })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('main.ks')
    expect(screen.getByText('room.png')).toBeTruthy()
    expect(screen.getByText('bgm.ogg')).toBeTruthy()
    expect(screen.getByText('SCRIPTS (.ks)')).toBeTruthy()
    expect(screen.getByText('IMAGES')).toBeTruthy()
    expect(screen.getByText('AUDIO')).toBeTruthy()
  })

  it('shows empty hints per group when no assets of that type exist', async () => {
    const client = makeClient({
      assets: vi.fn(async () => ASSETS.filter((a) => a.type === 'image')),
    })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('room.png')
    expect(screen.getByText('No scripts found')).toBeTruthy()
    expect(screen.getByText('No audio found')).toBeTruthy()
    expect(screen.queryByText('No images found')).toBeNull()
  })

  it('filters assets by name (case-insensitive substring)', async () => {
    const client = makeClient({ assets: vi.fn(async () => ASSETS) })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('main.ks')
    fireEvent.change(screen.getByPlaceholderText('Filter assets…'), {
      target: { value: 'START' },
    })
    expect(screen.getByText('start.ks')).toBeTruthy()
    expect(screen.queryByText('main.ks')).toBeNull()
    expect(screen.queryByText('room.png')).toBeNull()
  })

  it('shows the fetch error when the engine is unreachable', async () => {
    const client = makeClient({
      assets: vi.fn(async () => {
        throw new Error('fetch failed')
      }),
    })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('fetch failed')
  })

  it('opens a script doc (kag) on double-click, fetching content via inspect', async () => {
    const client = makeClient({
      assets: vi.fn(async () => ASSETS),
      inspect: vi.fn(async () => '*start\n[bg storage="room.png"]'),
    })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    const item = await screen.findByText('main.ks')
    fireEvent.doubleClick(item)
    await waitFor(() => {
      const s = useEditor.getState()
      expect(s.docs.some((d) => d.path === 'assets/script/main.ks' && d.language === 'kag')).toBe(true)
    })
    const doc = useEditor.getState().docs[0]
    expect(doc?.content).toContain('[bg storage="room.png"]')
    expect(client.inspect).toHaveBeenCalledWith('__editor_read_script("assets/script/main.ks")')
  })

  it('opens a script doc with empty content when inspect fails', async () => {
    const client = makeClient({
      assets: vi.fn(async () => ASSETS),
      inspect: vi.fn(async () => {
        throw new Error('sandbox denied')
      }),
    })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    const item = await screen.findByText('main.ks')
    fireEvent.doubleClick(item)
    await waitFor(() => {
      expect(useEditor.getState().docs.some((d) => d.path === 'assets/script/main.ks')).toBe(true)
    })
    expect(useEditor.getState().docs[0]?.content).toBe('')
  })

  it('opens an image doc with a plaintext placeholder on double-click', async () => {
    const client = makeClient({ assets: vi.fn(async () => ASSETS) })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    const item = await screen.findByText('room.png')
    fireEvent.doubleClick(item)
    await waitFor(() => {
      const s = useEditor.getState()
      expect(s.docs.some((d) => d.path === 'assets/image/room.png' && d.language === 'plaintext')).toBe(true)
    })
  })

  it('renders open editors with dirty dots; click activates, close removes', async () => {
    const client = makeClient({ assets: vi.fn(async () => ASSETS) })
    const dirtyDoc: OpenDoc = {
      path: 'assets/script/wip.ks',
      name: 'wip.ks',
      language: 'kag',
      content: '*start',
      dirty: true,
    }
    useEditor.setState({
      docs: [dirtyDoc],
      activePath: 'assets/script/wip.ks',
    })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('OPEN EDITORS')
    expect(screen.getByText('wip.ks')).toBeTruthy()
    expect(document.querySelector('.dirty-dot')).toBeTruthy()

    // close removes the doc
    fireEvent.click(document.querySelector('.explorer-close')!)
    await waitFor(() => {
      expect(useEditor.getState().docs).toHaveLength(0)
    })
  })

  it('open editors reflect active-path highlighting', async () => {
    const client = makeClient({ assets: vi.fn(async () => ASSETS) })
    useEditor.setState({
      docs: [
        { path: 'a.ks', name: 'a.ks', language: 'kag', content: '', dirty: false },
        { path: 'b.ks', name: 'b.ks', language: 'kag', content: '', dirty: false },
      ],
      activePath: 'b.ks',
    })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('OPEN EDITORS')
    const items = document.querySelectorAll('.explorer-item')
    expect(items[0].className).not.toContain('active')
    expect(items[1].className).toContain('active')
    // click switches active
    fireEvent.click(items[0])
    await waitFor(() => {
      expect(useEditor.getState().activePath).toBe('a.ks')
    })
  })
})
describe('ExplorerView type filters & asset affordances', () => {
  it('renders type filter buttons with All active by default', async () => {
    const client = makeClient({ assets: vi.fn(async () => ASSETS) })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('main.ks')
    const all = screen.getByRole('button', { name: 'All' })
    expect(all.getAttribute('aria-pressed')).toBe('true')
    expect(screen.getByRole('button', { name: 'Images' })).toBeTruthy()
    expect(screen.getByRole('button', { name: 'Audio' })).toBeTruthy()
    expect(screen.getByRole('button', { name: 'Scripts' })).toBeTruthy()
  })

  it('clicking Audio shows only audio assets and hides the others', async () => {
    const client = makeClient({ assets: vi.fn(async () => ASSETS) })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('main.ks')
    fireEvent.click(screen.getByRole('button', { name: 'Audio' }))
    expect(screen.getByText('bgm.ogg')).toBeTruthy()
    expect(screen.queryByText('main.ks')).toBeNull()
    expect(screen.queryByText('room.png')).toBeNull()
    expect(screen.getByRole('button', { name: 'Audio' }).getAttribute('aria-pressed')).toBe('true')
    expect(screen.getByRole('button', { name: 'All' }).getAttribute('aria-pressed')).toBe('false')
  })

  it('clicking Images shows only image assets', async () => {
    const client = makeClient({ assets: vi.fn(async () => ASSETS) })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('main.ks')
    fireEvent.click(screen.getByRole('button', { name: 'Images' }))
    expect(screen.getByText('room.png')).toBeTruthy()
    expect(screen.queryByText('main.ks')).toBeNull()
    expect(screen.queryByText('bgm.ogg')).toBeNull()
  })

  it('clicking Scripts shows only script assets', async () => {
    const client = makeClient({ assets: vi.fn(async () => ASSETS) })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('main.ks')
    fireEvent.click(screen.getByRole('button', { name: 'Scripts' }))
    expect(screen.getByText('main.ks')).toBeTruthy()
    expect(screen.getByText('start.ks')).toBeTruthy()
    expect(screen.queryByText('room.png')).toBeNull()
    expect(screen.queryByText('bgm.ogg')).toBeNull()
  })

  it('type filter stacks with the text filter', async () => {
    const client = makeClient({ assets: vi.fn(async () => ASSETS) })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('main.ks')
    fireEvent.click(screen.getByRole('button', { name: 'Audio' }))
    expect(screen.getByText('bgm.ogg')).toBeTruthy()
    // Now the text filter also applies on top of type selection
    fireEvent.change(screen.getByPlaceholderText('Filter assets…'), {
      target: { value: 'room' },
    })
    expect(screen.queryByText('bgm.ogg')).toBeNull()
    expect(screen.queryByText('room.png')).toBeNull()
  })

  it('renders a colored thumbnail placeholder on image assets with the kind label', async () => {
    const client = makeClient({
      assets: vi.fn(async () => [
        { path: 'assets/image/bg_01.png', name: 'bg_01.png', type: 'image', kind: 'bg' },
      ]),
    })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('bg_01.png')
    const thumb = document.querySelector('.explorer-thumb')
    expect(thumb).toBeTruthy()
    expect(thumb!.textContent).toBe('bg')
    expect((thumb as HTMLElement).style.background).toBeTruthy()
  })

  it('shows the unavailability hint when the audio preview fails', async () => {
    const client = makeClient({ assets: vi.fn(async () => ASSETS) })
    render(<ExplorerView client={client as unknown as EngineClient} />)
    await screen.findByText('bgm.ogg')
    fireEvent.click(document.querySelector('.explorer-audio-btn')!)
    await waitFor(() => {
      expect(document.body.textContent).toContain('不可用')
    })
  })
})
