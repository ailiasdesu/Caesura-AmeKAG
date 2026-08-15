import { defineConfig } from 'vite'
import { cpSync, mkdirSync } from 'node:fs'
import { resolve } from 'node:path'

// The web player serves the whole repo root as static content so
// /scripts/, /demo/, /assets/ and /cache/story/story.lua resolve
// (dev server: publicDir '..'). For a production build we do NOT copy
// the whole repo into dist (node_modules/.git/build would bloat it);
// instead copyPublicDir is off and a plugin copies exactly the runtime
// directories the player needs.
const REPO_ROOT = resolve(process.cwd(), '..')
const RUNTIME_DIRS = ['scripts', 'demo', 'assets', 'cache/story']

function copyRuntimeDirs() {
  return {
    name: 'caesura-copy-runtime-dirs',
    closeBundle() {
      for (const dir of RUNTIME_DIRS) {
        const from = resolve(REPO_ROOT, dir)
        const to = resolve(process.cwd(), 'dist', dir)
        mkdirSync(to, { recursive: true })
        cpSync(from, to, { recursive: true })
      }
    },
  }
}

export default defineConfig({
  publicDir: '..',
  server: { port: 5174, host: '127.0.0.1' },
  build: {
    outDir: 'dist',
    copyPublicDir: false,
    target: 'es2022',
    assetsDir: 'web-assets',
  },
  plugins: [copyRuntimeDirs()],
})
