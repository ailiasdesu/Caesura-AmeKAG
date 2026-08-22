// @vitest-environment jsdom
import { describe, it, expect, beforeEach, vi } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { ProjectManagerView, storyPathFor, isValidProjectName } from './ProjectManagerView'
import { useEditor } from '../store'
import type { EngineClient, ProjectTemplate } from '../lib/rpc'

const TEMPLATES: ProjectTemplate[] = [
  { id: 'basic', name: 'basic', description: 'Minimal', dir: 'basic' },
  { id: 'showcase', name: 'showcase', description: 'Showcase', dir: 'showcase' },
]

/** Default metadata payload used by the projectMeta mock. */
const DEMO_META = {
  name: 'demo',
  template: '',
  version: '1.0',
  language: 'zh',
  description: '',
  created: '2026-01-01T00:00:00Z',
  modified: '2026-01-01T00:00:00Z',
}

function makeClient(over: Partial<EngineClient> = {}): EngineClient {
  return {
    projectCreate: vi.fn(async () => ({ ok: true, path: 'projects/demo' })),
    projectDuplicate: vi.fn(async () => ({ ok: true, path: 'projects/demo_copy' })),
    projectImport: vi.fn(async () => ({ ok: true, path: 'projects/imported' })),
    projectList: vi.fn(async () => []),
    projectTemplates: vi.fn(async () => []),
    projectMeta: vi.fn(async () => ({
      ok: true,
      inferred: true,
      meta: { ...DEMO_META },
    })),
    projectSaveMeta: vi.fn(async () => ({ ok: true, meta: { ...DEMO_META } })),
    inspect: vi.fn(async () => ''),
    ...over,
  } as unknown as EngineClient
}

beforeEach(() => {
  cleanup()
  useEditor.setState({
    client: null,
    projects: [],
    templates: [],
    recentProjects: [],
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

describe('storyPathFor (pure)', () => {
  it('appends story.ks to the project directory', () => {
    expect(storyPathFor('projects/demo')).toBe('projects/demo/story.ks')
    expect(storyPathFor('projects/demo/')).toBe('projects/demo/story.ks')
  })
})

describe('isValidProjectName (pure)', () => {
  it('accepts letters, digits, _ and -', () => {
    expect(isValidProjectName('demo')).toBe(true)
    expect(isValidProjectName('my_game-2')).toBe(true)
  })
  it('rejects spaces, separators and empty strings', () => {
    expect(isValidProjectName('bad name')).toBe(false)
    expect(isValidProjectName('a/b')).toBe(false)
    expect(isValidProjectName('../escape')).toBe(false)
    expect(isValidProjectName('')).toBe(false)
  })
})

describe('ProjectManagerView (component)', () => {
  it('renders the template list from the store', () => {
    useEditor.setState({ templates: TEMPLATES })
    render(<ProjectManagerView client={makeClient()} />)
    // "basic"/"showcase" appear both in the template dropdown and the list.
    expect(screen.getAllByText('basic').length).toBeGreaterThan(0)
    expect(screen.getAllByText('showcase').length).toBeGreaterThan(0)
  })

  it('renders existing projects with Open/Duplicate controls', () => {
    useEditor.setState({
      projects: [
        { path: 'projects/demo', name: 'demo', template: 'basic', modified: '1' },
      ],
    })
    render(<ProjectManagerView client={makeClient()} />)
    expect(screen.getByText('demo')).toBeTruthy()
    expect(screen.getAllByRole('button').some((b) => b.textContent === 'Open')).toBe(true)
    expect(screen.getAllByRole('button').some((b) => b.textContent === 'Duplicate')).toBe(true)
  })

  it('renders recent projects and opens them on click', async () => {
    const client = makeClient()
    useEditor.setState({ recentProjects: [{ path: 'projects/recent', name: 'recent', openedAt: 1 }] })
    render(<ProjectManagerView client={client} />)
    const openButtons = screen.getAllByRole('button').filter((b) => b.textContent === 'Open')
    expect(openButtons.length).toBeGreaterThan(0)
    fireEvent.click(openButtons[0])
    // openProject awaits readStory (a microtask) before opening the doc — flush it.
    await new Promise((r) => setTimeout(r, 0))
    // Recent open → the project is recorded into recent history and a doc is opened.
    expect(useEditor.getState().recentProjects[0].path).toBe('projects/recent')
    expect(useEditor.getState().docs.some((d) => d.path === 'projects/recent/story.ks')).toBe(true)
  })

  it('create with an invalid name shows a validation error and does not call RPC', () => {
    const client = makeClient()
    render(<ProjectManagerView client={client} />)
    const input = screen.getByPlaceholderText('Project name')
    fireEvent.change(input, { target: { value: 'bad name!' } })
    fireEvent.click(screen.getAllByRole('button').find((b) => b.textContent === 'Create Project')!)
    expect(screen.getByText(/may only contain/i)).toBeTruthy()
    expect(client.projectCreate).not.toHaveBeenCalled()
  })

  it('import flow calls projectImport with srcPath/name and confirms', async () => {
    const client = makeClient({
      projectImport: vi.fn(async () => ({ ok: true, path: 'projects\\imported' })),
    })
    render(<ProjectManagerView client={client} />)
    fireEvent.change(screen.getByPlaceholderText('Source folder path'), {
      target: { value: '/tmp/mygame' },
    })
    fireEvent.change(screen.getByPlaceholderText('Import as name'), {
      target: { value: 'imported' },
    })
    fireEvent.click(
      screen.getAllByRole('button').find((b) => b.textContent === 'Import Project')!,
    )
    // handleImport awaits the RPC before showing feedback — flush microtasks.
    await new Promise((r) => setTimeout(r, 0))
    expect(client.projectImport).toHaveBeenCalledWith('/tmp/mygame', 'imported')
    expect(screen.getByText(/Imported imported/i)).toBeTruthy()
    // Imported project lands in recent history with normalized separators.
    expect(useEditor.getState().recentProjects[0].path).toBe('projects/imported')
  })

  it('import with an invalid name shows a validation error and does not call RPC', () => {
    const client = makeClient()
    render(<ProjectManagerView client={client} />)
    fireEvent.change(screen.getByPlaceholderText('Source folder path'), {
      target: { value: '/tmp/mygame' },
    })
    fireEvent.change(screen.getByPlaceholderText('Import as name'), {
      target: { value: 'bad name!' },
    })
    fireEvent.click(
      screen.getAllByRole('button').find((b) => b.textContent === 'Import Project')!,
    )
    expect(screen.getByText(/may only contain/i)).toBeTruthy()
    expect(client.projectImport).not.toHaveBeenCalled()
  })

  // -- Project Settings (§6.3): load / edit / save metadata -------------

  it('settings: clicking Settings loads project meta into the form', async () => {
    const client = makeClient({
      projectMeta: vi.fn(async () => ({
        ok: true,
        inferred: false,
        meta: {
          ...DEMO_META,
          language: 'ja',
          description: 'hello world',
        },
      })),
    })
    useEditor.setState({
      projects: [{ path: 'projects/demo', name: 'demo', template: 'basic' }],
    })
    render(<ProjectManagerView client={client} />)
    fireEvent.click(
      screen.getAllByRole('button').find((b) => b.textContent === 'Settings')!,
    )
    await new Promise((r) => setTimeout(r, 0))
    expect(client.projectMeta).toHaveBeenCalledWith('projects/demo')
    const langSelect = screen.getByLabelText('Project language') as HTMLSelectElement
    expect(langSelect.value).toBe('ja')
    const desc = screen.getByLabelText('Project description') as HTMLTextAreaElement
    expect(desc.value).toBe('hello world')
    expect(screen.getByText('PROJECT SETTINGS')).toBeTruthy()
  })

  it('settings: Save posts language/description and confirms', async () => {
    const client = makeClient()
    useEditor.setState({
      projects: [{ path: 'projects/demo', name: 'demo', template: 'basic' }],
    })
    render(<ProjectManagerView client={client} />)
    fireEvent.click(
      screen.getAllByRole('button').find((b) => b.textContent === 'Settings')!,
    )
    await new Promise((r) => setTimeout(r, 0))
    fireEvent.change(screen.getByLabelText('Project description'), {
      target: { value: 'A tiny story' },
    })
    fireEvent.change(screen.getByLabelText('Project language'), {
      target: { value: 'en' },
    })
    fireEvent.click(
      screen.getAllByRole('button').find((b) => b.textContent === 'Save Settings')!,
    )
    await new Promise((r) => setTimeout(r, 0))
    expect(client.projectSaveMeta).toHaveBeenCalledWith('projects/demo', {
      language: 'en',
      description: 'A tiny story',
    })
    expect(screen.getByText(/Saved settings for demo/i)).toBeTruthy()
  })

  it('settings: failed save surfaces the backend error', async () => {
    const client = makeClient({
      projectSaveMeta: vi.fn(async () => ({
        ok: false,
        error: 'disk full',
      })),
    })
    useEditor.setState({
      projects: [{ path: 'projects/demo', name: 'demo', template: 'basic' }],
    })
    render(<ProjectManagerView client={client} />)
    fireEvent.click(
      screen.getAllByRole('button').find((b) => b.textContent === 'Settings')!,
    )
    await new Promise((r) => setTimeout(r, 0))
    fireEvent.click(
      screen.getAllByRole('button').find((b) => b.textContent === 'Save Settings')!,
    )
    await new Promise((r) => setTimeout(r, 0))
    expect(screen.getByText(/disk full/i)).toBeTruthy()
  })
})
