import { createPlayer } from './bridge.js'
import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))
const scriptsDir = join(here, '..', 'scripts')
const index = JSON.parse(readFileSync(join(here, 'scripts-index.json'), 'utf8'))
const fileFetch = async (url) => {
  const u = new URL(url)
  const p = u.pathname.replace('/scripts/', scriptsDir + '/').replaceAll('/', '\\')
  return { text: async () => readFileSync(p, 'utf8'), json: async () => index }
}

const player = await createPlayer({ scriptsBase: 'http://local/scripts/', fetchImpl: fileFetch })
console.log('player created')

const ks = readFileSync(join(here, '..', 'demo', 'galgame_demo.ks'), 'utf8')

const parked = await player.runScene(ks, 'galgame_demo.ks', { maxFrames: 50000 })
console.log('parked:', parked)
console.log('bgm at park:', player.core.audioBus.bgm ? player.core.audioBus.bgm.path : 'none')
console.log('layers at park:', [...player.core.layers.keys()].join(', ') || '(none)')

player.core.events.length = 0
const out = await player.runScene(ks, 'galgame_demo.ks', { maxFrames: 200000, autoClick: true })
console.log('full:', out)

const kinds = {}
for (const e of player.core.events) kinds[e.kind] = (kinds[e.kind] ?? 0) + 1
console.log('event kinds:', JSON.stringify(kinds))
console.log('last events:', JSON.stringify(player.core.events.slice(-6)))
console.log('final layers:', [...player.core.layers.values()].map((n) => n.name + '@z' + n.z).join(', '))
console.log('final bgm:', player.core.audioBus.bgm ? player.core.audioBus.bgm.path : 'none')
console.log(out.startsWith('DONE') ? 'BRIDGE TEST PASS' : 'BRIDGE TEST PARTIAL: ' + out)
