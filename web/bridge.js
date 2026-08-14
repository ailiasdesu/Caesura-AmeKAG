// G5 path-B web player — wasmoon bridge.
// Loads the pure-Lua KAG stack + the REAL kag command table, wires the
// AdapterCore as the binding surface, and runs .ks scenes.
import { Lua } from 'wasmoon'
import { AdapterCore, LAYER_TYPE } from './adapter-core.js'

// modules whose Lua source lives in scripts/ (NOT bindings)
const BINDING_MODULES = new Set([
  'backend', 'layers', 'audio', 'rtt', 'blend', 'transition', 'transform',
  'vfx', 'flow', 'replay', 'mods', 'pool', 'config', 'system', 'i18n',
  'settings', 'gallery', 'music_room', 'title_menu', 'saveload_menu',
  'chapter_select', 'dev_hud', 'history_ui', 'toast', 'ks_i18n',
  'fileutil', 'sandbox',
])

export async function createPlayer({ scriptsBase, fetchImpl = fetch }) {
  const factory = await Lua.load()
  const lua = factory.createState()
  const core = new AdapterCore()

  // ---- collect scripts/*.lua via HTTP ----
  const entries = await (await fetchImpl(scriptsBase + 'index.json')).json()
  const preloads = {}
  for (const name of Object.keys(entries)) {
    if (BINDING_MODULES.has(name)) continue
    let src = await (await fetchImpl(scriptsBase + name.replaceAll('.', '/') + '.lua')).text()
    if (src.charCodeAt(0) === 0xfeff) src = src.slice(1)
    preloads[name] = src
  }
  lua.global.set('__PRELOAD_MAP', Object.keys(preloads))
  for (const [name, src] of Object.entries(preloads)) {
    lua.global.set('__PRELOAD_' + name.replaceAll('.', '_'), src)
  }
  await lua.doString(`
    for _, name in ipairs(__PRELOAD_MAP) do
      local safe = name:gsub('%.', '_')
      package.preload[name] = function()
        local src = _G['__PRELOAD_' .. safe]
        return assert(load(src, '@' .. name .. '.lua', 't', _ENV))()
      end
    end
  `)

  // ---- JS binding adapter (backend.* / layers.*) ----
  const jsLayers = {
    Type: { LAYER_NORMAL: 0, LAYER_MESSAGE: 1, LAYER_BG: 2, LAYER_FG: 3 },
    get: (id) => core.getLayer(id) ?? null,
    find: (id) => core.getLayer(id) ?? null,
    ensure: (id, opts) => core.ensureLayer(id, opts),
    get_root: () => 0,
    add_layer: (parent, opts) => {
      const name = (opts && opts.name) || ('layer' + (core._seq + 1))
      return core.ensureLayer(name, opts)
    },
    bg: (name) => core.ensureLayer(name ?? 'bg', { layerType: 2, z: 0 }),
    fg: (name) => core.ensureLayer(name ?? 'fg', { layerType: 3, z: 1 }),
    set_layer_image: (node, tex) => core.setLayerImage(node, tex),
    set_layer_visible: (node, v) => core.setLayerVisible(node, v),
    set_layer_opacity: (node, v) => core.setLayerOpacity(node, v),
    set_z: (node, z) => core.setLayerZ(node, z),
    move_layer: (node, x, y) => core.moveLayer(node, x, y),
    set_layer_blend: () => {}, set_position: (n, x, y) => core._log('layer.position', { name: String(n), x, y }), set_options: () => {},
    fade_to: () => {}, mark_dirty: () => {}, get_layer: (id) => core.getLayer(id) ?? null,
  }
  const jsBackend = {
    close: () => {}, audio_play: (kind, file, opts) => { core.audioPlay(String(kind), String(file), opts && opts.volume); const e = core.audioBus[String(kind)]; if (e) e._startFrame = core._frame ?? 0; return 1 },
    audio_stop: (kind) => core.audioStop(String(kind)), audio_xfade: () => {}, audio_is_playing: (kind) => { core.tick(); return core.audioIsPlaying(String(kind)) },
    audio_set_bus_volume: () => {}, audio_fade_volume: () => {},
    load_texture: (f) => {
      // resolve via mods.resolve-like identity; fetch metadata in browser
      const id = core.loadTexture(f)
      return id
    },
    load_texture_async: (f) => core.loadTexture(f),
    create_solid_texture: () => 0, destroy_texture: (id) => core.destroyTexture(id),
    font_render_text: () => 1, font_clear: () => {}, line_height: () => 22,
    render_text: (t) => { core.setText(t ?? ''); return 1 }, clear_text: () => core.clearText(),
    text_render_ruby: () => {}, text_set_font: () => {}, text_reset_state: () => {},
    create_lut_texture: () => 0, render_frame: () => {}, set_screen_offset: () => {},
    particles_create_emitter: () => 0, particles_emit: () => {}, particles_destroy_emitter: () => {}, clear_particles: () => {},
    video_stop: () => {}, video_play: () => 0, video_is_playing: () => false,
    ai_available: () => false, ai_query_async: () => {}, ai_cancel: () => {},
  }
  const jsStubs = {
    audio: { lua: () => {} }, rtt: { create: () => 0, destroy: () => {}, bind: () => {} },
    blend: { lua: () => {} }, transition: { lua: () => {}, start: () => {}, is_active: () => false },
    transform: { lua: () => {} }, vfx: { lua: () => {}, flash: () => {} },
    flow: { scene_cache: () => {}, load_scene: () => {} },
    replay: { save: () => {}, event_count: () => 0, load: () => {}, set_mode: () => {} },
    pool: {}, config: { ai: () => {} }, system: { lua: () => {} },
    settings: {}, gallery: {}, music_room: {}, title_menu: {}, saveload_menu: {},
    chapter_select: {}, dev_hud: {}, history_ui: {}, toast: {}, ks_i18n: {}, fileutil: {}, sandbox: {},
    mods: { resolve: (p) => p, register: () => {}, list: () => ({}) },
    i18n: { localize: (s) => s, current: '', t: (s) => s },
  }
  for (const [k, v] of Object.entries(jsStubs)) lua.global.set(k, v)
  lua.global.set('backend', jsBackend)
  lua.global.set('layers', jsLayers)
  await lua.doString(`
    backend = _G['backend']
    layers = _G['layers']
    for _, name in ipairs({'backend','layers','audio','rtt','blend','transition','transform','vfx','flow','replay','pool','config','system','settings','gallery','music_room','title_menu','saveload_menu','chapter_select','dev_hud','history_ui','toast','ks_i18n','fileutil','sandbox','mods','i18n'}) do
      package.loaded[name] = _G[name]
    end
  `)

  // ---- load the real kag command table ----
  await lua.doString(`local kag = require('kag')`)

  return {
    lua,
    core,
    /** Run a .ks source; resolves with final ctx.token_index. */
    /** Current scene coroutine state (persisted across advances). */
    _co: null,
    _ctx: null,
    /** Start a scene; resolves DONE:n/m or WAIT:idx (parked at [p]). */
    /** Start + drive a scene to completion or first WAIT. */
    async runScene(ksSrc, sceneName = 'scene.ks', opts = {}) {
      const maxFrames = opts.maxFrames ?? 200000
      lua.global.set('KS_SRC', ksSrc)
      lua.global.set('__CLICK', false)
      lua.global.set('__AUTOCLICK', !!opts.autoClick)
      const out = await lua.doString(`
        local tokenizer = require('tokenizer')
        local scheduler = require('scheduler')
        local tokens = tokenizer.parse(KS_SRC)
        local ctx = {
          f = {}, sf = {}, tf = {}, mp = {}, lf = {},
          variables = {}, current_scene = '${sceneName}',
          token_index = 1, tokens = tokens,
          text_state = {}, layer_state = {}, audio_state = {},
          macro_args = {}, call_stack = {}, flag_stack = {},
        }
        local co = coroutine.create(function()
          local ok, err = pcall(function() scheduler.run(ctx, tokens, 1) end)
          if not ok then error(err) end
        end)
        local frames = 0
        local clicks = 0
        local result = ''
        while true do
          local status = coroutine.status(co)
          if status == 'dead' then result = 'DONE:' .. tostring(ctx.token_index) .. ':' .. tostring(clicks) break end
          frames = frames + 1
          if frames > ${maxFrames} then result = 'ERR:frame-limit@' .. tostring(ctx.token_index) .. ':' .. tostring((ctx.tokens and ctx.tokens[ctx.token_index] and (ctx.tokens[ctx.token_index].cmd or ctx.tokens[ctx.token_index].type)) or '?') break end
          if ctx.waiting_input then
            if not __CLICK then
              if __AUTOCLICK and clicks < 100 then __CLICK = true else
                result = 'WAIT:' .. tostring(ctx.token_index) break
              end
            end
            ctx.waiting_input = false
            __CLICK = false
            clicks = clicks + 1
          end
          local r = { coroutine.resume(co, 16) }
          if not r[1] then result = 'ERR:' .. tostring(r[2]) break end
        end
        return result
      `)
      return out
    },
    /** Raise the click signal for the next runScene. */
    async click() { lua.global.set('__CLICK', true) },
    /** Pre-resolve texture ids to URLs (browser: asset URLs). */
    async linkTextures(assetUrl) {
      for (const [id, t] of core.textures) {
        const u = assetUrl(t.path)
        core.textures.set(id, { ...t, url: u })
      }
    },
    LAYER_TYPE,
  }
}
