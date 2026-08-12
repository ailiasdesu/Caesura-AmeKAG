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

export type SideView = 'explorer' | 'debug' | 'visual'

interface EditorState {
  docs: OpenDoc[]
  activePath: string | null
  sideView: SideView
  engineConnected: boolean
  engineScene: string
  engineToken: number
  enginePaused: boolean
  openDoc: (doc: OpenDoc) => void
  updateDoc: (path: string, content: string) => void
  closeDoc: (path: string) => void
  setActive: (path: string) => void
  setSideView: (v: SideView) => void
  setEngine: (s: Partial<Pick<EditorState, 'engineConnected' | 'engineScene' | 'engineToken' | 'enginePaused'>>) => void
}

export const useEditor = create<EditorState>((set) => ({
  docs: [],
  activePath: null,
  sideView: 'explorer',
  engineConnected: false,
  engineScene: '',
  engineToken: 0,
  enginePaused: false,
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
}))
