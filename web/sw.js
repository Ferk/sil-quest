const APP_CACHE = "sil-q-web-v1";
const APP_ASSETS = [
  "./",
  "./index.html",
  "./style.css",
  "./helper.js",
  "./app.js",
  "./manifest.webmanifest",
  "./icon192.png",
  "./icon512.png",
  "./assets/unscii-fantasy.woff",
  "./lib/sil.js",
  "./lib/sil.wasm",
  "./lib/sil.data",
  "./lib/16x16_microchasm.png"
];

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(APP_CACHE).then((cache) => cache.addAll(APP_ASSETS))
  );
  self.skipWaiting();
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches.keys().then((keys) => Promise.all(
      keys
        .filter((key) => key !== APP_CACHE)
        .map((key) => caches.delete(key))
    )).then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", (event) => {
  const { request } = event;
  if (request.method !== "GET") return;

  const url = new URL(request.url);
  if (url.origin !== self.location.origin) return;

  event.respondWith(
    fetch(request)
      .then((response) => {
        if (!response.ok || response.type === "opaque") {
          return response;
        }

        const responseCopy = response.clone();
        event.waitUntil(
          caches.open(APP_CACHE).then((cache) => cache.put(request, responseCopy))
        );
        return response;
      })
      .catch(() => caches.match(request).then((cached) => {
        if (cached) return cached;
        if (request.mode === "navigate") {
          return caches.match("./index.html");
        }
        throw new Error(`No cached response for ${request.url}`);
      }))
  );
});
