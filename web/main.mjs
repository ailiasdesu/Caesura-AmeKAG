// Caesura Web Player entry — load scripts via fetch, run scenes in wasmoon.
import { createPlayer } from './bridge.js'
import { DomRenderer } from './dom-renderer.js'

const stage = document.getElementById('stage')
const logEl = document.getElementById('log')
const statusEl = document.getElementById('status')
const log = (s) => { logEl.textContent += s + '\n'; logEl.scrollTop = logEl.scrollHeight }

// serve scripts/ and demo/ from the repo root (vite dev server or static host)
const SCRIPTS_BASE = '/scripts/'
const DEMO_BASE = '/demo/'
const ASSET_BASE = '/assets/'

const player = await createPlayer({ scriptsBase: SCRIPTS_BASE })
log('kag table loaded; ' + player.core.textures.size + ' textures pending')

const renderer = new DomRenderer(player.core, stage)
const textureUrls = new Map()
for (const [id, t] of player.core.textures) {
  textureUrls.set(id, ASSET_BASE + t.path)
  renderer.setTextureUrl(id, ASSET_BASE + t.path)
}

async function runScene(name) {
  log('running ' + name + '…')
  statusEl.textContent = 'running…'
  const ks = await (await fetch(DEMO_BASE + name)).text()
  const out = await player.runScene(ks, name)
  renderer.render()
  statusEl.textContent = 'done: ' + out
  log('scene result: ' + out)
  log('events: ' + player.core.events.length)
  const layers = [...player.core.layers.values()].map((n) => n.name + '@' + n.z).join(', ')
  log('layers: ' + layers)
  const audio = player.core.audioBus.bgm ? 'bgm=' + player.core.audioBus.bgm.path : 'no bgm'
  log('audio: ' + audio)
}

document.getElementById('run').addEventListener('click', () => {
  void runScene(document.getElementById('scene').value)
})
document.getElementById('advance').addEventListener('click', () => {
  renderer.render()
  log('render tick')
})

// auto-run the first scene so a static host shows content immediately
void runScene('galgame_demo.ks')
