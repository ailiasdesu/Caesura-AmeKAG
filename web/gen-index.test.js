// @vitest-environment node
// Tests for web/gen-index.mjs — the scripts/*.lua module index generator.
import { describe, it, expect } from 'vitest'
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, rmSync, existsSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { fileURLToPath } from 'node:url'
import { dirname, join, basename } from 'node:path'
import { buildIndex, serialize, resolvePaths, main } from './gen-index.mjs'

const here = dirname(fileURLToPath(import.meta.url))

// Create an isolated fixture scripts/ tree under the OS temp dir.
function makeFixture(files) {
  const root = mkdtempSync(join(tmpdir(), 'gen-index-fixture-'))
  for (const rel of files) {
    const full = join(root, rel)
    mkdirSync(dirname(full), { recursive: true })
    writeFileSync(full, 'x')
  }
  return root
}

function cleanup(root) {
  if (root) rmSync(root, { recursive: true, force: true })
}

describe('buildIndex (fixture scripts tree)', () => {
  it('indexes legal *.lua modules with dot-joined subdirectory paths', () => {
    const root = makeFixture([
      'tokenizer.lua',
      'scheduler.lua',
      'kag/schema.lua',
      'kag/commands/text.lua',
      'nested/deep/util.lua',
    ])
    try {
      const idx = buildIndex(root)
      expect(idx).toEqual({
        tokenizer: true,
        scheduler: true,
        'kag.schema': true,
        'kag.commands.text': true,
        'nested.deep.util': true,
      })
    } finally { cleanup(root) }
  })

  it('excludes non-.lua files and hidden entries', () => {
    const root = makeFixture([
      'good.lua',
      'backend.lua.bak',
      'story.ks',
      'count_coupling.py',
      'build.sh',
      '.hidden.lua',
      '.git/hooks/init.lua',
    ])
    try {
      expect(buildIndex(root)).toEqual({ good: true })
    } finally { cleanup(root) }
  })

  it('returns an empty object for an empty scripts directory', () => {
    const root = makeFixture([])
    try {
      expect(buildIndex(root)).toEqual({})
    } finally { cleanup(root) }
  })

  it('throws a descriptive error when the scripts directory is missing', () => {
    expect(() => buildIndex(join(here, 'nonexistent-scripts-dir-xyz'))).toThrow(/scripts directory not found/)
  })
})

describe('determinism and serialization', () => {
  it('serializes keys in sorted order regardless of insertion order', () => {
    const a = { zebra: true, alpha: true, 'kag.z': true, mid: true }
    const b = { mid: true, 'kag.z': true, alpha: true, zebra: true }
    expect(serialize(a)).toBe(serialize(b))
    expect(Object.keys(JSON.parse(serialize(a)))).toEqual(['alpha', 'kag.z', 'mid', 'zebra'])
  })

  it('produces valid, importable JSON (plain true-map, no trailing newline)', () => {
    const str = serialize({ b: true, a: true })
    expect(() => JSON.parse(str)).not.toThrow()
    expect(JSON.parse(str)).toEqual({ a: true, b: true })
    for (const v of Object.values(JSON.parse(str))) expect(v).toBe(true)
    expect(str.endsWith('}')).toBe(true)
  })
})

describe('main writes a valid output artifact', () => {
  it('writes configurable output that round-trips through JSON.parse', () => {
    const fixture = makeFixture(['a.lua', 'sub/b.lua', 'c.txt'])
    const outRoot = mkdtempSync(join(tmpdir(), 'gen-index-out-'))
    const outFile = join(outRoot, 'custom-index.json')
    try {
      const res = main([fixture, outFile])
      expect(res.count).toBe(2)
      expect(res.output).toBe(outFile)
      const written = readFileSync(outFile, 'utf8')
      expect(JSON.parse(written)).toEqual({ a: true, 'sub.b': true })
      expect(written.endsWith('}')).toBe(true)
    } finally { cleanup(fixture); cleanup(outRoot) }
  })
})

describe('resolvePaths (CLI argument mapping)', () => {
  it('defaults to repo scripts -> web/scripts-index.json', () => {
    const { scriptsDir, output } = resolvePaths([])
    expect(basename(scriptsDir)).toBe('scripts')
    expect(basename(output)).toBe('scripts-index.json')
  })
  it('honors explicit positional args resolved against cwd', () => {
    const { scriptsDir, output } = resolvePaths(['some/dir', 'some/out.json'])
    expect(scriptsDir.endsWith(join('some', 'dir'))).toBe(true)
    expect(output.endsWith(join('some', 'out.json'))).toBe(true)
  })
})

describe('real-repo consistency smoke (round 79 guard)', () => {
  it('checked-in scripts-index.json matches the live scripts/ tree', () => {
    const repoScripts = join(here, '..', 'scripts')
    const checkedIn = join(here, 'scripts-index.json')
    if (!existsSync(repoScripts) || !existsSync(checkedIn)) return
    const live = buildIndex(repoScripts)
    const stored = JSON.parse(readFileSync(checkedIn, 'utf8'))
    expect(stored).toEqual(live)
  })
})
