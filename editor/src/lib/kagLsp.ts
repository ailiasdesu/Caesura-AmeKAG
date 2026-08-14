// Caesura IDE — KAG language service client.
// Bridges the engine's /api/eval endpoint to Monaco providers:
// completion / hover / diagnostics, driven by the engine's declarative
// command contracts (scripts/kag/lsp.lua). The engine returns a JSON
// string; we parse it once.

import type * as monaco from 'monaco-editor'
import type { EngineClient } from './rpc'
import { luaString, luaValue } from './luaString'

export interface CompletionItem {
  label: string
  kind: number
  detail?: string
  insertText?: string
}

export interface HoverItem {
  title: string
  text: string
}

export interface DiagnosticItem {
  line: number
  col: number
  message: string
  severity: number
}

export interface DefinitionItem {
  name?: string
  line?: number | null
  col?: number
}

export interface ReferenceItem {
  kind: 'definition' | 'reference'
  line?: number | null
  col?: number
}

export interface KagLspOptions {
  /** Debounce for diagnostics (ms). */
  diagnosticsDelay?: number
}

const LSP_LINE_LIMIT = 2000 // don't push huge files through eval

export function lspCall(client: EngineClient, method: string, ...args: unknown[]): Promise<string> {
  const argStr = args
    .map((a) => (typeof a === 'string' ? luaString(a) : luaValue(a)))
    .join(', ')
  const code =
    `local lsp = require('kag.lsp'); ` +
    `return lsp.json('${method}'` +
    (args.length > 0 ? ', ' + argStr : '') +
    `)`
  return client.evalRaw(code)
}

export class KagLsp {
  private disposables: monaco.IDisposable[] = []
  private diagTimer: ReturnType<typeof setTimeout> | null = null
  private diagModel: monaco.editor.ITextModel | null = null

  constructor(
    private client: EngineClient,
    private monacoNs: typeof monaco,
    private opts: KagLspOptions = {},
  ) {}

  /** Register all providers for the 'kag' language. */
  register(): void {
    this.registerCompletion()
    this.registerHover()
    this.registerDiagnostics()
    this.registerDefinition()
    this.registerReferences()
  }

  private registerCompletion(): void {
    const m = this.monacoNs
    this.disposables.push(
      m.languages.registerCompletionItemProvider('kag', {
        triggerCharacters: ['[', ' '],
        provideCompletionItems: async (model, position) => {
          const lineText = model.getLineContent(position.lineNumber)
          // only suggest inside/after a tag on this line
          const open = lineText.lastIndexOf('[')
          const close = lineText.indexOf(']', open + 1)
          if (open < 0 || (close > 0 && close < position.column - 1)) {
            return { suggestions: [] }
          }
          try {
            const json = await lspCall(this.client, 'completion', lineText)
            const items = JSON.parse(json) as CompletionItem[]
            const word = model.getWordUntilPosition(position)
            const range = new m.Range(
              position.lineNumber,
              word.startColumn,
              position.lineNumber,
              word.endColumn,
            )
            return {
              suggestions: items.map((it) => ({
                label: it.label,
                kind: it.kind as monaco.languages.CompletionItemKind,
                detail: it.detail ?? '',
                insertText: it.insertText ?? it.label,
                range,
              })),
            }
          } catch {
            return { suggestions: [] }
          }
        },
      }),
    )
  }

  private registerHover(): void {
    const m = this.monacoNs
    this.disposables.push(
      m.languages.registerHoverProvider('kag', {
        provideHover: async (model, position) => {
          const line = model.getLineContent(position.lineNumber)
          const open = line.lastIndexOf('[')
          if (open < 0) return null
          // command name at/after the bracket
          const mCmd = line.slice(open + 1).match(/^\s*([\w_]+)/)
          if (!mCmd) return null
          const cmd = mCmd[1]
          const col = position.column - 1
          const inCmdName =
            col >= open && col <= open + cmd.length + 1
          const word = model.getWordAtPosition(position)
          const param = inCmdName ? undefined : word?.word
          try {
            const json = await lspCall(this.client, 'hover', cmd, param ?? '')
            const parsed = JSON.parse(json) as HoverItem[]
            const item = parsed[0]
            if (!item) return null
            return {
              range: new m.Range(
                position.lineNumber,
                Math.max(1, open),
                position.lineNumber,
                line.length + 1,
              ),
              contents: [
                { value: `**${item.title}**` },
                ...item.text.split('\n').map((t) => ({ value: t })),
              ],
            }
          } catch {
            return null
          }
        },
      }),
    )
  }

  private registerDefinition(): void {
    const m = this.monacoNs
    this.disposables.push(
      m.languages.registerDefinitionProvider('kag', {
        provideDefinition: async (model, position) => {
          // only on *label references and navigation targets
          const line = model.getLineContent(position.lineNumber)
          if (!line.includes('*')) return null
          try {
            const json = await lspCall(
              this.client,
              'definition',
              model.getValue(),
              position.lineNumber,
              position.column,
            )
            const parsed = JSON.parse(json) as DefinitionItem[]
            const def = parsed[0]
            if (!def || def.line == null) return null // cross-scene target
            return {
              uri: model.uri,
              range: new m.Range(def.line, def.col || 1, def.line, (def.col || 1) + 1),
            }
          } catch {
            return null
          }
        },
      }),
    )
  }

  private registerReferences(): void {
    const m = this.monacoNs
    this.disposables.push(
      m.languages.registerReferenceProvider('kag', {
        provideReferences: async (model, position) => {
          const line = model.getLineContent(position.lineNumber)
          if (!line.includes('*')) return []
          try {
            const json = await lspCall(
              this.client,
              'definition',
              model.getValue(),
              position.lineNumber,
              position.column,
            )
            const parsed = JSON.parse(json) as DefinitionItem[]
            const def = parsed[0]
            if (!def || def.name == null) return []
            const refsJson = await lspCall(this.client, 'references', model.getValue(), def.name)
            const refs = JSON.parse(refsJson) as ReferenceItem[]
            return refs
              .filter((r): r is ReferenceItem & { line: number } => r.line != null)
              .map((r) => ({
                uri: model.uri,
                range: new m.Range(r.line, r.col || 1, r.line, (r.col || 1) + 1),
              }))
          } catch {
            return []
          }
        },
      }),
    )
  }

  private registerDiagnostics(): void {
    const m = this.monacoNs
    const owner = 'kag-lsp'

    const run = async (model: monaco.editor.ITextModel): Promise<void> => {
      if (model.getLanguageId() !== 'kag') return
      if (model.getValueLength() > LSP_LINE_LIMIT * 80) return // heuristic cap
      const text = model.getValue()
      try {
        const json = await lspCall(this.client, 'diagnostics', text)
        const items = JSON.parse(json) as DiagnosticItem[]
        const markers: monaco.editor.IMarkerData[] = items.map((d) => ({
          severity: (d.severity === 1
            ? m.MarkerSeverity.Error
            : m.MarkerSeverity.Warning) as monaco.MarkerSeverity,
          message: d.message,
          startLineNumber: d.line,
          startColumn: d.col || 1,
          endLineNumber: d.line,
          endColumn: (d.col || 1) + 1,
        }))
        m.editor.setModelMarkers(model, owner, markers)
      } catch {
        // engine unreachable: leave stale markers
      }
    }

    // debounce on content change (model instance event); full pass when
    // a doc opens (editor-level onDidCreateModel).
    this.disposables.push(
      m.editor.onDidCreateModel((model) => {
        if (model.getLanguageId() === 'kag') void run(model)
      }),
    )
    this.disposables.push(
      m.editor.onDidCreateModel((model) => {
        if (model.getLanguageId() !== 'kag') return
        model.onDidChangeContent(() => {
          if (this.diagTimer) clearTimeout(this.diagTimer)
          this.diagModel = model
          this.diagTimer = setTimeout(() => {
            if (this.diagModel) void run(this.diagModel)
            this.diagTimer = null
          }, this.opts.diagnosticsDelay ?? 500)
        })
      }),
    )
  }

  dispose(): void {
    if (this.diagTimer) clearTimeout(this.diagTimer)
    for (const d of this.disposables) d.dispose()
    this.disposables = []
  }
}