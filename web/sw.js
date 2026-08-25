// Caesura Web Player Service Worker (PWA Offline Support & IndexedDB Asset Persistence)
//
// CLASSIC SCRIPT — DO NOT ADD ESM SYNTAX (`export`, or a top-level `import`).
//
// web/main.mjs registers this file with
//   navigator.serviceWorker.register('./sw.js')
// deliberately WITHOUT { type: 'module' }, so the browser parses it with the
// classic script goal. A single `export` statement here is a SyntaxError that
// aborts service-worker registration outright and silently disables the whole
// PWA offline cache. That regression already shipped once: ESM exports were added
// purely so vitest could `import` this file, and the unit suite stayed green
// while the runtime feature was dead.
//
// Consequences for how this file is structured:
//   * The IndexedDB helpers live HERE, not in a separate importScripts() include.
//     The build and packaging pipelines deploy exactly this one file
//     (web/vite.config.js closeBundle and scripts/package_game.sh both copy
//     'sw.js' by name), so a second script file would 404 at install time and
//     break registration in the same invisible way.
//   * web/test/sw.test.mjs therefore loads this file the way a browser does —
//     evaluated as a classic script with the worker globals stubbed — instead of
//     importing it as a module. That test also asserts the parse goal itself, so
//     re-adding `export` here now fails the suite instead of only the runtime.
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
  './touch-gestures.js',
  './scripts-index.json',
  './manifest.webmanifest',
  './web-assets/glue.wasm',
  './scripts/index.json',
  './cache/story/story.lua',
  './assets/fonts/NotoSansCJKsc-Regular.otf',
  './assets/icon-192.png',
  './assets/icon-512.png'
];

const IDB_DB_NAME = 'caesura-asset-cache';
const IDB_DB_VERSION = 1;
const IDB_STORE_NAME = 'assets';

/**
 * Categorize asset URL based on its file extension.
 * Supports audio (.ogg, .mp3, .wav), textures (.png, .webp, .jpg, .jpeg),
 * bytecode/script (.ksc, story.lua) and fonts (.otf, .ttf, .woff, .woff2).
 */
function getAssetCategory(url) {
  const cleanUrl = String(url || '').split(/[?#]/)[0].toLowerCase();
  if (/\.(ogg|mp3|wav)$/.test(cleanUrl)) return 'audio';
  if (/\.(png|webp|jpg|jpeg|gif)$/.test(cleanUrl)) return 'texture';
  if (/\.(ksc)$/.test(cleanUrl) || cleanUrl.endsWith('story.lua')) return 'bytecode';
  if (/\.(otf|ttf|woff|woff2)$/.test(cleanUrl)) return 'font';
  return null;
}

function isIDBCachedAsset(url) {
  return getAssetCategory(url) !== null;
}

/**
 * Open or upgrade the IndexedDB asset database.
 */
function openAssetDatabase() {
  return new Promise((resolve, reject) => {
    if (typeof indexedDB === 'undefined') {
      return reject(new Error('IndexedDB is not available in current environment'));
    }
    const request = indexedDB.open(IDB_DB_NAME, IDB_DB_VERSION);
    request.onupgradeneeded = (event) => {
      const db = event.target.result;
      if (!db.objectStoreNames.contains(IDB_STORE_NAME)) {
        const store = db.createObjectStore(IDB_STORE_NAME, { keyPath: 'url' });
        store.createIndex('category', 'category', { unique: false });
        store.createIndex('timestamp', 'timestamp', { unique: false });
      }
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

/**
 * Retrieve an asset record from IndexedDB by its URL.
 */
async function getAssetFromIDB(url) {
  const db = await openAssetDatabase();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(IDB_STORE_NAME, 'readonly');
    const store = tx.objectStore(IDB_STORE_NAME);
    const req = store.get(url);
    req.onsuccess = () => resolve(req.result || null);
    req.onerror = () => reject(req.error);
  });
}

/**
 * Store an asset into IndexedDB.
 */
async function putAssetToIDB(url, blob, mimeType, category) {
  const db = await openAssetDatabase();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(IDB_STORE_NAME, 'readwrite');
    const store = tx.objectStore(IDB_STORE_NAME);
    const record = {
      url,
      blob,
      mimeType: mimeType || (blob && blob.type) || 'application/octet-stream',
      size: (blob && blob.size) || 0,
      timestamp: Date.now(),
      category: category || getAssetCategory(url) || 'misc'
    };
    const req = store.put(record);
    req.onsuccess = () => resolve(record);
    req.onerror = () => reject(req.error);
  });
}

/**
 * Delete a specific asset from IndexedDB.
 */
async function deleteAssetFromIDB(url) {
  const db = await openAssetDatabase();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(IDB_STORE_NAME, 'readwrite');
    const store = tx.objectStore(IDB_STORE_NAME);
    const req = store.delete(url);
    req.onsuccess = () => resolve(true);
    req.onerror = () => reject(req.error);
  });
}

/**
 * Clear all records in the IndexedDB asset store.
 */
async function clearAssetCacheIDB() {
  const db = await openAssetDatabase();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(IDB_STORE_NAME, 'readwrite');
    const store = tx.objectStore(IDB_STORE_NAME);
    const req = store.clear();
    req.onsuccess = () => resolve(true);
    req.onerror = () => reject(req.error);
  });
}

/**
 * Compute aggregate statistics of cached assets.
 */
async function getCacheStatsIDB() {
  const db = await openAssetDatabase();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(IDB_STORE_NAME, 'readonly');
    const store = tx.objectStore(IDB_STORE_NAME);
    const req = store.getAll();
    req.onsuccess = () => {
      const all = req.result || [];
      let totalBytes = 0;
      const byCategory = { audio: 0, texture: 0, bytecode: 0, font: 0, misc: 0 };
      for (const item of all) {
        totalBytes += Number(item.size || (item.blob && item.blob.size) || 0);
        const cat = item.category || 'misc';
        byCategory[cat] = (byCategory[cat] || 0) + 1;
      }
      resolve({
        count: all.length,
        totalBytes,
        byCategory,
        assets: all.map((a) => ({
          url: a.url,
          category: a.category,
          size: a.size,
          mimeType: a.mimeType,
          timestamp: a.timestamp
        }))
      });
    };
    req.onerror = () => reject(req.error);
  });
}

/**
 * Explicitly download and store an asset into IndexedDB.
 */
async function cacheAssetUrl(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`HTTP ${res.status} fetching ${url}`);
  const blob = await res.blob();
  const mimeType = res.headers.get('content-type') || (blob && blob.type) || 'application/octet-stream';
  const category = getAssetCategory(url);
  return putAssetToIDB(url, blob, mimeType, category);
}

// ---------------------------------------------------------------------------
// Service Worker Lifecycle Events
// ---------------------------------------------------------------------------

if (typeof self !== 'undefined' && typeof self.addEventListener === 'function') {
  self.addEventListener('install', (event) => {
    event.waitUntil(
      caches.open(CACHE_NAME).then(async (cache) => {
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

    const pathname = url.pathname;
    const isIDBAsset = isIDBCachedAsset(pathname) || isIDBCachedAsset(request.url);

    if (isIDBAsset) {
      // Cache-first strategy via IndexedDB for binary assets
      event.respondWith(
        (async () => {
          try {
            // Check IndexedDB with full URL or pathname
            const cached = await getAssetFromIDB(request.url).catch(() => null)
              || await getAssetFromIDB(pathname).catch(() => null);

            if (cached && cached.blob) {
              return new Response(cached.blob, {
                status: 200,
                statusText: 'OK',
                headers: {
                  'Content-Type': cached.mimeType || 'application/octet-stream',
                  'Content-Length': String(cached.size || cached.blob.size || 0),
                  'X-Caesura-Cache': 'IndexedDB'
                }
              });
            }
          } catch (err) {
            console.warn('[SW-IDB] Query failed:', err);
          }

          // Network fallback: fetch and populate IndexedDB cache asynchronously
          try {
            const networkResponse = await fetch(request);
            if (networkResponse && networkResponse.status === 200) {
              const responseClone = networkResponse.clone();
              responseClone.blob().then((blob) => {
                const mimeType = networkResponse.headers.get('content-type') || blob.type;
                const category = getAssetCategory(pathname);
                putAssetToIDB(request.url, blob, mimeType, category).catch(() => {});
              }).catch(() => {});
            }
            return networkResponse;
          } catch (netErr) {
            return new Response('Asset unavailable offline', {
              status: 503,
              statusText: 'Service Unavailable'
            });
          }
        })()
      );
      return;
    }

    // Static application shell files via standard Cache API
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
          if (request.destination === 'document' || request.mode === 'navigate') {
            return caches.match('./index.html').then((html) => html || caches.match('./'));
          }
        });
      })
    );
  });

  // Client messaging for asset cache operations
  self.addEventListener('message', async (event) => {
    const data = event.data || {};
    const replyPort = event.ports && event.ports[0];
    const respond = (msg) => {
      if (replyPort) {
        replyPort.postMessage(msg);
      } else if (event.source && typeof event.source.postMessage === 'function') {
        event.source.postMessage(msg);
      }
    };

    try {
      switch (data.type) {
        case 'CACHE_ASSET': {
          const url = data.url;
          if (!url) throw new Error('Missing url in CACHE_ASSET');
          const record = await cacheAssetUrl(url);
          respond({ type: 'CACHE_ASSET_SUCCESS', url, size: record.size, success: true });
          break;
        }
        case 'CLEAR_ASSET_CACHE': {
          await clearAssetCacheIDB();
          respond({ type: 'CLEAR_ASSET_CACHE_SUCCESS', success: true });
          break;
        }
        case 'GET_CACHE_STATS': {
          const stats = await getCacheStatsIDB();
          respond({ type: 'GET_CACHE_STATS_SUCCESS', success: true, ...stats });
          break;
        }
        case 'DELETE_ASSET': {
          const url = data.url;
          await deleteAssetFromIDB(url);
          respond({ type: 'DELETE_ASSET_SUCCESS', url, success: true });
          break;
        }
        case 'SKIP_WAITING': {
          if (typeof self.skipWaiting === 'function') self.skipWaiting();
          respond({ type: 'SKIP_WAITING_SUCCESS', success: true });
          break;
        }
        default:
          break;
      }
    } catch (err) {
      respond({ type: 'ERROR', error: String(err.message || err), success: false });
    }
  });
}