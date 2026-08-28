import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// The engine's HTTP editor server is on localhost:9876 and only allows
// localhost/127.0.0.1 CORS origins. In dev, Vite runs on :5173 -- proxy
// /api to the engine so the browser sees a same-origin request (no CORS
// preflight, no token header leaks). In production build the front-end
// is served from the same origin as the engine (or behind the same
// proxy), so this matches both modes.
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:9876',   // Windows: localhost resolves ::1 first (EditorServer binds IPv4 only)
        changeOrigin: true,
      },
    },
  },
  build: {
    outDir: 'dist',
    sourcemap: true,
  },
})
