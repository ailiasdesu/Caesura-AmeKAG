// Unit tests for lib/commandLint.ts — the shared command-name table and the
// Inspector's parameter lint. Pure logic, no DOM / Monaco involved.
import { describe, it, expect } from 'vitest'
import {
  KNOWN_COMMANDS,
  KNOWN_COMMAND_SET,
  KNOWN_PARAM_HINTS,
  lintCommand,
  knownCommandName,
} from './commandLint'

describe('KNOWN_COMMANDS table', () => {
  it('covers representative flow / text / layer / audio / system commands', () => {
    for (const cmd of ['jump', 'goto', 'call', 'if', 'while', 'ch', 'text', 'l',
      'bg', 'fg', 'position', 'trans', 'playbgm', 'playse', 'wait', 'set', 'inc']) {
      expect(KNOWN_COMMANDS, 'missing ' + cmd).toContain(cmd)
    }
  })

  it('contains no duplicates (a Set of the array has the same size)', () => {
    expect(KNOWN_COMMAND_SET.size).toBe(KNOWN_COMMANDS.length)
  })
})

describe('knownCommandName', () => {
  it('recognizes a known command case-insensitively', () => {
    expect(knownCommandName('ch')).toBe(true)
    expect(knownCommandName('PLAYBGM')).toBe(true)
  })

  it('rejects an unknown command and empty/whitespace', () => {
    expect(knownCommandName('notacommand')).toBe(false)
    expect(knownCommandName('')).toBe(false)
    expect(knownCommandName('   ')).toBe(false)
  })
})

describe('KNOWN_PARAM_HINTS', () => {
  it('documents the params used by the round-82 test fixture commands', () => {
    expect(KNOWN_PARAM_HINTS.bg).toContain('storage')
    expect(KNOWN_PARAM_HINTS.ch).toContain('text')
    expect(KNOWN_PARAM_HINTS.playbgm).toContain('loop')
  })
})

describe('lintCommand', () => {
  it('marks a known command and passes documented params as known', () => {
    const lint = lintCommand('bg', { storage: 'room.png' })
    expect(lint.knownCommand).toBe(true)
    expect(lint.params.storage).toBe('known')
    expect(lint.unlistedCount).toBe(0)
  })

  it('flags an unknown command without crashing', () => {
    const lint = lintCommand('notacommand', { foo: 'bar' })
    expect(lint.knownCommand).toBe(false)
    expect(lint.params.foo).toBe('known')
  })

  it('flags an unlisted param (soft hint) for a curated command', () => {
    const lint = lintCommand('bg', { storage: 'room.png', wobble: '2' })
    expect(lint.params.wobble).toBe('unlisted')
    expect(lint.unlistedCount).toBe(1)
    expect(lint.params.storage).toBe('known')
  })

  it('treats a lone param key (value === "true") as a bare flag', () => {
    const lint = lintCommand('bg', { loop: 'true' })
    expect(lint.params.loop).toBe('flag')
  })

  it('keeps a documented flag param green (loop is in playbgm hints)', () => {
    const lint = lintCommand('playbgm', { file: 'bgm.ogg', loop: 'true' })
    expect(lint.params.loop).toBe('flag')
    expect(lint.params.file).toBe('known')
  })

  it('grants no unlisted verdict for an uncurated command (no opinion)', () => {
    const lint = lintCommand('random', { seed: '42' })
    expect(lint.knownCommand).toBe(true)
    expect(lint.params.seed).toBe('known')
    expect(lint.unlistedCount).toBe(0)
  })

  it('normalizes the command word to lowercase before lookup', () => {
    expect(lintCommand('BG', { storage: 'x' }).knownCommand).toBe(true)
  })
})
