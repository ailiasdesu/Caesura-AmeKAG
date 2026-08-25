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
  './assets/fonts/NotoSansCJKsc-Regular.otf'
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => {
      return cache.addAll(STATIC_ASSETS).catch((err) => {
        console.warn('[SW] Pre-caching warning (some assets might load on-demand):', err);
      });
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
        // Offline fallback
        if (request.destination === 'document') {
          return caches.match('./index.html');
        }
      });
    })
  );
});
