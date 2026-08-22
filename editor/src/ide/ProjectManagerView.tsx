import { useCallback, useEffect, useState } from 'react'
import type { EngineClient } from '../lib/rpc'
import { useEditor } from '../store'

interface Props {
  client: EngineClient
}

const VALID_NAME = /^[A-Za-z0-9_-]+$/

/**
 * Project names must survive the backend sanitizer ([A-Za-z0-9_-] only --
 * no separators, spaces or path escapes). Shared by New and Import flows;
 * exported pure so tests cover it directly.
 */
export function isValidProjectName(name: string): boolean {
  return VALID_NAME.test(name)
}

/**
 * Resolve the story document path for a project directory. Projects are
 * created from templates that carry a story.ks (preferred) or entry.lua.
 */
export function storyPathFor(projectPath: string): string {
  return projectPath.replace(/\/+$/, '') + '/story.ks'
}

/** Read a project's story.ks through the engine's sandboxed script reader. */
async function readStory(client: EngineClient, path: string): Promise<string> {
  try {
    const content = (await client.inspect(
      '__editor_read_script(' + JSON.stringify(path) + ')',
    )) as string
    return typeof content === 'string' ? content : ''
  } catch {
    return ''
  }
}

export function ProjectManagerView({ client }: Props) {
  const templates = useEditor((s) => s.templates)
  const projects = useEditor((s) => s.projects)
  const recentProjects = useEditor((s) => s.recentProjects)
  const loadProjects = useEditor((s) => s.loadProjects)
  const loadTemplates = useEditor((s) => s.loadTemplates)
  const addRecentProject = useEditor((s) => s.addRecentProject)
  const openDoc = useEditor((s) => s.openDoc)

  const [name, setName] = useState('')
  const [template, setTemplate] = useState('basic')
  const [importSrc, setImportSrc] = useState('')
  const [importName, setImportName] = useState('')
  const [error, setError] = useState('')
  const [msg, setMsg] = useState('')

  const refresh = useCallback(() => {
    void loadProjects()
    void loadTemplates()
  }, [loadProjects, loadTemplates])

  useEffect(() => {
    refresh()
  }, [refresh])

  const openProject = async (p: { path: string; name: string }) => {
    addRecentProject(p.path, p.name)
    const story = storyPathFor(p.path)
    const content = await readStory(client, story)
    openDoc({
      path: story,
      name: story.split('/').pop() ?? story,
      language: 'kag',
      content,
      dirty: false,
    })
  }

  const handleCreate = async () => {
    setError('')
    setMsg('')
    const trimmed = name.trim()
    if (!trimmed) {
      setError('Enter a project name')
      return
    }
    if (!isValidProjectName(trimmed)) {
      setError('Name may only contain letters, digits, _ or -')
      return
    }
    try {
      const reply = await client.projectCreate(template, trimmed)
      if (!reply.ok) {
        setError(reply.error ?? 'Create failed')
        return
      }
      setMsg(`Created ${trimmed}`)
      setName('')
      void loadProjects()
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e))
    }
  }

  const handleDuplicate = async (p: { path: string; name: string }) => {
    setError('')
    setMsg('')
    const copyName = p.name + '_copy'
    try {
      const reply = await client.projectDuplicate(p.path, copyName)
      if (!reply.ok) {
        setError(reply.error ?? 'Duplicate failed')
        return
      }
      setMsg(`Duplicated as ${copyName}`)
      void loadProjects()
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e))
    }
  }

  const handleImport = async () => {
    setError('')
    setMsg('')
    const src = importSrc.trim()
    const trimmed = importName.trim()
    if (!src) {
      setError('Enter a source folder path')
      return
    }
    if (!trimmed) {
      setError('Enter a project name')
      return
    }
    if (!isValidProjectName(trimmed)) {
      setError('Name may only contain letters, digits, _ or -')
      return
    }
    try {
      const reply = await client.projectImport(src, trimmed)
      if (!reply.ok) {
        setError(reply.error ?? 'Import failed')
        return
      }
      // Normalize Windows separators so the recent entry matches
      // the forward-slash paths used across the editor.
      const importedPath = (reply.path ?? 'projects/' + trimmed).replace(/\\/g, '/')
      addRecentProject(importedPath, trimmed)
      setMsg(`Imported ${trimmed}`)
      setImportSrc('')
      setImportName('')
      void loadProjects()
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e))
    }
  }

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Projects
        <span className="spacer" />
        <button onClick={refresh}>↻</button>
      </div>

      {/* New project */}
      <section className="explorer-section">
        <div className="explorer-section-title">NEW PROJECT</div>
        <input
          className="explorer-filter"
          placeholder="Project name"
          value={name}
          onChange={(e) => setName(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === 'Enter') void handleCreate()
          }}
        />
        <select
          className="explorer-filter"
          value={template}
          onChange={(e) => setTemplate(e.target.value)}
          aria-label="Template"
        >
          {templates.length > 0 ? (
            templates.map((t) => (
              <option key={t.id} value={t.id}>
                {t.name}
              </option>
            ))
          ) : (
            <option value="basic">basic</option>
          )}
        </select>
        <button className="primary" onClick={() => void handleCreate()}>
          Create Project
        </button>
      </section>

      {/* Import existing project from disk */}
      <section className="explorer-section">
        <div className="explorer-section-title">IMPORT PROJECT</div>
        <input
          className="explorer-filter"
          placeholder="Source folder path"
          value={importSrc}
          onChange={(e) => setImportSrc(e.target.value)}
          aria-label="Source folder path"
        />
        <input
          className="explorer-filter"
          placeholder="Import as name"
          value={importName}
          onChange={(e) => setImportName(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === 'Enter') void handleImport()
          }}
          aria-label="Import as name"
        />
        <button className="primary" onClick={() => void handleImport()}>
          Import Project
        </button>
      </section>

      {/* Templates */}
      <section className="explorer-section">
        <div className="explorer-section-title">TEMPLATES ({templates.length})</div>
        {templates.length === 0 && (
          <div className="explorer-empty">No templates — engine offline</div>
        )}
        {templates.map((t) => (
          <div key={t.id} className="explorer-item" title={t.description || t.name}>
            <span className="explorer-file-icon">📦</span>
            <span className="explorer-item-name">{t.name}</span>
            <button
              onClick={() => {
                setName('')
                setTemplate(t.id)
              }}
              title={`Use ${t.name} for a new project`}
            >
              Use
            </button>
          </div>
        ))}
      </section>

      {/* Existing projects */}
      <section className="explorer-section">
        <div className="explorer-section-title">PROJECTS ({projects.length})</div>
        {projects.length === 0 && (
          <div className="explorer-empty">No projects yet</div>
        )}
        {projects.map((p) => (
          <div key={p.path} className="explorer-item" title={p.path}>
            <span className="explorer-file-icon">🗂</span>
            <span className="explorer-item-name">{p.name}</span>
            <button onClick={() => void openProject(p)}>Open</button>
            <button onClick={() => void handleDuplicate(p)}>Duplicate</button>
          </div>
        ))}
      </section>

      {/* Recent projects */}
      <section className="explorer-section">
        <div className="explorer-section-title">RECENT</div>
        {recentProjects.length === 0 && (
          <div className="explorer-empty">No recent projects</div>
        )}
        {recentProjects.map((p) => (
          <div key={p.path} className="explorer-item" title={p.path}>
            <span className="explorer-file-icon">🕘</span>
            <span className="explorer-item-name">{p.name}</span>
            <button onClick={() => void openProject(p)}>Open</button>
          </div>
        ))}
      </section>

      {error && <div className="panel-error">{error}</div>}
      {msg && <div className="panel-msg">{msg}</div>}
    </div>
  )
}
