// Editor store — lightweight IDE state (VS Code-style workbench model).
// Tabs of open documents, active tab, sidebar view, status line.
import { create } from 'zustand'
import { loadSettings, saveSettings, type EditorSettings } from './lib/settings'
import type { EngineClient, ProjectInfo, ProjectTemplate } from './lib/rpc'
import {
  loadRecentProjects,
  saveRecentProjects,
  pushRecentProject,
  type RecentProject,
} from './lib/recentProjects'

export interface OpenDoc {
  /** Asset path as reported by /api/assets, e.g. assets/script/main.ks */
  path: string
  name: string
  /** Language id for Monaco: 'kag' for .ks, 'lua' otherwise */
  language: string
  content: string
  dirty: boolean
}

export type SideView = 'explorer' | 'debug' | 'visual' | 'ai' | 'settings' | 'project'

interface EditorState {
  docs: OpenDoc[]
  activePath: string | null
  sideView: SideView
  /** IDE preference settings (theme/font/line numbers/engine params). */
  settings: EditorSettings
  engineConnected: boolean
  engineScene: string
  engineToken: number
  enginePaused: boolean
  /** Current execution element from the engine, e.g. "[ch]" (round 28). */
  engineCmd: string
  /** Battle 4b: reveal request — the editor scrolls to this line after
   *  a scene-tree click (set by SceneTree, consumed by EditorArea). */
  revealRequest: { path: string; line: number; nonce: number } | null
  /** G4 inspector: the scene element currently inspected (path + line),
   *  set by SceneTree clicks, consumed by InspectorView. */
  inspected: { path: string; line: number } | null
  /** Battle 4c+: the text currently selected in the active editor widget
   *  (path + selection). Set by the text widget; the AiPanel "Ask" section
   *  injects it into LLM queries when the user opts in (default off). */
  editorSelection: { path: string; text: string } | null
  /** Scene Builder: the live cursor position of the active Monaco editor
   *  (path + 1-based line/column). Set by EditorArea on cursor move; consumed
   *  by SceneBuilder so generated lines land at the user's insertion point. */
  editorCursor: { path: string; line: number; column: number } | null
  openDoc: (doc: OpenDoc) => void
  updateDoc: (path: string, content: string) => void
  closeDoc: (path: string) => void
  setActive: (path: string) => void
  setSideView: (v: SideView) => void
  /** Persist + apply a partial settings update (theme/font/line numbers/...). */
  setSettings: (patch: Partial<EditorSettings>) => void
  setEngine: (s: Partial<Pick<EditorState, 'engineConnected' | 'engineScene' | 'engineToken' | 'enginePaused' | 'engineCmd'>>) => void
  /** Insert generated tag text into the active document at the cursor
   *  (or append); marks the doc dirty. */
  insertIntoActive: (text: string) => void
  /** Request the editor to reveal a line in a doc (scene-tree jump). */
  requestReveal: (path: string, line: number) => void
  /** G4 inspector: select a scene element for the inspector panel. */
  setInspected: (path: string, line: number) => void
  /** Battle 4c+: update the active editor selection (or null when none). */
  setSelection: (sel: { path: string; text: string } | null) => void
  /** Scene Builder: record the active editor's live cursor position (or null). */
  setCursor: (cursor: { path: string; line: number; column: number } | null) => void
  /** Scene Builder: insert generated statement text at the active doc's live
   *  cursor line (falling back to append when no cursor is tracked/active);
   *  marks the doc dirty. */
  insertAtCursor: (text: string) => void
  /** Project Manager: the engine RPC client used by loadProjects/loadTemplates.
   *  Registered once by App so the store's async project actions can reach
   *  the engine (avoids threading the client through every panel). */
  client: EngineClient | null
  /** Project Manager: managed projects under ./projects/ (ProjectInfo[]). */
  projects: ProjectInfo[]
  /** Project Manager: discoverable templates (ProjectTemplate[]). */
  templates: ProjectTemplate[]
  /** Project Manager: recently opened projects (persisted to localStorage). */
  recentProjects: RecentProject[]
  /** Project Manager: register the engine client (called by App). */
  setEngineClient: (c: EngineClient | null) => void
  /** Project Manager: fetch + set the managed project list from the engine. */
  loadProjects: () => Promise<void>
  /** Project Manager: fetch + set the template list from the engine. */
  loadTemplates: () => Promise<void>
  /** Project Manager: record an opened project into the recent history
   *  (de-duplicated by path, capped, persisted) and update state. */
  addRecentProject: (path: string, name: string) => void
}

export const useEditor = create<EditorState>((set, get) => ({
  docs: [],
  activePath: null,
  sideView: 'explorer',
  // Hydrate persisted preferences; missing/corrupt storage → defaults.
  settings: loadSettings(),
  client: null,
  projects: [],
  templates: [],
  // Hydrate persisted recent-project history; missing/corrupt → [].
  recentProjects: loadRecentProjects(),
  engineConnected: false,
  engineScene: '',
  engineToken: 0,
  enginePaused: false,
  engineCmd: '',
  revealRequest: null,
  inspected: null,
  editorSelection: null,
  editorCursor: null,
  openDoc: (doc) =>
    set((s) => {
      const existing = s.docs.find((d) => d.path === doc.path)
      if (existing) return { activePath: doc.path }
      return { docs: [...s.docs, doc], activePath: doc.path }
    }),
  updateDoc: (path, content) =>
    set((s) => ({
      docs: s.docs.map((d) =>
        d.path === path ? { ...d, content, dirty: true } : d,
      ),
    })),
  closeDoc: (path) =>
    set((s) => {
      const idx = s.docs.findIndex((d) => d.path === path)
      const docs = s.docs.filter((d) => d.path !== path)
      let activePath = s.activePath
      if (s.activePath === path) {
        const next = docs[Math.max(0, idx - 1)]
        activePath = next ? next.path : null
      }
      return { docs, activePath }
    }),
  setActive: (path) => set({ activePath: path }),
  setSideView: (v) => set({ sideView: v }),
  setSettings: (patch) =>
    set((s) => {
      const next = { ...s.settings, ...patch }
      saveSettings(next)
      return { settings: next }
    }),
  setEngine: (p) => set(p),
  insertIntoActive: (text) =>
    set((s) => {
      if (!s.activePath) return {}
      return {
        docs: s.docs.map((d) =>
          d.path === s.activePath
            ? { ...d, content: d.content + text, dirty: true }
            : d,
        ),
      }
    }),
  requestReveal: (path, line) =>
    set((s) => ({
      activePath: path,
      revealRequest: { path, line, nonce: (s.revealRequest?.nonce ?? 0) + 1 },
    })),
  setInspected: (path, line) => set({ inspected: { path, line } }),
  setSelection: (sel) => set({ editorSelection: sel }),
  setCursor: (cursor) => set({ editorCursor: cursor }),
  insertAtCursor: (text) =>
    set((s) => {
      if (!s.activePath) return {}
      const doc = s.docs.find((d) => d.path === s.activePath)
      if (!doc) return {}
      const lineIndex = resolveInsertLine(
        doc.content,
        s.editorCursor,
        s.activePath,
      )
      const lines = doc.content.split('\n')
      lines.splice(lineIndex, 0, text)
      return {
        docs: s.docs.map((d) =>
          d.path === s.activePath
            ? { ...d, content: lines.join('\n'), dirty: true }
            : d,
        ),
      }
    }),
  setEngineClient: (c) => set({ client: c }),
  addRecentProject: (path, name) =>
    set((s) => {
      const recentProjects = pushRecentProject(s.recentProjects, path, name)
      saveRecentProjects(recentProjects)
      return { recentProjects }
    }),
  loadProjects: async () => {
    const { client } = get()
    if (!client) return
    try {
      const projects = await client.projectList()
      await set({ projects })
    } catch {
      // Engine unreachable — leave the current list untouched.
    }
  },
  loadTemplates: async () => {
    const { client } = get()
    if (!client) return
    try {
      const templates = await client.projectTemplates()
      await set({ templates })
    } catch {
      // Engine unreachable — leave the current list untouched.
    }
  },
}))

/**
 * Resolve the 0-based line index at which a generated statement should be
 * inserted into a document. When a live cursor exists for the target doc the
 * new line is placed just above the cursor line (clamped to the document);
 * otherwise the text is appended at the end. Pure — unit-testable.
 */
export function resolveInsertLine(
  content: string,
  cursor: { path: string; line: number; column: number } | null,
  activePath: string | null,
): number {
  const lineCount = content === '' ? 1 : content.split('\n').length
  if (cursor && cursor.path === activePath && cursor.line >= 1) {
    return Math.min(cursor.line - 1, lineCount)
  }
  return lineCount
}
