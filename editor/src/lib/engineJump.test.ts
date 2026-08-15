import { describe, it, expect } from 'vitest'
import {
  buildLabelJumpSnippet,
  parseJumpResult,
  escapeLuaString,
} from './engineJump'

describe('buildLabelJumpSnippet', () => {
  it('builds a well-formed eval that jumps the live ctx to *label', () => {
    const snip = buildLabelJumpSnippet('start')
    expect(snip).toContain('require("flow")')
    expect(snip).toContain('require("kag")')
    expect(snip).toContain('_G._CAESURA_CTX')
    expect(snip).toContain('flow.find_label(c.tokens, "start")')
    expect(snip).toContain('kag.jump(c, "*start")')
    expect(snip).toContain('return "ok"')
  })

  it('reports no-ctx when the running-scene anchor is absent', () => {
    const snip = buildLabelJumpSnippet('route')
    expect(snip).toContain('if not c then return "no-ctx" end')
  })

  it('reports missing when the label is absent from the live tokens', () => {
    const snip = buildLabelJumpSnippet('route')
    expect(snip).toContain('then return "missing" end')
  })

  it('escapes quotes/backslashes/line breaks so the Lua literal stays closed', () => {
    const label = 'a"b' + '\\' + '\n' + 'c'
    const snip = buildLabelJumpSnippet(label)
    const safe = escapeLuaString(label)
    // the escaped label is embedded verbatim in both the lookup and the jump
    expect(snip).toContain('find_label(c.tokens, "' + safe + '")')
    expect(snip).toContain('kag.jump(c, "*' + safe + '")')
  })

  it('never returns an empty snippet', () => {
    expect(buildLabelJumpSnippet('').length).toBeGreaterThan(0)
  })
})

describe('parseJumpResult', () => {
  it('classifies the three discriminator outcomes', () => {
    expect(parseJumpResult('ok')).toBe('ok')
    expect(parseJumpResult('missing')).toBe('missing')
    expect(parseJumpResult('no-ctx')).toBe('no-ctx')
  })

  it('trims surrounding whitespace', () => {
    expect(parseJumpResult('  ok  ')).toBe('ok')
  })

  it('passes through unexpected engine text verbatim', () => {
    expect(parseJumpResult('table: 0x1')).toBe('table: 0x1')
  })

  it('treats empty / blank results as the empty string', () => {
    expect(parseJumpResult('')).toBe('')
    expect(parseJumpResult('   ')).toBe('')
  })
})

describe('escapeLuaString', () => {
  it('escapes a double quote into backslash-quote', () => {
    const v = escapeLuaString('a"b')
    expect(v).toContain('\\"')
  })

  it('doubles a literal backslash', () => {
    expect(escapeLuaString('a\\b')).toContain('\\\\')
  })

  it('escapes a line break into backslash-n', () => {
    expect(escapeLuaString('a\n' + 'b')).toContain('\\n')
  })

  it('escapes a carriage return into backslash-r', () => {
    expect(escapeLuaString('a\r')).toContain('\\r')
  })

  it('leaves plain identifiers unchanged', () => {
    expect(escapeLuaString('start_1_route')).toBe('start_1_route')
  })
})