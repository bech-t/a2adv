// offline.ts -- jeu hors ligne : enregistrement du Service Worker (public/sw.js),
// telechargement d'une aventure dans son cache, et annonce d'une nouvelle
// version du site.
//
// Le cache des aventures (`a2adv-adv`, meme nom que dans sw.js) est rempli de
// deux manieres : par le Service Worker au fil de la lecture, et par
// `download()`, qui recupere d'un coup tous les fichiers d'une aventure. Le
// dossier d'une aventure porte le hash de son contenu (cf. catalog.ts) : un
// fichier qui y est ne devient jamais perime.

import { Injectable, isDevMode, signal } from "@angular/core";
import type { CatalogEntry } from "./catalog";

const ADV_CACHE = "a2adv-adv";
const PARALLEL_DOWNLOADS = 4;

export interface DownloadProgress {
  done: number;
  total: number;
}

@Injectable({ providedIn: "root" })
export class Offline {
  /** Le navigateur sait-il garder des fichiers hors ligne ? */
  readonly supported = typeof caches !== "undefined";
  /** Une nouvelle version du site est installee et attend le feu vert. */
  readonly updateReady = signal(false);
  /** Ids des aventures dont tous les fichiers sont dans le cache. */
  readonly available = signal<ReadonlySet<string>>(new Set());
  /** Telechargements en cours, par id d'aventure. */
  readonly progress = signal<Readonly<Record<string, DownloadProgress>>>({});

  private registration: ServiceWorkerRegistration | null = null;
  private applying = false;

  /** Enregistre le Service Worker (production seulement : en dev il figerait
   * des reponses que le serveur de dev attend de pouvoir changer). */
  register(): void {
    if (isDevMode() || !("serviceWorker" in navigator)) return;
    window.addEventListener("load", () => void this.watch());
  }

  private async watch(): Promise<void> {
    const sw = navigator.serviceWorker;
    const reg = await sw.register("sw.js");
    this.registration = reg;

    const announce = (worker: ServiceWorker) =>
      worker.addEventListener("statechange", () => {
        // installe alors qu'une version tourne deja : c'est une mise a jour
        if (worker.state === "installed" && sw.controller) this.updateReady.set(true);
      });
    if (reg.waiting && sw.controller) this.updateReady.set(true);
    reg.addEventListener("updatefound", () => reg.installing && announce(reg.installing));

    // La premiere installation prend aussi la main (controllerchange) : on ne
    // recharge que si l'utilisateur a demande la mise a jour.
    sw.addEventListener("controllerchange", () => {
      if (this.applying) window.location.reload();
    });
    document.addEventListener("visibilitychange", () => {
      if (document.visibilityState === "visible") void reg.update();
    });
  }

  /** Active la version en attente ; la page se recharge toute seule. */
  applyUpdate(): void {
    this.applying = true;
    this.registration?.waiting?.postMessage("SKIP_WAITING");
  }

  private absolute(path: string): string {
    return new URL(path, document.baseURI).href;
  }

  private async cachedUrls(): Promise<{ cache: Cache; urls: Set<string> }> {
    const cache = await caches.open(ADV_CACHE);
    return { cache, urls: new Set((await cache.keys()).map((r) => r.url)) };
  }

  /** Recalcule quelles aventures du catalogue sont completes dans le cache, et
   * supprime les fichiers des versions qui n'y figurent plus. */
  async refresh(entries: readonly CatalogEntry[]): Promise<void> {
    if (!this.supported) return;
    try {
      const { cache, urls } = await this.cachedUrls();
      const bases = entries.map((e) => this.absolute(e.base + "/"));
      const advRoot = this.absolute("adventures/");
      for (const url of urls) {
        if (url.startsWith(advRoot) && !bases.some((b) => url.startsWith(b))) {
          await cache.delete(url);
          urls.delete(url);
        }
      }
      this.available.set(
        new Set(
          entries
            .filter((e) => e.files.every((f) => urls.has(this.absolute(f))))
            .map((e) => e.id),
        ),
      );
    } catch {
      // cache inutilisable (navigation privee...) : rien n'est hors ligne
      this.available.set(new Set());
    }
  }

  isAvailable(id: string): boolean {
    return this.available().has(id);
  }

  /** Telecharge tous les fichiers de l'aventure ; leve une Error lisible si un
   * fichier manque ou si le reseau coupe (les fichiers deja recus restent). */
  async download(entry: CatalogEntry, all: readonly CatalogEntry[]): Promise<void> {
    if (!this.supported) throw new Error("ce navigateur ne peut pas garder l'aventure hors ligne");
    // demande au navigateur de ne pas purger ces fichiers en cas de manque de place
    void navigator.storage?.persist?.();
    const { cache, urls } = await this.cachedUrls();
    const queue = entry.files.filter((f) => !urls.has(this.absolute(f)));
    const total = entry.files.length;
    let done = total - queue.length;
    const report = () => this.progress.update((p) => ({ ...p, [entry.id]: { done, total } }));
    report();
    try {
      const worker = async () => {
        for (let f = queue.shift(); f !== undefined; f = queue.shift()) {
          const res = await fetch(f);
          if (!res.ok) throw new Error(`${f} : HTTP ${res.status}`);
          await cache.put(this.absolute(f), res);
          ++done;
          report();
        }
      };
      await Promise.all(Array.from({ length: PARALLEL_DOWNLOADS }, worker));
    } catch (e) {
      throw new Error(
        `téléchargement interrompu (${e instanceof Error ? e.message : String(e)})`,
        { cause: e },
      );
    } finally {
      this.progress.update((p) => {
        const { [entry.id]: _, ...rest } = p;
        return rest;
      });
      await this.refresh(all);
    }
  }

  /** Supprime les fichiers de l'aventure du cache (ses sauvegardes restent). */
  async remove(entry: CatalogEntry, all: readonly CatalogEntry[]): Promise<void> {
    if (!this.supported) return;
    const cache = await caches.open(ADV_CACHE);
    await Promise.all(entry.files.map((f) => cache.delete(this.absolute(f))));
    await this.refresh(all);
  }
}
