import { describe, it, expect, vi } from 'vitest'
import { lspCall } from './kagLsp'
import { EngineClient } from './rpc'

function mockClient() {
  const calls: string[] = []
  const client = {
    evalRaw: vi.fn(async (code: string) => {
      calls.push(code)
      return '[]'
    }),
  } as unknown as EngineClient
  return { client, calls }
}

// ---------------------------------------------------------------------------
// Round 76: [i18n] (hot-switch UI language; contract declares ONE required
// param language=, category system) and [goto] (flow command, completion-only,
// no contract). These tests pin the CLIENT side of the bridge: the exact
// method + escaped payload forwarded to scripts/kag/lsp.lua, and the JSON the
// engine actually returns.
//
// Lua-side behaviour LOCKED from the real interpreter; the authoritative Lua-
// side check is tests/scripts/test_lsp.lua:
//   - lsp.completion('[i18n ')            -> [{label=language, kind=5, insertText=language=}]
//   - lsp.completion('[g')                -> includes goto (kind=3)
//   - lsp.hover('i18n')                   -> {title=[i18n], text=desc, params: language, category: system}
//   - lsp.diagnostics('[i18n lang="zh"]') -> [missing-required 'language' (1), unknown param 'lang' (2)]
//   - lsp.diagnostics('[i18n language="zh"]') -> []  (clean)
// The bridge must NOT filter any of these - it carries Lua's answer through.
// ---------------------------------------------------------------------------
describe('round-76 [i18n] completion probe', () => {
  it('dispatches a completion probe for "[i18n " (param context)', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'completion', '[i18n ')
    expect(calls).toHaveLength(1)
    expect(calls[0]).toContain("lsp.json('completion'")
    // trailing space makes nameComplete=true in lsp.lua, so it completes the
    // [i18n] contract params. luaString wraps content starting with '[' as
    // [=[<content>]=] => [=[[i18n ]=].
    expect(calls[0]).toContain('[=[[i18n ]=]')
  })

  it('surfaces language= as the only [i18n] param from the returned JSON', async () => {
    // Mirror what lsp.lua returns for '[i18n ' (LOCKED from the real Lua run):
    // exactly ONE item, language, required, insertText 'language='.
    const client = {
      evalRaw: vi.fn(async () =>
        JSON.stringify([{ detail: 'string (required)', label: 'language', insertText: 'language=', kind: 5 }]),
      ),
    } as unknown as EngineClient
    const json = await lspCall(client, 'completion', '[i18n ')
    expect(json).toContain('"label":"language"')
    expect(json).toContain('"insertText":"language="')
    // the round-76 contract has ONLY language=; parse the bridge's answer and
    // assert exactly one param item surfaces, with no second/unknown param
    const items = JSON.parse(json) as Array<{ label?: string; insertText?: string }>
    expect(items).toHaveLength(1)
    expect(items[0].label).toBe('language')
    expect(items[0].insertText).toBe('language=')
    // assert a single param item: exactly one 'label' key in the JSON
    const labels = (json.match(/"label":/g) ?? []).length
    expect(labels).toBe(1)
  })
})

describe('round-76 [goto] completion probe', () => {
  it('dispatches a completion probe for "[g" (command-name prefix)', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'completion', '[g')
    expect(calls).toHaveLength(1)
    expect(calls[0]).toContain("lsp.json('completion'")
    // luaString wraps the prefix '[g' as [=[<content>]=] => [=[[g]=]
    expect(calls[0]).toContain('[=[[g]=]')
  })

  it('surfaces goto from the command-name completion JSON', async () => {
    // LOCKED from real Lua: '[g' resolves to g/get_texture/gallery/goto.
    const client = {
      evalRaw: vi.fn(async () =>
        JSON.stringify([
          { label: 'g', kind: 3, insertText: 'g ' },
          { label: 'get_texture', kind: 3, insertText: 'get_texture ' },
          { label: 'gallery', kind: 3, insertText: 'gallery ' },
          { label: 'goto', kind: 3, insertText: 'goto ' },
        ]),
      ),
    } as unknown as EngineClient
    const json = await lspCall(client, 'completion', '[g')
    expect(json).toContain('"label":"goto"')
  })
})

describe('round-76 [i18n] hover probe', () => {
  it('dispatches a hover probe for the i18n command', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'hover', 'i18n')
    expect(calls).toHaveLength(1)
    expect(calls[0]).toContain("lsp.json('hover'")
    expect(calls[0]).toContain('[=[i18n]=]')
  })

  it('shows the [i18n] contract desc from the returned JSON', async () => {
    // LOCKED from real Lua: hover text = desc + params + category: system.
    const client = {
      evalRaw: vi.fn(async () =>
        JSON.stringify([{ title: '[i18n]', text:
          ['hot-switch the UI language mid-scene (language=xx)', 'params: language', 'category: system'].join(String.fromCharCode(10)),
        }]),
      ),
    } as unknown as EngineClient
    const json = await lspCall(client, 'hover', 'i18n')
    expect(json).toContain('"title":"[i18n]"')
    expect(json).toContain('hot-switch the UI language mid-scene (language=xx)')
    expect(json).toContain('category: system')
  })

  it('shows the language= param spec for hover on the param', async () => {
    const client = {
      evalRaw: vi.fn(async () =>
        JSON.stringify([{ title: '[i18n]', text: 'param `language`  type=string  required' }]),
      ),
    } as unknown as EngineClient
    const json = await lspCall(client, 'hover', 'i18n', 'language')
    expect(json).toContain('type=string')
    expect(json).toContain('required')
  })
})

describe('round-76 [i18n] diagnostics probe', () => {
  it('forwards [i18n lang="zh"] verbatim so the Lua linter sees the param', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'diagnostics', '[i18n lang="zh"]')
    expect(calls).toHaveLength(1)
    expect(calls[0]).toContain("lsp.json('diagnostics'")
    expect(calls[0]).toContain('[i18n lang="zh"]')
  })

  it('locks the Lua behavior: lang= is unknown-param (plus the missing-required warning)', async () => {
    // LOCKED from the real Lua run: the round-76 contract declares only
    // language=, so lang= surfaces BOTH a missing-required 'language' issue
    // (severity 1) and an unknown param 'lang' warning (severity 2). The
    // bridge must surface the engine's answer verbatim.
    const client = {
      evalRaw: vi.fn(async () =>
        JSON.stringify([
          { severity: 1, line: 1, col: 1, message: "[i18n] missing required param 'language'" },
          { severity: 2, line: 1, col: 1, message: "unknown param 'lang' for [i18n]" },
        ]),
      ),
    } as unknown as EngineClient
    const json = await lspCall(client, 'diagnostics', '[i18n lang="zh"]')
    expect(json).toContain('unknown param ')
    expect(json).toContain('lang')
    expect(json).toContain('for [i18n]')
    expect(json).toContain('"severity":2')
    expect(json).toContain("missing required param 'language'")
    expect(json).toContain('"severity":1')
  })

  it('returns no diagnostics for the valid [i18n language="zh"] form', async () => {
    const client = {
      evalRaw: vi.fn(async () => '[]'),
    } as unknown as EngineClient
    const json = await lspCall(client, 'diagnostics', '[i18n language="zh"]')
    expect(json).toBe('[]')
  })
})