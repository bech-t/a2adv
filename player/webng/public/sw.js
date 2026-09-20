// sw.js -- Service Worker : le site fonctionne hors ligne. Ecrit a la main,
// trois familles de fichiers, chacune sa strategie :
//
//  - l'application (index.html, scripts, styles... tout ce que `ng build`
//    produit hors aventures) : prechargee a l'installation dans un cache
//    propre a la version (`a2adv-app-<VERSION>`), servie CACHE D'ABORD. Les
//    noms de fichiers portent leur hash, VERSION change des que l'un d'eux
//    change. VERSION et PRECACHE sont ecrits apres le build par
//    tools/precache.mjs (les valeurs ci-dessous ne servent qu'en dev).
//  - les aventures (adventures/<id>/<hash>/...) : CACHE D'ABORD dans
//    `a2adv-adv`. Le dossier d'une aventure porte le hash de son contenu, un
//    fichier n'y change jamais : le cache peut le garder indefiniment. La page
//    y met aussi les aventures qu'on lui demande de telecharger (cf.
//    src/app/assets/offline.ts).
//  - catalog.json : RESEAU D'ABORD, repli sur `a2adv-data` hors ligne, pour
//    que les nouvelles aventures apparaissent des qu'il y a du reseau.
//
// Une nouvelle version s'installe en attente : elle ne prend la main qu'apres
// le message SKIP_WAITING (bouton « Recharger » de la page), pour qu'une page
// ouverte ne melange jamais des fichiers de deux versions.

const VERSION = "dev";
const PRECACHE = [];

const APP_CACHE = "a2adv-app-" + VERSION;
const ADV_CACHE = "a2adv-adv";      // meme nom dans src/app/assets/offline.ts
const DATA_CACHE = "a2adv-data";

const SCOPE = self.registration.scope;   // URL absolue, se termine par "/"
const scoped = (path) => new URL(path, SCOPE).href;

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(APP_CACHE).then((cache) => cache.addAll(PRECACHE.map(scoped))),
  );
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    (async () => {
      for (const name of await caches.keys()) {
        const oldApp = name.startsWith("a2adv-app-") && name !== APP_CACHE;
        if (oldApp || name === "a2adv-v1") await caches.delete(name);
      }
      await self.clients.claim();
    })(),
  );
});

self.addEventListener("message", (event) => {
  if (event.data === "SKIP_WAITING") self.skipWaiting();
});

self.addEventListener("fetch", (event) => {
  const req = event.request;
  if (req.method !== "GET") return;
  const url = new URL(req.url);
  if (!url.href.startsWith(SCOPE)) return;
  const path = url.href.slice(SCOPE.length);

  if (req.mode === "navigate") {
    event.respondWith(shell(req));
  } else if (path.startsWith("adventures/")) {
    event.respondWith(cacheFirst(req, ADV_CACHE));
  } else if (path === "catalog.json") {
    event.respondWith(networkFirst(req, DATA_CACHE));
  } else {
    event.respondWith(appFile(req));
  }
});

// Toute navigation charge la page d'accueil (les routes du player sont en `#`).
async function shell(req) {
  const cached = await caches.match(scoped("index.html"), { cacheName: APP_CACHE });
  return cached || fetch(req);
}

async function appFile(req) {
  const cached = await caches.match(req, { cacheName: APP_CACHE });
  return cached || fetch(req);
}

async function cacheFirst(req, cacheName) {
  const cache = await caches.open(cacheName);
  const cached = await cache.match(req);
  if (cached) return cached;
  const res = await fetch(req);
  if (res.ok) await cache.put(req, res.clone());
  return res;
}

async function networkFirst(req, cacheName) {
  const cache = await caches.open(cacheName);
  try {
    const res = await fetch(req);
    if (res.ok) await cache.put(req, res.clone());
    return res;
  } catch (err) {
    const cached = await cache.match(req);
    if (cached) return cached;
    throw err;
  }
}
