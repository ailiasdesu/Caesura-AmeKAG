// G5 path-B spike 4: REAL kag command table + JS binding adapter.
// Loads kag.lua (command registry) + all pure-Lua modules; backend.* /
// layers.* / audio.* etc. are JS functions that LOG calls. Runs the demo
// scene and reports which commands actually execute and their binding calls.
import { Lua } from 'wasmoon'
import { readFileSync, readdirSync } from 'node:fs'

const factory = await Lua.load()
const lua = factory.createState()

// ---------- collect all scripts/*.lua ----------
const SCRIPTS = new URL('../scripts/', import.meta.url)
const mods = {}
function collect(dir, prefix) {
  for (const f of readdirSync(new URL(dir, SCRIPTS), { withFileTypes: true })) {
    if (f.isDirectory()) collect(dir + f.name + '/', prefix + f.name + '.')
    else if (f.name.endsWith('.lua')) mods[prefix + f.name.slice(0, -4)] = dir + f.name
  }
}
collect('', '')

// C++ binding modules: stubbed in JS below (NOT loaded from scripts/).
const bindingNames = new Set(['backend','layers','audio','rtt','blend','transition','transform','vfx','flow','replay','mods','pool','config','system','i18n','settings','gallery','music_room','title_menu','saveload_menu','chapter_select','dev_hud','history_ui','toast','ks_i18n','fileutil','sandbox'])

// Preload every OTHER module (pure Lua).
for (const [name, rel] of Object.entries(mods)) {
  if (bindingNames.has(name)) continue
  let src = readFileSync(new URL(rel, SCRIPTS), 'utf8')
  if (src.charCodeAt(0) === 0xfeff) src = src.slice(1)
  lua.global.set('__PRELOAD_' + name.replaceAll('.', '_'), src)
}
lua.global.set('__PRELOAD_MAP', Object.keys(mods).filter((n) => !bindingNames.has(n)))
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

// ---------- JS binding adapter ----------
const callLog = []
const jsBackend = {
  close: () => {}, audio_play: (f) => { callLog.push(['backend.audio_play', f]); return 1 }, audio_stop: () => {}, audio_xfade: () => {}, audio_is_playing: () => false, audio_set_bus_volume: (b, v) => {}, audio_fade_volume: () => {},
  load_texture: (f) => { callLog.push(['backend.load_texture', f]); return 1 }, create_solid_texture: () => 1, load_texture_async: (f) => { callLog.push(['backend.load_texture_async', f]); return 1 }, destroy_texture: () => {},
  font_render_text: () => 1, font_clear: () => {}, line_height: () => 24, render_text: () => {}, clear_text: () => {}, text_render_ruby: () => {}, text_set_font: () => {}, text_reset_state: () => {},
  create_lut_texture: () => 1, render_frame: () => {}, set_screen_offset: () => {}, particles_create_emitter: () => 1, particles_emit: () => {}, particles_destroy_emitter: () => {}, clear_particles: () => {},
  video_stop: () => {}, video_play: (f) => { callLog.push(['backend.video_play', f]); return 1 }, video_is_playing: () => false,
  ai_available: () => false, ai_query_async: () => {}, ai_cancel: () => {},
}
// in-memory layer tree so handlers can look up/update nodes
const layerNodes = new Map()
let layerSeq = 1
function layerNode(id) {
  let n = layerNodes.get(id)
  if (!n) { n = { id: layerSeq++, name: id, opacity: 1, z: 0, visible: true, image: null, Type: 1 }; layerNodes.set(id, n) }
  return n
}
const jsLayers = {
  Type: { NODE: 1, SPRITE: 2, TEXT: 3, IMAGE: 4 },
  find: (id) => layerNode(id),
  get: (id) => layerNodes.get(id) ?? null,
  get_root: () => 1,
  add_layer: (parent, opts) => {
    const name = (opts && opts.name) || ('layer' + layerSeq)
    const n = layerNode(name)
    if (opts) { for (const [k, v] of Object.entries(opts)) if (typeof v !== 'object') n[k] = v }
    return n
  },
  bg: () => layerNode('bg'), fg: () => layerNode('fg'),
  set_layer_image: (l, f) => { callLog.push(['layers.set_layer_image', String(f)]) },
  set_layer_visible: (l, v) => { if (l && l.id) { layerNode(l.id).visible = v } },
  set_z: () => {}, move_layer: () => {}, set_layer_opacity: (l, v) => { if (l && l.id) layerNode(l.id).opacity = v },
  set_layer_blend: () => {}, set_position: () => {}, set_options: () => {},
  fade_to: () => {}, ensure: (id) => layerNode(id), mark_dirty: () => {}, get_layer: (id) => layerNode(id),
}
const jsStubs = {
  audio: { lua: (code) => callLog.push(['audio.lua', String(code).slice(0, 60)]) },
  rtt: { create: () => 1, destroy: () => {}, bind: () => {} },
  blend: { lua: () => {} }, transition: { lua: () => {}, start: () => {}, is_active: () => false }, transform: { lua: () => {} }, vfx: { lua: () => {}, flash: () => {} },
  flow: { scene_cache: () => {}, load_scene: () => {} }, replay: { save: () => {}, event_count: () => 0, load: () => {}, set_mode: () => {} },
  pool: {}, config: { ai: () => {} }, system: { lua: (code) => callLog.push(['system.lua', String(code).slice(0, 60)]) },
  settings: {}, gallery: {}, music_room: {}, title_menu: {}, saveload_menu: {}, chapter_select: {}, dev_hud: {}, history_ui: {}, toast: {}, ks_i18n: {}, fileutil: {}, sandbox: {},
  mods: { resolve: (p) => p, register: () => {}, list: () => ({}) }, i18n: { localize: (s) => s, current: '', t: (s) => s },
}
// register in Lua
await lua.doString(`
  backend = nil; layers = nil
`)
lua.global.set('backend', jsBackend)
lua.global.set('layers', jsLayers)
for (const [k, v] of Object.entries(jsStubs)) lua.global.set(k, v)
await lua.doString(`
  backend = _G['backend']; layers = _G['layers']
  for _, name in ipairs({'backend','layers','audio','rtt','blend','transition','transform','vfx','flow','replay','pool','config','system','settings','gallery','music_room','title_menu','saveload_menu','chapter_select','dev_hud','history_ui','toast','ks_i18n','fileutil','sandbox','mods','i18n'}) do
    package.loaded[name] = _G[name]
  end
`)

// ---------- load the REAL kag command table ----------
try {
  await lua.doString(`local kag = require('kag')`)
  console.log('kag table loaded')
} catch (e) {
  console.log('kag load error:', String(e).split('\n').slice(0, 4).join(' | '))
  process.exit(1)
}

// ---------- run the demo scene ----------
const ks = readFileSync(new URL('../demo/galgame_demo.ks', import.meta.url), 'utf8')
lua.global.set('KS_SRC', ks)
const out = await lua.doString(`
  local tokenizer = require('tokenizer')
  local scheduler = require('scheduler')
  local kag = require('kag')
  local tokens = tokenizer.parse(KS_SRC)
  local ctx = {
    f = {}, sf = {}, tf = {}, mp = {}, lf = {},
    variables = {}, current_scene = 'galgame_demo.ks',
    token_index = 1, tokens = tokens,
    text_state = {}, layer_state = {}, audio_state = {},
    macro_args = {}, call_stack = {}, flag_stack = {},
  }
  local errors = {}
  local ok = pcall(function()
    scheduler.run(ctx, tokens, 1)
  end)
  return string.format('ok=%s tokens=%d final_index=%d', tostring(ok), #tokens, ctx.token_index)
`)
console.log('run:', out)
console.log('binding calls:', callLog.length, '| sample:', callLog.slice(0, 6).map((c) => c.join('=')).join(', '))
console.log('SPIKE4 ' + (out.startsWith('ok=true') ? 'PASS' : 'PARTIAL') + ': real kag command table under wasmoon with JS adapter')
