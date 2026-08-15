// Editor store — lightweight IDE state (VS Code-style workbench model).
// Tabs of open documents, active tab, sidebar view, status line.
import { create } from 'zustand'

export interface OpenDoc {
  /** Asset path as reported by /api/assets, e.g. assets/script/main.ks */
  path: string
  name: string
  /** Language id for Monaco: 'kag' for .ks, 'lua' otherwise */
  language: string
  content: string
  dirty: boolean
}

export type SideView = 'explorer' | 'debug' | 'visual' | 'ai'

interface EditorState {
  docs: OpenDoc[]
  activePath: string | null
  sideView: SideView
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
  openDoc: (doc: OpenDoc) => void
  updateDoc: (path: string, content: string) => void
  closeDoc: (path: string) => void
  setActive: (path: string) => void
  setSideView: (v: SideView) => void
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
}

export const useEditor = create<EditorState>((set) => ({
  docs: [],
  activePath: null,
  sideView: 'explorer',
  engineConnected: false,
  engineScene: '',
  engineToken: 0,
  enginePaused: false,
  engineCmd: '',
  revealRequest: null,
  inspected: null,
  editorSelection: null,
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
}))
