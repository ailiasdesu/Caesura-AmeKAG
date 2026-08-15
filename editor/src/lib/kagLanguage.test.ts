// Editor LSP surface tests for kagLanguage.ts — KAG Neo-Genesis Monaco
// language definition. Verifies that every command added in rounds 71-74
// is registered for completion-adjacent highlighting (tag token), that the
// [sel]/[button]/choice-command family is present, and that
// registerKagLanguage is idempotent and installs the expected Monaco hooks.
//
// Note: actual tag/param/diagnostic *completions* are produced engine-side
// by scripts/kag/lsp.lua (driven by the schema registry); the editor's
// local surface is this tokenizer + the KAG_COMMANDS keyword set that tags
// <command> as valid so Monaco highlights it. That set must stay in sync
// with the commands the engine exposes.
import { describe, it, expect, vi, beforeEach } from 'vitest'
import * as monaco from 'monaco-editor'
import { KAG_COMMANDS, registerKagLanguage } from '../ide/kagLanguage'

// Minimal Monaco facade capturing what registerKagLanguage installs.
vi.mock('monaco-editor', () => ({
  languages: {
    getLanguages: vi.fn(() => []),
    register: vi.fn(),
    setMonarchTokensProvider: vi.fn(),
    setLanguageConfiguration: vi.fn(),
  },
}))

// Cast the facades for typed assertions.
const langs = monaco.languages as any
const tokenProviders: any[] = []
const langConfigs: any[] = []

beforeEach(() => {
  vi.clearAllMocks()
  tokenProviders.length = 0
  langConfigs.length = 0
  langs.getLanguages.mockReturnValue([])
  langs.setMonarchTokensProvider.mockImplementation((_id: string, cfg: any) => {
    tokenProviders.push(cfg)
  })
  langs.setLanguageConfiguration.mockImplementation((_id: string, cfg: any) => {
    langConfigs.push(cfg)
  })
})

describe('KAG_COMMANDS (rounds 71-74 registration)', () => {
  const rounds71to74 = [
    // round 71 math (KAG3-compat arithmetic)
    'add', 'sub', 'mul', 'div', 'mod', 'dec',
    // round 71 character
    'csp', 'csd', 'csl',
    // round 71 text speed
    'textspeed', 'cps',
    // round 71 effects
    'palette', 'vibrate',
    // round 71 notification + resource
    'notify', 'preload',
  ]

  it('registers every round-71/72 command', () => {
    for (const cmd of rounds71to74) {
      expect(KAG_COMMANDS, 'missing round-71/72 command: ' + cmd).toContain(cmd)
    }
  })

  it('registers the round-74 choice family (sel/select/button)', () => {
    expect(KAG_COMMANDS).toContain('sel')
    expect(KAG_COMMANDS).toContain('select')
    expect(KAG_COMMANDS).toContain('button')
    expect(KAG_COMMANDS).toContain('endselect')
    expect(KAG_COMMANDS).toContain('endbutton')
  })

  it('holds no duplicate command names', () => {
    const seen = new Set<string>()
    for (const cmd of KAG_COMMANDS) {
      expect(seen.has(cmd), 'duplicate command: ' + cmd).toBe(false)
      seen.add(cmd)
    }
  })
})

describe('registerKagLanguage (Monaco wiring)', () => {
  it('installs the kag language with the round-71 command set as keywords', () => {
    const kw: any[] = []
    langs.setMonarchTokensProvider.mockImplementation((_id: string, cfg: any) => {
      tokenProviders.push(cfg)
      kw.push(cfg.keywords)
    })
    registerKagLanguage()
    expect(langs.register).toHaveBeenCalledTimes(1)
    expect(langs.register).toHaveBeenCalledWith({ id: 'kag', extensions: ['.ks'] })
    expect(tokenProviders).toHaveLength(1)
    // the exact exported keyword array is what flags [cmd] as valid tags
    expect(kw[0]).toBe(KAG_COMMANDS)
    expect(langConfigs).toHaveLength(1)
    expect(langs.setMonarchTokensProvider.mock.calls[0][0]).toBe('kag')
  })

  it('is idempotent: skips re-registration when kag is already registered', () => {
    langs.getLanguages.mockReturnValue([{ id: 'kag' }])
    registerKagLanguage()
    expect(langs.register).not.toHaveBeenCalled()
    expect(tokenProviders).toHaveLength(0)
    expect(langConfigs).toHaveLength(0)
  })
})
