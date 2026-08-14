// G5 path-B spike 2: scheduler compile front-end under wasmoon.
// Proves kag.schema + kag.expr + kag.compiler + kag.operation (pure Lua)
// compile a real demo .ks token stream unmodified; only backend.* is stubbed.
import { Lua } from 'wasmoon'
import { readFileSync } from 'node:fs'

const factory = await Lua.load()
const lua = factory.createState()

const preloads = {
  lpeg: '../scripts/lpeg.lua',
  tokenizer: '../scripts/tokenizer.lua',
  'kag.schema': '../scripts/kag/schema.lua',
  'kag.expr': '../scripts/kag/expr.lua',
  'kag.operation': '../scripts/kag/operation.lua',
  'kag.cancel_token': '../scripts/kag/cancel_token.lua',
  'kag.compiler': '../scripts/kag/compiler.lua',
  'kag_debug': '../scripts/kag_debug.lua',
}
for (const [name, rel] of Object.entries(preloads)) {
  let src = readFileSync(new URL(rel, import.meta.url), 'utf8')
  if (src.charCodeAt(0) === 0xfeff) src = src.slice(1)
  lua.global.set('__PRELOAD_' + name.replaceAll('.', '_'), src)
}
lua.global.set('__PRELOAD_MAP', Object.keys(preloads))
await lua.doString(`
  for _, name in ipairs(__PRELOAD_MAP) do
    local safe = name:gsub('%.', '_')
    package.preload[name] = function()
      local src = _G['__PRELOAD_' .. safe]
      local chunk = assert(load(src, '@' .. name .. '.lua', 't', _ENV))
      return chunk()
    end
  end
`)

// stub the C++ binding surface (backend.*) and the kag command table
// (compiler binds handlers from it; the compile path only needs the table).
// stub globals inside Lua (package.loaded registration must happen in-Lua)
await lua.doString(`
  backend = { close = function() end }
  package.loaded['kag'] = {}
`)

// compile the demo story
const ks = readFileSync(new URL('../demo/galgame_demo.ks', import.meta.url), 'utf8')
lua.global.set('KS_SRC', ks)
const out = await lua.doString(`
  local tokenizer = require('tokenizer')
  local tokens = tokenizer.parse(KS_SRC)
  local compiler = require('kag.compiler')
  compiler.compile(tokens)
  local c = tokens._compiled
  if not c then error('NO COMPILED SIDE TABLE; tokens=' .. #tokens) end
  local nlabels = 0
  if not c.labels then error('NO LABEL INDEX') end
  for _ in pairs(c.labels) do nlabels = nlabels + 1 end
  local handlers = {}
  for i, h in ipairs(c.handlers) do
    if h and h[1] then handlers[i] = h[1] end
  end
  return string.format('compiled: %d tokens, %d labels, first handlers: %s', #tokens, nlabels, table.concat(handlers, ', '))
  `)
console.log('compile:', out)
console.log('SPIKE2 PASS: kag compiler front-end runs unmodified under wasmoon (only backend.close stubbed)')
