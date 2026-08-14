// G5 path-B spike 3: full scheduler execution under wasmoon.
// Loads the REAL kag/commands/* handlers with a stub binding surface;
// runs a demo .ks from tokenize -> compile -> scheduler.run.
import { Lua } from 'wasmoon'
import { readFileSync, readdirSync } from 'node:fs'

const factory = await Lua.load()
const lua = factory.createState()

// --- module loader: reads scripts/ files into package.preload ---
const SCRIPTS = new URL('../scripts/', import.meta.url)
const mods = {}

function collect(dir, prefix) {
  for (const f of readdirSync(new URL(dir, SCRIPTS), { withFileTypes: true })) {
    if (f.isDirectory()) collect(dir + f.name + '/', prefix + f.name + '.')
    else if (f.name.endsWith('.lua')) mods[prefix + f.name.slice(0, -4)] = dir + f.name
  }
}
collect('', '')

// preload ALL pure-Lua modules; bindings (backend/layers/audio/rtt/...) are stubbed
const bindingNames = ['backend', 'layers', 'audio', 'rtt', 'blend', 'transition', 'transform', 'vfx', 'flow', 'replay', 'mods', 'pool', 'config', 'system', 'i18n', 'settings', 'gallery', 'music_room', 'title_menu', 'saveload_menu', 'chapter_select', 'dev_hud', 'history_ui', 'toast', 'ks_i18n', 'fileutil', 'sandbox']
const skip = new Set([...bindingNames, 'main'])

for (const [name, rel] of Object.entries(mods)) {
  if (skip.has(name)) continue
  let src = readFileSync(new URL(rel, SCRIPTS), 'utf8')
  if (src.charCodeAt(0) === 0xfeff) src = src.slice(1)
  lua.global.set('__PRELOAD_' + name.replaceAll('.', '_'), src)
}
lua.global.set('__PRELOAD_MAP', Object.keys(mods).filter((n) => !skip.has(n)))
await lua.doString(`
  local found_mods = __PRELOAD_MAP[1] == 'mods' or false
  for _, name in ipairs(__PRELOAD_MAP) do
    if name == 'mods' then found_mods = true end
    local safe = name:gsub('%.', '_')
    package.preload[name] = function()
      local src = _G['__PRELOAD_' .. safe]
      local chunk = assert(load(src, '@' .. name .. '.lua', 't', _ENV))
      return chunk()
    end
  end
`)

// --- stub binding modules (no-ops; log calls) ---
await lua.doString(`
  local function stub(mod, fns)
    local t = {}
    for _, fn in ipairs(fns) do t[fn] = function(...) end end
    package.loaded[mod] = t
    _G[mod] = t
  end
  stub('backend', {'close','audio_play','audio_stop','audio_xfade','audio_is_playing','audio_set_bus_volume','load_texture','destroy_texture','render_text','clear_text','font_render_text','font_clear','create_solid_texture','render_frame','set_screen_offset','line_height','video_stop','video_play','video_is_playing','create_lut_texture','text_set_font','text_reset_state','particles_create_emitter','particles_emit','particles_destroy_emitter','clear_particles','ai_available','ai_query_async','ai_cancel','load_texture_async','audio_fade_volume','font_render_text','text_render_ruby'})
  stub('layers', {'find','get_root','add_layer','bg','fg','set_layer_image','set_layer_visible','set_z','move_layer','set_layer_opacity','set_layer_blend','set_position','set_options','get','fade_to','ensure','mark_dirty','get_layer','Type'})
  stub('audio', {'lua'})
  stub('rtt', {'create','destroy','bind'})
  stub('blend', {'lua'})
  stub('transition', {'lua','start'})
  stub('transform', {'lua'})
  stub('vfx', {'lua','flash'})
  stub('flow', {'scene_cache','load_scene'})
  stub('replay', {'save','event_count','load','set_mode'})
  stub('pool', {})
  stub('config', {'ai'})
  stub('system', {'lua'})
  stub('settings', {})
  stub('gallery', {})
  stub('music_room', {})
  stub('title_menu', {})
  stub('saveload_menu', {})
  stub('chapter_select', {})
  stub('dev_hud', {})
  stub('history_ui', {})
  stub('toast', {})
  stub('ks_i18n', {})
  stub('fileutil', {})
  stub('sandbox', {})
`)

// --- run: tokenize + compile + scheduler.run over galgame_demo.ks ---
const ks = readFileSync(new URL('../demo/galgame_demo.ks', import.meta.url), 'utf8')
lua.global.set('KS_SRC', ks)
const out = await lua.doString(`
  local tokenizer = require('tokenizer')
  local tokens = tokenizer.parse(KS_SRC)
  local scheduler = require('scheduler')
  local ctx = {
    f = {}, sf = {}, tf = {}, mp = {}, lf = {},
    variables = {}, current_scene = 'galgame_demo.ks',
    token_index = 1, tokens = tokens,
    text_state = {}, layer_state = {}, audio_state = {},
    macro_args = {}, call_stack = {}, flag_stack = {},
  }
  local ok, err = pcall(function()
    scheduler.run(ctx, tokens, 1)
  end)
  if not ok then return 'SCHEDULER ERROR: ' .. tostring(err) end
  return string.format('SCHEDULER OK: %d tokens, token_index=%d', #tokens, ctx.token_index)
`)
console.log(out)
console.log(out.startsWith('SCHEDULER OK') ? 'SPIKE3 PASS: scheduler executes demo scene under wasmoon with stubbed bindings' : 'SPIKE3 PARTIAL: ' + out)
