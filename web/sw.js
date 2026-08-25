// Caesura Web Player Service Worker (PWA Offline Support)
const CACHE_NAME = 'caesura-web-v1.0.0-rc.1';
const STATIC_ASSETS = [
  './',
  './index.html',
  './main.mjs',
  './bridge.js',
  './adapter-core.js',
  './audio-engine.js',
  './dom-renderer.js',
  './player-settings.js',
  './scene-options.js',
  './scripts-index.json',
  './manifest.webmanifest',
  './web-assets/glue.wasm',
  './scripts/index.json',
  './cache/story/story.lua',
  './assets/fonts/NotoSansCJKsc-Regular.otf',
  './assets/icon-192.png',
  './assets/icon-512.png'
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME).then(async (cache) => {
      // Pre-cache statically available assets with fault tolerance
      await Promise.allSettled(
        STATIC_ASSETS.map(async (url) => {
          try {
            const res = await fetch(url);
            if (res.ok) {
              await cache.put(url, res);
            }
          } catch {
            // Optional or dynamically generated asset miss during install
          }
        })
      );
    }).then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys().then((keys) => {
      return Promise.all(
        keys.map((key) => {
          if (key !== CACHE_NAME) {
            return caches.delete(key);
          }
        })
      );
    }).then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', (event) => {
  const request = event.request;
  if (request.method !== 'GET') return;

  const url = new URL(request.url);
  if (!url.protocol.startsWith('http')) return;

  event.respondWith(
    caches.match(request).then((cachedResponse) => {
      if (cachedResponse) {
        return cachedResponse;
      }
      return fetch(request).then((networkResponse) => {
        if (networkResponse && networkResponse.status === 200) {
          const responseToCache = networkResponse.clone();
          caches.open(CACHE_NAME).then((cache) => {
            cache.put(request, responseToCache);
          });
        }
        return networkResponse;
      }).catch(() => {
        // Offline fallback for navigation / documents
        if (request.destination === 'document' || request.mode === 'navigate') {
          return caches.match('./index.html').then((html) => html || caches.match('./'));
        }
      });
    })
  );
});
