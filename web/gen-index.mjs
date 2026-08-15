// Generate web/scripts-index.json — lists every scripts/*.lua module.
//
// Usage:
//   node web/gen-index.mjs                       # defaults (repo scripts -> web/scripts-index.json)
//   node web/gen-index.mjs <scriptsDir> <out>     # override inputs/output
//   node web/gen-index.mjs --check                # freshness guard: fail if committed artifact is stale/missing
//
// Scans the Lua script tree for *.lua files (ignoring dotfiles/dot-directories),
// and writes a flat JSON object mapping each module path (dot-joined, no .lua
// suffix) -> true. Keys are output in sorted order so the artifact is
// byte-for-byte reproducible regardless of filesystem enumeration order.
import { readdirSync, writeFileSync, existsSync, readFileSync, mkdtempSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { fileURLToPath } from 'node:url'
import { dirname, join, resolve } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))

// Recursively collect every *.lua module path (dot-joined, no .lua suffix).
// Skips hidden entries (leading '.') both for files and directories.
function collectLua(dir, prefix = '', out = {}) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    if (entry.name.startsWith('.')) continue
    const rel = prefix ? prefix + '.' + entry.name : entry.name
    if (entry.isDirectory()) collectLua(join(dir, entry.name), rel, out)
    else if (entry.isFile() && entry.name.endsWith('.lua')) {
      out[rel.slice(0, -4)] = true
    }
  }
  return out
}

// Build the module index object for a scripts directory.
// Throws a descriptive error when the directory does not exist.
export function buildIndex(scriptsDir) {
  if (!existsSync(scriptsDir)) {
    throw new Error(
      '[gen-index] scripts directory not found: ' + scriptsDir + '\n' +
      'Pass the scripts directory as the first argument, or run from the repo root.'
    )
  }
  return collectLua(scriptsDir)
}

// Canonical, deterministic serialization: keys sorted, one-space indent, no
// trailing newline (matches the historical artifact byte-for-byte).
export function serialize(mods) {
  const sorted = {}
  for (const key of Object.keys(mods).sort()) sorted[key] = mods[key]
  return JSON.stringify(sorted, null, 1)
}

// Resolve scripts dir and output path from argv (defaults derived from this
// file's location so the script can run from any working directory).
export function resolvePaths(argv = process.argv.slice(2)) {
  const scriptsDir = argv[0]
    ? resolve(process.cwd(), argv[0])
    : join(here, '..', 'scripts')
  const output = argv[1]
    ? resolve(process.cwd(), argv[1])
    : join(here, 'scripts-index.json')
  return { scriptsDir, output }
}

// Generate the byte-for-byte canonical artifact for scriptsDir into output.
// In --check mode the artifact is written to a temp file and diffed against the
// committed output without overwriting it, exiting nonzero when stale (the
// round 79 ENOENT guard: deleting scripts/*.lua without regenerating the index).
export function main(argv = process.argv.slice(2)) {
  const { scriptsDir, output } = resolvePaths(
    argv.filter((arg) => arg !== '--check')
  )
  const mods = buildIndex(scriptsDir)
  const text = serialize(mods)

  if (argv.includes('--check')) {
    const tmp = join(mkdtempSync(join(tmpdir(), 'gen-index-')), 'scripts-index.json')
    try {
      writeFileSync(tmp, text)
      const expected = existsSync(output) ? readFileSync(output, 'utf8') : null
      if (expected !== text) {
        const detail = expected === null
          ? 'artifact missing: ' + output
          : 'artifact stale (' + Object.keys(mods).length + ' modules scanned)'
        console.error(
          '[gen-index] CHECK FAILED: ' + detail + '\n' +
          'Run `node web/gen-index.mjs` and commit the regenerated scripts-index.json.'
        )
        return { ok: false, count: Object.keys(mods).length, output }
      }
      console.log('[gen-index] CHECK OK: ' + Object.keys(mods).length + ' modules up to date ->', output)
      return { ok: true, count: Object.keys(mods).length, output }
    } finally {
      rmSync(dirname(tmp), { recursive: true, force: true })
    }
  }

  writeFileSync(output, text)
  console.log('[gen-index] ' + Object.keys(mods).length + ' modules ->', output)
  return { count: Object.keys(mods).length, output }
}

// Run only when invoked directly (not when imported by tests).
if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const result = main()
    // --check exits nonzero when the committed artifact is stale/missing.
    if (result && result.ok === false) process.exit(1)
  } catch (err) {
    console.error(err.message)
    process.exit(1)
  }
}
