// Generate web/scripts-index.json — lists every scripts/*.lua module.
// Run: node web/gen-index.mjs
import { readdirSync, writeFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const scriptsDir = join(dirname(fileURLToPath(import.meta.url)), '..', 'scripts')
const mods = {}
function walk(dir, prefix) {
  for (const f of readdirSync(dir, { withFileTypes: true })) {
    if (f.isDirectory()) walk(join(dir, f.name), prefix + f.name + '.')
    else if (f.name.endsWith('.lua')) mods[prefix + f.name.slice(0, -4)] = true
  }
}
walk(scriptsDir, '')
writeFileSync(join(dirname(fileURLToPath(import.meta.url)), 'scripts-index.json'), JSON.stringify(mods, null, 1))
console.log('scripts-index.json:', Object.keys(mods).length, 'modules')
