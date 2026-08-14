// G5 path-B spike 1: tokenizer + lpeg (pure Lua) under wasmoon 2.x.
// Proves scripts/tokenizer.lua + scripts/lpeg.lua run unmodified in the browser runtime.
import { Lua } from 'wasmoon'
import { readFileSync } from 'node:fs'

const factory = await Lua.load()
const lua = factory.createState()

// preload pure-Lua modules via package.preload
const preloads = {
  lpeg: '../scripts/lpeg.lua',
  tokenizer: '../scripts/tokenizer.lua',
}
for (const [name, rel] of Object.entries(preloads)) {
  let src = readFileSync(new URL(rel, import.meta.url), 'utf8')
  if (src.charCodeAt(0) === 0xfeff) src = src.slice(1) // strip UTF-8 BOM
  lua.global.set('__PRELOAD_' + name, src)
}
lua.global.set('__PRELOAD_MAP', Object.keys(preloads))
await lua.doString(`
  for _, name in ipairs(__PRELOAD_MAP) do
    package.preload[name] = function()
      local src = _G['__PRELOAD_' .. name]
      local chunk = assert(load(src, '@' .. name .. '.lua', 't', _ENV))
      return chunk()
    end
  end
`)

// tokenize all three demo scripts
for (const f of ['full_pipeline_demo.ks', 'galgame_demo.ks', 'sma_demo.ks']) {
  const ks = readFileSync(new URL('../demo/' + f, import.meta.url), 'utf8')
  lua.global.set('KS_SRC', ks)
  const out = await lua.doString(`
    local tokenizer = require('tokenizer')
    local tokens = tokenizer.parse(KS_SRC)
    local counts = {}
    for _, t in ipairs(tokens) do counts[t.type] = (counts[t.type] or 0) + 1 end
    local parts = {}
    for k, v in pairs(counts) do parts[#parts + 1] = k .. '=' .. v end
    table.sort(parts)
    return string.format('%d tokens [%s]', #tokens, table.concat(parts, ', '))
  `)
  console.log(f + ':', out)
}
console.log('SPIKE1 PASS: tokenizer + lpeg run unmodified under wasmoon')
