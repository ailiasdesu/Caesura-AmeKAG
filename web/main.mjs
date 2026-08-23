// Caesura Web Player entry — loads scripts via fetch, runs scenes in
// wasmoon. Prefers the pre-baked story bundle (ks_bake --web) for
// zero-parse scene starts; falls back to raw .ks sources.
import { createPlayer } from './bridge.js'
import { DomRenderer } from './dom-renderer.js'
import { buildSceneOptions } from './scene-options.js'
import { createPlayerSettings, VOLUME_BUSSES } from './player-settings.js'

const stage = document.getElementById('stage')
const logEl = document.getElementById('log')
const statusEl = document.getElementById('status')
const endingsEl = document.getElementById('endings')
const endingsCount = document.getElementById('endings-count')
const audioStatusEl = document.getElementById('audio-status')
const savesEl = document.getElementById('saves')
const savesCount = document.getElementById('saves-count')
const log = (s) => { logEl.textContent += s + '\n'; logEl.scrollTop = logEl.scrollHeight }

const SCRIPTS_BASE = '/scripts/'
const DEMO_BASE = '/demo/'
const STORY_BASE = '/cache/story/story.lua'
const ASSET_BASE = '/assets/'

// wasmFile stays undefined in production (wasmoon fetches its CDN
// default); a test host may pin a local copy via self.__CAESURA_WASM_FILE__
// so fake-DOM runs can load the engine offline without changing browser
// behavior (behavior unchanged when the global is unset).
const wasmFile = (typeof self !== 'undefined' && self.__CAESURA_WASM_FILE__)
  ? self.__CAESURA_WASM_FILE__
  : undefined
const player = await createPlayer({ scriptsBase: SCRIPTS_BASE, ...(wasmFile ? { wasmFile } : {}) })
log('engine loaded; kag table ready')

const renderer = new DomRenderer(player.core, stage)
let storyBundle = null

// ---- player settings (round 86) -------------------------------------
// Centralized, persistent settings (language / auto / text speed / skip /
// per-bus volume). Reading here so the player boots with saved preferences
// and any UI write propagates to the engine (i18n + audio buses) live.
const settings = createPlayerSettings()
settings.load()
// Push the persisted volume / language into the engine at boot.
applyVolumes(settings.get('volumes'))
void applyLanguage(settings.get('language'))

/**
 * Mirror the current settings controller state back into the settings UI
 * controls wherever they actually mirror a persisted field. Safe against
 * echo loops: assigning el.value / el.checked programmatically does NOT
 * fire change/input events, so this never re-triggers a settings set.
 * Called after load, on any settings notification (so an external write
 * such as a reset or a programmatic set updates the sliders/checkbox), and
 * after reset. Text-speed/volume sliders show the sanitized in-range value.
 */
function syncSettingsControls() {
  const langEl = document.getElementById('settings-lang')
  const autoEl = document.getElementById('settings-auto')
  const speedEl = document.getElementById('settings-speed')
  const bgmEl = document.getElementById('settings-bgm')
  const seEl = document.getElementById('settings-se')
  const voiceEl = document.getElementById('settings-voice')
  if (!langEl) return // settings UI absent (headless) — nothing to mirror
  langEl.value = settings.get('language') in { en: 1, zh: 1, 'zh-TW': 1, ja: 1 } ? settings.get('language') : 'en'
  autoEl.checked = settings.get('autoClick')
  speedEl.value = String(settings.get('textSpeed'))
  bgmEl.value = String(settings.get('volumes').bgm)
  seEl.value = String(settings.get('volumes').se)
  voiceEl.value = String(settings.get('volumes').voice)
}

// Keep the engine (volume buses + i18n) live behind any later settings
// change, and keep the settings UI mirrors in sync (bidirectional, no cycle:
// control writes -> settings.set -> notify -> mirror back without an event).
settings.subscribe(({ field, settings: s }) => {
  if (field === '*') {
    // Reset carried defaults: re-apply every engine-facing setting now.
    applyVolumes(s.volumes)
    void applyLanguage(s.language)
  } else {
    if (field === 'volumes') applyVolumes(s.volumes)
    if (field === 'language') void applyLanguage(s.language)
  }
  syncSettingsControls()
})

/**
 * Reset all settings to their defaults and apply them to the engine +
 * UI immediately (round 87 reset): language resets to default (en), volume
 * buses return to 1.0, auto/skip off, text speed back to DEFAULT_TEXT_SPEED.
 * settings.reset() notifies subscribers ('*'), which re-applies volumes +
 * language and mirrors the controls.
 */
function resetPlayerSettings() {
  settings.reset()
  const s = settings.getAll()
  autoMode = s.autoClick
  document.getElementById('auto').textContent = autoMode ? '⏸ Auto' : '⏩ Auto'
  if (autoMode) scheduleAuto()
  else if (autoTimer) clearTimeout(autoTimer)
  syncSettingsControls()
}

/** Forward volumes to both the core state machine and the WebAudio engine. */
function applyVolumes(vols) {
  for (const b of VOLUME_BUSSES) {
    const v = Number.isFinite(vols?.[b]) ? vols[b] : 1
    try { player.core.audioSetBusVolume(b, v) } catch { /* noop */ }
    try { player.audio.setBusVolume(b, v) } catch { /* noop */ }
  }
}

/** Forward the active language to the pure-Lua i18n module. */
async function applyLanguage(lang) {
  try { await player.setLanguage(lang) } catch { /* noop */ }
}

/** Sync once from the persisted settings into the settings UI and bind the
 *  controls so any change writes straight back to the settings controller
 *  (which notifies -> engine wiring). Auto toggle also mirrors the #auto
 *  play/advance button (round 86). */
function bindSettingsControls() {
  const langEl = document.getElementById('settings-lang')
  const autoEl = document.getElementById('settings-auto')
  const speedEl = document.getElementById('settings-speed')
  const bgmEl = document.getElementById('settings-bgm')
  const seEl = document.getElementById('settings-se')
  const voiceEl = document.getElementById('settings-voice')
  // seed control values from the settings controller (kept in sync with the
  // subscriber notify path — see syncSettingsControls).
  syncSettingsControls()
  // writes: control operation -> settings controller (which notifies -> engine
  // wiring + control mirror). No echo loop: programmatic .value assignment
  // never fires change/input events.
  langEl.addEventListener('change', () => settings.set('language', langEl.value))
  autoEl.addEventListener('change', () => {
    settings.set('autoClick', autoEl.checked)
    const cur = settings.get('autoClick')
    autoMode = cur
    document.getElementById('auto').textContent = cur ? '⏸ Auto' : '⏩ Auto'
    if (cur) scheduleAuto()
    else if (autoTimer) clearTimeout(autoTimer)
  })
  speedEl.addEventListener('input', () => settings.set('textSpeed', Number(speedEl.value)))
  bgmEl.addEventListener('input', () => settings.setVolume('bgm', Number(bgmEl.value)))
  seEl.addEventListener('input', () => settings.setVolume('se', Number(seEl.value)))
  voiceEl.addEventListener('input', () => settings.setVolume('voice', Number(voiceEl.value)))
  const resetBtn = document.getElementById('settings-reset')
  if (resetBtn) resetBtn.addEventListener('click', () => resetPlayerSettings())
}

const syncTextures = () => {
  for (const [id, t] of player.core.textures) {
    renderer.setTextureUrl(id, ASSET_BASE + t.path)
  }
}

async function loadStoryBundle() {
  try {
    const src = await (await fetch(STORY_BASE)).text()
    player.lua.global.set('STORY_SRC', src)
    const SQ = String.fromCharCode(39)
    const code = '  local chunk = assert(load(STORY_SRC, ' + SQ + '@story.lua' + SQ + ', ' + SQ + 't' + SQ + ', _ENV))' + String.fromCharCode(10) + '  return chunk()'
    const bundle = await player.lua.doString(code)
    storyBundle = bundle
    log('story bundle: ' + Object.keys(bundle.scenes).length + ' scenes, ' + bundle.assets.length + ' assets')
    populateScenePicker(bundle.scenes)
    populateFallbackScenes()
  } catch (e) {
    log('story bundle unavailable: ' + String(e).slice(0, 80))
  }
}


function populateScenePicker(scenes) {
  const sel = document.getElementById('scene')
  sel.textContent = ''
  for (const group of buildSceneOptions(scenes)) {
    const og = document.createElement('optgroup')
    og.label = group.label
    for (const opt of group.options) {
      const o = document.createElement('option')
      o.value = opt.value
      o.textContent = opt.label
      og.appendChild(o)
    }
    sel.appendChild(og)
  }
}

// When the bundle is unavailable, offer raw .ks files from /demo (flat list).
const FALLBACK_SCENES = [
  'galgame_demo.ks', 'full_pipeline_demo.ks', 'sma_demo.ks', 'showcase.ks',
  'tutorial/tutorial_01_hello.ks', 'tutorial/tutorial_02_text.ks',
  'tutorial/tutorial_03_layers.ks', 'tutorial/tutorial_04_audio.ks',
  'tutorial/tutorial_05_branching.ks', 'tutorial/tutorial_06_effects.ks',
]

function populateFallbackScenes() {
  if (document.getElementById('scene').options.length > 0) return
  const sel = document.getElementById('scene')
  sel.textContent = ''
  for (const name of FALLBACK_SCENES) {
    const o = document.createElement('option')
    o.value = name
    o.textContent = name
    sel.appendChild(o)
  }
}

let lastEndingsLen = -1
const syncEndings = () => {
  const list = player.core.endings || []
  if (list.length === lastEndingsLen) return
  lastEndingsLen = list.length
  endingsCount.textContent = String(list.length)
  endingsEl.textContent = ''
  for (const e of list) {
    const row = document.createElement('div')
    row.className = 'ending-entry'
    row.textContent = (e.id || '?') + ' — ' + (e.name || '')
    endingsEl.appendChild(row)
  }
}

const syncAudioStatus = () => {
  const bus = player.core.audioBus
  const parts = []
  if (bus.bgm && bus.bgm.playing) parts.push('BGM: ' + bus.bgm.path.split('/').pop())
  // audioPlay(kind, ...) stores the played entry directly on audioBus[kind],
  // so after any [se ...] the SE bus is the *last* entry (an object), not the
  // initial empty array — normalize both shapes so this never throws.
  const seList = Array.isArray(bus.se) ? bus.se : (bus.se ? [bus.se] : [])
  const sePlaying = seList.filter((x) => x && x.playing)
  if (sePlaying.length > 0) parts.push('SE: ' + sePlaying.length)
  if (bus.voice && bus.voice.playing) parts.push('VOICE: ' + bus.voice.path.split('/').pop())
  const txt = parts.length > 0 ? parts.join(' · ') : '—'
  if (audioStatusEl.textContent !== txt) audioStatusEl.textContent = txt
}

async function runScene(name) {
  log('running ' + name)
  statusEl.textContent = 'running…'
  const runOpts = {
    autoClick: settings.get('autoClick'),
    textSpeed: settings.get('textSpeed'),
    skip: settings.get('skipMode'),
  }
  let out
  if (storyBundle && storyBundle.scenes[name]) {
    out = await player.runFromBundle(storyBundle, name, runOpts)
  } else {
    const ks = await (await fetch(DEMO_BASE + name)).text()
    out = await player.runScene(ks, name, runOpts)
  }
  syncTextures()
  await renderer.render()
  statusEl.textContent = 'parked: ' + out
  log('result: ' + out)
}

async function advance() {
  const sel = document.getElementById('scene').value
  // VN semantics: advance resumes the previously parked scene cursor one page
  // (desktop on_click parity) instead of re-running the whole scene from token 1.
  // textSpeed/skip ride along so a mid-run setting toggle takes effect on the
  // next advanced page (the bridge re-applies them to the live/wrapped ctx).
  const advOpts = {
    autoClick: false, maxFrames: 5000, advance: true, advanceScene: sel,
    textSpeed: settings.get('textSpeed'),
    skip: settings.get('skipMode'),
  }
  let out
  if (storyBundle && storyBundle.scenes[sel]) {
    out = await player.runFromBundle(storyBundle, sel, advOpts)
  } else {
    const ks = await (await fetch(DEMO_BASE + sel)).text()
    out = await player.runScene(ks, sel, advOpts)
  }
  syncTextures()
  await renderer.render()
  statusEl.textContent = 'advance: ' + out
  log('advance: ' + out)
}

await loadStoryBundle()
renderSlots()
bindSettingsControls()

document.getElementById('run').addEventListener('click', () => {
  void runScene(document.getElementById('scene').value)
})
document.getElementById('advance').addEventListener('click', () => {
  void advance()
})
document.getElementById('auto').addEventListener('click', () => {
  // persist the toggle through player settings (round 86); autoMode + UI
  // labels stay as the single source of truth derived from settings.
  settings.set('autoClick', !settings.get('autoClick'))
  autoMode = settings.get('autoClick')
  document.getElementById('auto').textContent = autoMode ? '⏸ Auto' : '⏩ Auto'
  if (autoMode) scheduleAuto()
  else if (autoTimer) clearTimeout(autoTimer)
})

// --- backlog (VN history) ---
const backlogEl = document.getElementById('backlog')
const backlogCount = document.getElementById('backlog-count')
let lastBacklogLen = -1
const syncBacklog = () => {
  const bl = player.core.backlog
  if (bl.length === lastBacklogLen) return
  lastBacklogLen = bl.length
  backlogCount.textContent = String(bl.length)
  backlogEl.textContent = ''
  bl.forEach((entry, i) => {
    const row = document.createElement('div')
    row.className = 'backlog-entry'
    row.innerHTML = '<span class="bl-line">' + (i + 1) + '</span>' + entry.text.replace(/</g, '&lt;')
    row.title = 'backlog ' + (i + 1)
    backlogEl.appendChild(row)
  })
  backlogEl.scrollTop = backlogEl.scrollHeight
}

// --- save slots (round 49) ---
const fmtTime = (ts) => (ts ? new Date(ts).toLocaleTimeString() : '—')

async function renderSlots() {
  const slots = player.listSlots()
  savesCount.textContent = String(slots.length)
  savesEl.textContent = ''
  if (slots.length === 0) {
    const row = document.createElement('div')
    row.className = 'save-entry'
    row.innerHTML = '<span class="slot-meta">(no saves yet — run a scene, then Save Current)</span>'
    savesEl.appendChild(row)
    return
  }
  for (const s of slots) {
    const row = document.createElement('div')
    row.className = 'save-entry'
    const meta = document.createElement('span')
    meta.className = 'slot-meta'
    meta.textContent = 'scene ' + s.scene + ' · token ' + s.token + ' · ' + fmtTime(s.savedAt)
    const loadBtn = document.createElement('button')
    loadBtn.textContent = 'Load'
    loadBtn.addEventListener('click', async () => {
      log('loading slot ' + s.slot + '…')
      const out = await player.loadSlot(s.slot, { sceneSources: {} })
      syncTextures()
      await renderer.render()
      statusEl.textContent = 'load: ' + out
      log('load result: ' + out)
      renderSlots()
    })
    const delBtn = document.createElement('button')
    delBtn.textContent = 'Delete'
    delBtn.addEventListener('click', () => {
      player.deleteSlot(s.slot)
      log('deleted slot ' + s.slot)
      renderSlots()
    })
    const num = document.createElement('span')
    num.className = 'slot-num'
    num.textContent = String(s.slot).padStart(2, '0')
    row.appendChild(num)
    row.appendChild(meta)
    row.appendChild(loadBtn)
    row.appendChild(delBtn)
    savesEl.appendChild(row)
  }
}

document.getElementById('save-now').addEventListener('click', async () => {
  const slot = Number(document.getElementById('save-slot').value)
  if (!Number.isInteger(slot) || slot < 0 || slot > 99) { log('slot must be 0..99'); return }
  const ok = await player.saveCurrent(slot)
  log(ok ? 'saved current position to slot ' + slot : 'save failed (no scene run yet)')
  renderSlots()
})
document.getElementById('refresh-slots').addEventListener('click', () => renderSlots())

// --- auto-advance ---
let autoMode = settings.get('autoClick') // derived from persisted settings (round 86)
let autoTimer = null
document.getElementById('auto').textContent = autoMode ? '⏸ Auto' : '⏩ Auto'
const scheduleAuto = () => {
  if (!autoMode) return
  autoTimer = setTimeout(() => { void advance(); scheduleAuto() }, 1200)
}

// render loop: sync core state to DOM every frame (CSS transitions
// interpolate sprite moves/fades between renders).
const frame = () => {
  renderer.render()
  syncBacklog()
  syncEndings()
  syncAudioStatus()
  requestAnimationFrame(frame)
}
requestAnimationFrame(frame)

// Default scene: URL ?scene=<name> wins, else the FIRST scene of the
// loaded story bundle, else the demo fallback -- a packaged game must boot
// without any manual bundle edits (Validation-Release task book §9).
const initialScene =
    new URLSearchParams(location.search).get('scene')
    ?? (storyBundle ? Object.keys(storyBundle.scenes)[0] : null)
    ?? 'galgame_demo.ks'
void runScene(initialScene)
