// @vitest-environment jsdom
import { describe, it, expect, beforeEach, vi } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { ProjectManagerView, storyPathFor } from './ProjectManagerView'
import { useEditor } from '../store'
import type { EngineClient, ProjectTemplate } from '../lib/rpc'

const TEMPLATES: ProjectTemplate[] = [
  { id: 'basic', name: 'basic', description: 'Minimal', dir: 'basic' },
  { id: 'showcase', name: 'showcase', description: 'Showcase', dir: 'showcase' },
]

function makeClient(over: Partial<EngineClient> = {}): EngineClient {
  return {
    projectCreate: vi.fn(async () => ({ ok: true, path: 'projects/demo' })),
    projectDuplicate: vi.fn(async () => ({ ok: true, path: 'projects/demo_copy' })),
    projectList: vi.fn(async () => []),
    projectTemplates: vi.fn(async () => []),
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
})
