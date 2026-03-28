const SOUND_ASSETS = [
  "./assets/sound/bell.flac",
  "./assets/sound/coin.flac",
  "./assets/sound/death.flac",
  "./assets/sound/dig01.flac",
  "./assets/sound/disarm01.flac",
  "./assets/sound/disarm02.flac",
  "./assets/sound/drop01.flac",
  "./assets/sound/drop02.flac",
  "./assets/sound/eat01.flac",
  "./assets/sound/flee01.flac",
  "./assets/sound/hit01.flac",
  "./assets/sound/hit02.flac",
  "./assets/sound/hit03.flac",
  "./assets/sound/hitpoint_warn.flac",
  "./assets/sound/hitwall01.flac",
  "./assets/sound/hitwall02.flac",
  "./assets/sound/identify.flac",
  "./assets/sound/kill01.flac",
  "./assets/sound/kill02.flac",
  "./assets/sound/kill03.flac",
  "./assets/sound/kill04.flac",
  "./assets/sound/kill05.flac",
  "./assets/sound/miss01.flac",
  "./assets/sound/miss02.flac",
  "./assets/sound/miss03.flac",
  "./assets/sound/open01.flac",
  "./assets/sound/open02.flac",
  "./assets/sound/pickup01.flac",
  "./assets/sound/pickup02.flac",
  "./assets/sound/quaff01.flac",
  "./assets/sound/shoot01.flac",
  "./assets/sound/shut01.flac",
  "./assets/sound/step01.flac",
  "./assets/sound/step02.flac",
  "./assets/sound/step03.flac",
  "./assets/sound/step04.flac",
  "./assets/sound/teleport.flac",
].map((path) => encodeURI(path));

const APP_CACHE = "sil-q-web-v6";
const APP_ASSETS = [
  "./",
  "./index.html",
  "./style.css",
  "./helper.js",
  "./app.js",
  "./manifest.webmanifest",
  "./icon192.png",
  "./icon512.png",
  "./assets/sound/sound.json",
  ...SOUND_ASSETS,
  "./assets/icon-song.png",
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
