// G5 path-B web player — wasmoon bridge.
// Loads the pure-Lua KAG stack + the REAL kag command table, wires the
// AdapterCore as the binding surface, and runs .ks scenes.
import { Lua } from 'wasmoon'
import { AdapterCore, LAYER_TYPE } from './adapter-core.js'
import { AudioEngine } from './audio-engine.js'

// modules whose Lua source lives in scripts/ (NOT bindings)
// layers/audio/etc are C++-binding modules stubbed in JS below (the real
// scripts/layers.lua pulls rtt/view_id/RTT pools — too heavy for the web
// adapter; the stub honors its Type enum and node contract).
function makeDefaultStorage() {
  if (typeof localStorage !== 'undefined' && localStorage) {
    return {
      get: (k) => { try { return localStorage.getItem(k) } catch { return null } },
      set: (k, v) => { try { localStorage.setItem(k, v); return true } catch { return false } },
      del: (k) => { try { localStorage.removeItem(k) } catch { /* noop */ } },
    }
  }
  // Node/jsdom fallback: process-lifetime memory store.
  const mem = new Map()
  return {
    get: (k) => (mem.has(k) ? mem.get(k) : null),
    set: (k, v) => { mem.set(k, v); return true },
    del: (k) => { mem.delete(k) },
  }
}


// JSON-safe value -> Lua literal source (bracketed keys + escaped strings).
// Round 46: the web save bridge parses stored state back into real Lua
// tables with load('return ' .. literal) — wasmoon's userdata proxies are
// rejected by the engine's load handler (type(state) ~= 'table').
function luaLiteralValue(v) {
  if (v === null || v === undefined) return 'nil'
  if (typeof v === 'number') return Number.isFinite(v) ? String(v) : 'nil'
  if (typeof v === 'boolean') return v ? 'true' : 'false'
  if (typeof v === 'string') {
    let out = '"'
    for (const ch of v) {
      const code = ch.codePointAt(0)
      if (ch === '\\') out += '\\\\'
      else if (ch === '"') out += '\\"'
      else if (ch === '\n') out += '\\n'
      else if (ch === '\r') out += '\\r'
      else if (ch === '\t') out += '\\t'
      else if (code < 32) out += '\\' + code
      else out += ch
    }
    return out + '"'
  }
  if (Array.isArray(v)) return '{' + v.map(luaLiteralValue).join(',') + '}'
  if (typeof v === 'object') {
    const parts = []
    for (const k of Object.keys(v)) {
      const key = /^[A-Za-z_][A-Za-z0-9_]*$/.test(k) ? k : '[' + luaLiteralValue(k) + ']'
      parts.push(key + '=' + luaLiteralValue(v[k]))
    }
    return '{' + parts.join(',') + '}'
  }
  return 'nil'
}

const BINDING_MODULES = new Set([
  'backend', 'layers', 'audio', 'rtt', 'blend', 'transition', 'transform',
  'vfx', 'flow', 'replay', 'pool', 'config', 'system',
  'settings', 'gallery', 'music_room', 'title_menu', 'saveload_menu',
  'chapter_select', 'dev_hud', 'history_ui', 'toast', 'ks_i18n',
  'fileutil', 'sandbox',
])

export async function createPlayer({ scriptsBase, fetchImpl = fetch, wasmFile, audioAssetUrl = (p) => '/assets/' + p, storageBackend }) {
  const factory = await Lua.load(wasmFile ? { wasmFile } : undefined)
  const lua = factory.createState()
  const core = new AdapterCore()
  const audio = new AudioEngine()

  // ---- save storage backend (round 46) ----
  // Desktop engine persists saves through C++ SaveManager; the web player
  // bridges [save]/[load] to a KV store. Default: browser localStorage;
  // tests inject an in-memory backend (jsdom lacks a shared origin).
  const storage = storageBackend ?? makeDefaultStorage()
  const saveKey = (slot) => 'caesura.save.' + String(slot)

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

  // ---- JS binding adapter (backend.* + layers.*) ----
  // Type enum mirrors scripts/layers.lua (LAYER_BASE=1 ... LAYER_MESSAGE=6).
  const jsLayers = {
    Type: { LAYER_BASE: 1, LAYER_LAYER0: 2, LAYER_LAYER1: 3, LAYER_FORE: 4, LAYER_UI: 5, LAYER_MESSAGE: 6, LAYER_EFFECT: 7 },
    get: (id) => core.getLayer(id) ?? null,
    find: (id) => core.getLayer(id) ?? null,
    ensure: (id, opts) => core.ensureLayer(id, opts),
    get_root: () => 0,
    add_layer: (parent, opts) => {
      const name = (opts && opts.name) || ('layer' + (core._seq + 1))
      return core.ensureLayer(name, opts)
    },
    bg: (name) => core.ensureLayer(name ?? 'bg', { layer_type: 1, z: 0 }),
    fg: (name) => core.ensureLayer(name ?? 'fg', { layer_type: 4, z: 1 }),
    set_layer_image: (node, tex) => core.setLayerImage(node, tex),
    set_layer_visible: (node, v) => core.setLayerVisible(node, v),
    set_layer_opacity: (node, v) => core.setLayerOpacity(node, v),
    set_z: (node, z) => core.setLayerZ(node, z),
    move_layer: (node, x, y) => core.moveLayer(node, x, y),
    set_layer_blend: () => {}, set_position: (n, x, y) => core._log('layer.position', { name: String(n), x, y }), set_options: () => {},
    fade_to: () => {}, mark_dirty: () => {}, get_layer: (id) => core.getLayer(id) ?? null,
  }

  // ---- KAG binding surface (C++ bindings in the real engine) ----
  // Save/load go through KAG.save_game / KAG.load_game in the desktop
  // engine; the web player has no SaveManager, so expose honest stubs
  // that return nil (engine-side handlers degrade to "error" results
  // instead of crashing on a nil call).
  const jsKAG = {
    // Serialize the engine state table (Lua -> JS object, JSON-safe) into
    // the storage backend. Returns truthy on success (engine sets
    // tf.save_result = "ok"), falsy on failure ("error").
    save_game: (slot, state, sceneName, tokenIdx, thumbnail) => {
      try {
        // Normalize the scene path inside the captured state into the
        // engine's allowlisted form (SaveCommands.load validates
        // state.scene_path with _safeScenePath — bare web basenames
        // would be rejected and [load] would never set _pendingLoadScene).
        const st = state && typeof state === 'object' ? { ...state } : {}
        let scene = String(sceneName ?? '')
        if (scene.length > 0 && !scene.includes('/') && !scene.startsWith('demo/')) {
          scene = 'demo/' + scene
        }
        if (typeof st.scene_path === 'string' && st.scene_path.length > 0
            && !st.scene_path.includes('/')) {
          st.scene_path = 'demo/' + st.scene_path
        }
        const payload = JSON.stringify({
          state: st,
          scene,
          token: Number(tokenIdx) || 1,
          thumbnail: String(thumbnail ?? ''),
          savedAt: Date.now(),
        })
        const ok = storage.set(saveKey(slot), payload)
        core._log('save.write', { slot: Number(slot), ok: !!ok })
        return ok
      } catch (e) {
        core._log('save.error', { slot: Number(slot), error: String(e) })
        return null
      }
    },
    // Raw read used by the Lua-side load_game wrapper. Returns the
    // stored state as a Lua literal string (the wrapper parses it with
    // load('return ' .. s) into a real table).
    _load_raw: (slot) => {
      const raw = storage.get(saveKey(slot))
      core._log('save.read', { slot: Number(slot), found: raw != null })
      if (raw == null) return null
      try {
        const payload = JSON.parse(raw)
        return luaLiteralValue(payload.state ?? {})
      } catch {
        return null
      }
    },
    capture_thumbnail: () => null,
  }
  const jsBackend = {
    // audio_play routes through the real WebAudio engine when available;
    // the core state machine always records the call (telemetry + fallback).
    audio_play: (kind, file, opts) => {
      const k = String(kind), f = String(file)
      core.audioPlay(k, f, opts && opts.volume)
      void audio.play(k, f, { volume: opts && opts.volume, assetUrl: audioAssetUrl })
      return 1
    },
    audio_stop: (kind) => { const k = String(kind); core.audioStop(k); audio.stop(k) },
    audio_xfade: () => {},
    audio_is_playing: (kind) => { core.tick(); return core.audioIsPlaying(String(kind)) },
    audio_set_bus_volume: (kind, v) => { core.audioSetBusVolume(String(kind), v); audio.setBusVolume(String(kind), v) },
    audio_fade_volume: () => {},
    load_texture: (f) => {
      // resolve via mods.resolve-like identity; fetch metadata in browser
      const id = core.loadTexture(f)
      return id
    },
    load_texture_async: (f) => core.loadTexture(f),
    create_solid_texture: () => 0, destroy_texture: (id) => core.destroyTexture(id),
    font_render_text: () => 1, font_clear: () => {}, line_height: () => 22,
    // Lua contract: render_text(text, x, y, r, g, b, a) — the text is the
    // first arg; x/y/r/g/b/a are render coordinates we ignore in the DOM
    // overlay (TextScene/add_wrapped_spans drive the real layout).
    render_text: (...args) => { const t = args[0]; if (typeof t === 'string') core.appendText(t); return 1 }, clear_text: () => core.clearText(),
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
    settings: {},
    // System-UI modules: the desktop engine opens overlay screens; the web
    // player has no overlay runtime, so every entry point is a safe no-op
    // (engine handlers call show(ctx)/_hideAll(ctx) — nil would crash).
    gallery: { show: () => {}, _hideAll: () => {} },
    music_room: { show: () => {}, _hideAll: () => {} },
    history_ui: { show: () => {}, _hideAll: () => {} },
    title_menu: { show: () => {}, _hideAll: () => {} },
    saveload_menu: { show: () => {}, _hideAll: () => {} },
    chapter_select: { show: () => {}, _hideAll: () => {} },
    dev_hud: { show: () => {}, _hideAll: () => {} },
    toast: { show: () => {}, _hideAll: () => {} },
    ks_i18n: {}, fileutil: {}, sandbox: {},
    mods: { resolve: (p) => p, register: () => {}, list: () => ({}) },
    i18n: { localize: (s) => s, current: '', t: (s) => s },
  }
  for (const [k, v] of Object.entries(jsStubs)) lua.global.set(k, v)
  lua.global.set('backend', jsBackend)
  lua.global.set('layers', jsLayers)
  lua.global.set('KAG', jsKAG)
  await lua.doString(`
    backend = _G['backend']
    layers = _G['layers']
    for _, name in ipairs({'backend','layers','audio','rtt','blend','transition','transform','vfx','flow','replay','pool','config','system','settings','gallery','music_room','title_menu','saveload_menu','chapter_select','dev_hud','history_ui','toast','ks_i18n','fileutil','sandbox','mods','i18n'}) do
      package.loaded[name] = _G[name]
    end
  `)

  // ---- load the real kag command table ----
  await lua.doString(`local kag = require('kag')`)

  // ---- web save/load bridge (round 46) ----
  // [save] already works through jsKAG.save_game. [load] needs a real
  // Lua table back, but wasmoon returns JS objects as userdata proxies
  // (type(v) ~= 'table'), which the engine's load handler rejects. So
  // wrap load_game in Lua: read the raw JSON via the JS stub, then
  // parse it with load() (JSON is valid Lua literal syntax) into a real
  // table. This mirrors ks_bake's encode_lua_literal round-trip.
  await lua.doString([
    // Wrap KAG in a pure-Lua table: kag_binding reads _G.KAG[name], and a
    // function stored on the JS proxy would have its return values marshaled
    // back through the bridge as userdata (type ~= 'table' breaks load).
    // A Lua-side table holding the JS functions + our load_game wrapper
    // keeps the load_game return value a genuine Lua table.
    'local _kag_js = _G.KAG',
    'local _kag_json_to_table = function(s)',
    "  local f, err = load('return ' .. s, '@save.json', 't', _ENV)",
    '  if not f then return nil, err end',
    '  local ok, t = pcall(f)',
    "  if not ok or type(t) ~= 'table' then return nil, 'corrupt save' end",
    '  return t',
    'end',
    'local _kag = {}',
    'for k, v in pairs(_kag_js) do _kag[k] = v end',
    '_kag.load_game = function(slot)',
    '  local raw = _kag._load_raw(slot)',
    "  if raw == nil then return nil, 'no save in slot' end",
    "  if type(raw) ~= 'string' then return nil, 'bad payload' end",
    '  local t, err = _kag_json_to_table(raw)',
    "  if not t then return nil, err or 'corrupt' end",
    "  return t, 'ok'",
    'end',
    '_G.KAG = _kag',
  ].join(String.fromCharCode(10)))

  return {
    lua,
    core,
    audio,
    /** Run a .ks source; resolves with final ctx.token_index. */
    /** Current scene coroutine state (persisted across advances). */
    _co: null,
    _ctx: null,
    /** Start a scene; resolves DONE:n/m or WAIT:idx (parked at [p]). */
    /** Start + drive a scene to completion or first WAIT. */
    async runScene(ksSrc, sceneName = 'scene.ks', opts = {}) {
      const maxFrames = opts.maxFrames ?? 200000
      lua.global.set('KS_SRC', ksSrc)
      lua.global.set('__SCENE_SOURCES', opts.sceneSources ?? {})
      lua.global.set('__CLICK', false)
      lua.global.set('__AUTOCLICK', !!opts.autoClick)
      const out = await lua.doString(`
        local tokenizer = require('tokenizer')
        local scheduler = require('scheduler')
        local tokens = tokenizer.parse(KS_SRC)
        local __load_scene_tokens = function(name)
          -- Engine scene paths are allowlisted to demo/...; web scene keys
          -- are bare basenames, so try the key as-is and stripped variants.
          local key = name
          if type(key) == 'string' and key:sub(1, 5) == 'demo/' then
            key = key:sub(6)
          end
          local src = __SCENE_SOURCES and __SCENE_SOURCES[key]
          if type(src) ~= 'string' or #src == 0 then
            src = __SCENE_SOURCES and __SCENE_SOURCES[name]
          end
          if type(src) ~= 'string' or #src == 0 then return nil end
          local okP, nt = pcall(tokenizer.parse, src)
          if not okP or type(nt) ~= 'table' then return nil end
          return nt
        end
        local ctx = {
          f = {}, sf = {}, tf = {}, mp = {}, lf = {},
          variables = {}, current_scene = '${sceneName}',
          token_index = 1, tokens = tokens,
          text_state = {}, layer_state = {}, audio_state = {},
          macro_args = {}, call_stack = {}, flag_stack = {},
        }
        local result = ''
        while result == '' do
        -- NOTE: rebuild from ctx.tokens / ctx.token_index, NOT the local
        -- tokens var — [load] resume swaps ctx.tokens to the saved scene,
        -- and the outer loop must continue THAT scene (round 47 fix).
        local co = coroutine.create(function()
          local ok, err = pcall(function() scheduler.run(ctx, ctx.tokens, ctx.token_index) end)
          if not ok then error(err) end
        end)
        local frames = 0
        local clicks = 0
        while true do
          local status = coroutine.status(co)
          if status == 'dead' then
            -- [load] resume: engine semantics — reload the saved scene and
            -- continue from the saved token (SaveCommands.load sets these).
            if ctx._pendingLoadScene and type(__load_scene_tokens) == 'function' then
              local ok2, ntoks = pcall(__load_scene_tokens, ctx._pendingLoadScene)
              if ok2 and type(ntoks) == 'table' then
                ctx.tokens = ntoks
                ctx.token_index = math.max(1, tonumber(ctx._pendingLoadToken) or 1)
                ctx.current_scene = ctx._pendingLoadScene
                ctx.currentScene = ctx._pendingLoadScene
                ctx.stop_flag = false
                ctx._pendingLoadScene = nil
                ctx._pendingLoadToken = nil
                co = coroutine.create(function()
                  local okc, errc = pcall(function() scheduler.run(ctx, ntoks, ctx.token_index) end)
                  if not okc then error(errc) end
                end)
                break
              end
            end
            result = 'DONE:' .. tostring(ctx.token_index) .. ':' .. tostring(clicks) break
          end
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
            local pg = ctx.text_state and ctx.text_state.draws
            local pdraws = {}
            if type(pg) == 'table' then
              for _, d in ipairs(pg) do
                if type(d) == 'table' and d.text and #d.text > 0 then
                  pdraws[#pdraws + 1] = { t = tostring(d.text), x = tonumber(d.x) or 0, y = tonumber(d.y) or 0 }
                end
              end
            end
            __SCENE_PAGE = pdraws
            __SCENE_BACKLOG = __SCENE_BACKLOG or {}
            __SCENE_BACKLOG[#__SCENE_BACKLOG + 1] = pdraws
          end
          local r = { coroutine.resume(co, 16) }
          if not r[1] then result = 'ERR:' .. tostring(r[2]) break end
        end
        end
        -- Collect the current visible text draws from the Lua TextScene
        -- as a lightweight JSON array (fields: text/x/y/r/g/b/a/scale/bold/italic).
        local st = ctx.text_state
        local draws = {}
        if st and type(st.draws) == 'table' then
          for _, d in ipairs(st.draws) do
            if type(d) == 'table' and d.text and #d.text > 0 then
              draws[#draws + 1] = {
                t = tostring(d.text),
                x = tonumber(d.x) or 0,
                y = tonumber(d.y) or 0,
                r = tonumber(d.r) or 255,
                g = tonumber(d.g) or 255,
                b = tonumber(d.b) or 255,
                a = tonumber(d.a) or 255,
                s = tonumber(d.scale) or 1,
                bd = d.bold == true and 1 or 0,
                it = d.italic == true and 1 or 0,
              }
            end
          end
        end
                __SCENE_DRAWS_TABLE = draws
        -- Export endings unlocked by [ending] (engine: ctx.seen_endings).
        __SCENE_ENDINGS = {}
        if type(ctx.seen_endings) == 'table' then
          local n = 0
          for eid, einfo in pairs(ctx.seen_endings) do
            n = n + 1
            __SCENE_ENDINGS[n] = { id = tostring(eid), name = (type(einfo) == 'table' and tostring(einfo.name or '')) or '' }
          end
        end
        return result
      `)
      // sync the structured draws into the core overlay (JSON parse)
      const drawsTable = lua.global.get('__SCENE_DRAWS_TABLE')
      core.setDraws(drawsTable ? JSON.parse(JSON.stringify(drawsTable)) : [])
      // commit all [p]-parked pages accumulated during the run (VN history)
      const pages = lua.global.get('__SCENE_BACKLOG')
      if (pages && Array.isArray(pages)) {
        for (const pg of pages) {
          if (Array.isArray(pg) && pg.length > 0) {
            core.pushBacklog(JSON.parse(JSON.stringify(pg)))
          }
        }
      }
            const endings = lua.global.get('__SCENE_ENDINGS')
      if (endings && Array.isArray(endings)) core.recordEndings(JSON.parse(JSON.stringify(endings)))
      lua.global.set('__SCENE_BACKLOG', null)
      return out
    },
    /** Run a scene from a pre-baked story bundle (ks_bake --web): zero
     *  parse/compile at start — the serialized stream loads directly. */
    async runFromBundle(bundle, sceneKey, opts = {}) {
      const scene = bundle && bundle.scenes && bundle.scenes[sceneKey]
      if (!scene) return 'ERR:scene-not-in-bundle:' + String(sceneKey)
      lua.global.set('BAKED_SCENE', scene)
      lua.global.set('__BUNDLE_SCENES', bundle.scenes ?? {})
      lua.global.set('__SCENE_SOURCES', opts.sceneSources ?? {})
      lua.global.set('__CLICK', false)
      lua.global.set('__AUTOCLICK', !!opts.autoClick)
      const maxFrames = opts.maxFrames ?? 200000
      const out = await lua.doString(`
        local compiler = require('kag.compiler')
        local scheduler = require('scheduler')
        local tokenizer = require('tokenizer')
        local tokens = compiler.deserialize(BAKED_SCENE)
        -- [load] resume: bundled scenes are serialized; parse them on demand.
        local __load_scene_tokens = function(name)
          local key = name
          if type(key) == 'string' and key:sub(1, 5) == 'demo/' then
            key = key:sub(6)
          end
          local ser = __BUNDLE_SCENES and __BUNDLE_SCENES[key]
          if type(ser) ~= 'string' or #ser == 0 then
            ser = __BUNDLE_SCENES and __BUNDLE_SCENES[name]
          end
          if type(ser) == 'string' and #ser > 0 then
            return compiler.deserialize(ser)
          end
          local src = __SCENE_SOURCES and __SCENE_SOURCES[key]
          if type(src) ~= 'string' or #src == 0 then
            src = __SCENE_SOURCES and __SCENE_SOURCES[name]
          end
          if type(src) == 'string' and #src > 0 then
            local okP, nt = pcall(tokenizer.parse, src)
            return okP and type(nt) == 'table' and nt or nil
          end
          return nil
        end
        local ctx = {
          f = {}, sf = {}, tf = {}, mp = {}, lf = {},
          variables = {}, current_scene = '${sceneKey}',
          token_index = 1, tokens = tokens,
          text_state = {}, layer_state = {}, audio_state = {},
          macro_args = {}, call_stack = {}, flag_stack = {},
        }
        local result = ''
        while result == '' do
        -- NOTE: rebuild from ctx.tokens / ctx.token_index, NOT the local
        -- tokens var — [load] resume swaps ctx.tokens to the saved scene,
        -- and the outer loop must continue THAT scene (round 47 fix).
        local co = coroutine.create(function()
          local ok, err = pcall(function() scheduler.run(ctx, ctx.tokens, ctx.token_index) end)
          if not ok then error(err) end
        end)
        local frames = 0
        local clicks = 0
        while true do
          local status = coroutine.status(co)
          if status == 'dead' then
            -- [load] resume: reload the saved (bundled) scene and continue
            -- from the saved token — engine resume_from_save semantics.
            if ctx._pendingLoadScene and type(__load_scene_tokens) == 'function' then
              local ok2, ntoks = pcall(__load_scene_tokens, ctx._pendingLoadScene)
              if ok2 and type(ntoks) == 'table' then
                ctx.tokens = ntoks
                ctx.token_index = math.max(1, tonumber(ctx._pendingLoadToken) or 1)
                ctx.current_scene = ctx._pendingLoadScene
                ctx.currentScene = ctx._pendingLoadScene
                ctx.stop_flag = false
                ctx._pendingLoadScene = nil
                ctx._pendingLoadToken = nil
                co = coroutine.create(function()
                  local okc, errc = pcall(function() scheduler.run(ctx, ntoks, ctx.token_index) end)
                  if not okc then error(errc) end
                end)
                break
              end
            end
            result = 'DONE:' .. tostring(ctx.token_index) .. ':' .. tostring(clicks) break
          end
          frames = frames + 1
          if frames > ${maxFrames} then result = 'ERR:frame-limit@' .. tostring(ctx.token_index) break end
          if ctx.waiting_input then
            if not __CLICK then
              if __AUTOCLICK and clicks < 100 then __CLICK = true else
                result = 'WAIT:' .. tostring(ctx.token_index) break
              end
            end
            ctx.waiting_input = false
            __CLICK = false
            clicks = clicks + 1
            local pg = ctx.text_state and ctx.text_state.draws
            local pdraws = {}
            if type(pg) == 'table' then
              for _, d in ipairs(pg) do
                if type(d) == 'table' and d.text and #d.text > 0 then
                  pdraws[#pdraws + 1] = { t = tostring(d.text), x = tonumber(d.x) or 0, y = tonumber(d.y) or 0 }
                end
              end
            end
            __SCENE_PAGE = pdraws
            __SCENE_BACKLOG = __SCENE_BACKLOG or {}
            __SCENE_BACKLOG[#__SCENE_BACKLOG + 1] = pdraws
          end
          local r = { coroutine.resume(co, 16) }
          if not r[1] then result = 'ERR:' .. tostring(r[2]) break end
        end
        end
        local st = ctx.text_state
        local draws = {}
        if st and type(st.draws) == 'table' then
          for _, d in ipairs(st.draws) do
            if type(d) == 'table' and d.text and #d.text > 0 then
              draws[#draws + 1] = {
                t = tostring(d.text),
                x = tonumber(d.x) or 0,
                y = tonumber(d.y) or 0,
                r = tonumber(d.r) or 255,
                g = tonumber(d.g) or 255,
                b = tonumber(d.b) or 255,
                a = tonumber(d.a) or 255,
                s = tonumber(d.scale) or 1,
                bd = d.bold == true and 1 or 0,
                it = d.italic == true and 1 or 0,
              }
            end
          end
        end
                __SCENE_DRAWS_TABLE = draws
        -- Export endings unlocked by [ending] (engine: ctx.seen_endings).
        __SCENE_ENDINGS = {}
        if type(ctx.seen_endings) == 'table' then
          local n = 0
          for eid, einfo in pairs(ctx.seen_endings) do
            n = n + 1
            __SCENE_ENDINGS[n] = { id = tostring(eid), name = (type(einfo) == 'table' and tostring(einfo.name or '')) or '' }
          end
        end
        return result
      `)
      const drawsTable = lua.global.get('__SCENE_DRAWS_TABLE')
      core.setDraws(drawsTable ? JSON.parse(JSON.stringify(drawsTable)) : [])
      // commit all [p]-parked pages accumulated during the run (VN history)
      const pages = lua.global.get('__SCENE_BACKLOG')
      if (pages && Array.isArray(pages)) {
        for (const pg of pages) {
          if (Array.isArray(pg) && pg.length > 0) {
            core.pushBacklog(JSON.parse(JSON.stringify(pg)))
          }
        }
      }
            const endings = lua.global.get('__SCENE_ENDINGS')
      if (endings && Array.isArray(endings)) core.recordEndings(JSON.parse(JSON.stringify(endings)))
      lua.global.set('__SCENE_BACKLOG', null)
      return out
    },
    /** Raise the click signal for the next runScene. */
    async click() { lua.global.set('__CLICK', true) },
    /** Snapshot the real Lua layer tree (Layers.snapshot) for rendering. */
    async snapshotLayers() {
      const out = await lua.doString(`
        local layers = require('layers')
        local snaps = layers.snapshot()
        local out = {}
        for i, s in ipairs(snaps) do
          out[i] = { name = s.name, tag = s.tag, x = s.x, y = s.y, w = s.w, h = s.h, visible = s.visible, opacity = s.opacity, z = s.z, texture = s.texture }
        end
        return out
      `)
      return out
    },
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
