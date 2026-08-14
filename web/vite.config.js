import { defineConfig } from 'vite'

// The web player serves the whole repo root as static content so
// /scripts/, /demo/, /assets/ and /cache/story/story.lua resolve.
export default defineConfig({
  publicDir: '..',
  server: { port: 5174, host: '127.0.0.1' },
})
