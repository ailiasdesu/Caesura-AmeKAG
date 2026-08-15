// Generate web/scripts-index.json — lists every scripts/*.lua module.
//
// Usage:
//   node web/gen-index.mjs                       # defaults (repo scripts -> web/scripts-index.json)
//   node web/gen-index.mjs <scriptsDir> <out>     # override inputs/output
//
// Scans the Lua script tree for *.lua files (ignoring dotfiles/dot-directories),
// and writes a flat JSON object mapping each module path (dot-joined, no .lua
// suffix) -> true. Keys are output in sorted order so the artifact is
// byte-for-byte reproducible regardless of filesystem enumeration order.
import { readdirSync, writeFileSync, existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join, resolve } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))

// Recursively collect every *.lua module path (dot-joined, no .lua suffix).
// Skips hidden entries (leading '.') both for files and directories.
function collectLua(dir, prefix = '', out = {}) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    if (entry.name.startsWith('.')) continue
    const rel = prefix ? `${prefix}.${entry.name}` : entry.name
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
      `[gen-index] scripts directory not found: ${scriptsDir}\n` +
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

export function main(argv = process.argv.slice(2)) {
  const { scriptsDir, output } = resolvePaths(argv)
  const mods = buildIndex(scriptsDir)
  writeFileSync(output, serialize(mods))
  console.log(`[gen-index] ${Object.keys(mods).length} modules ->`, output)
  return { count: Object.keys(mods).length, output }
}

// Run only when invoked directly (not when imported by tests).
if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    main()
  } catch (err) {
    console.error(err.message)
    process.exit(1)
  }
}
