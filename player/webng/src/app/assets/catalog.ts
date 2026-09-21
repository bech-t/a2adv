// catalog.ts -- le catalogue des aventures publiees (catalog.json, produit par
// `python3 -m a2c.site`, cf. compiler/a2c/site.py). Les chemins qu'il contient
// sont relatifs a la page : ils suivent le <base href> du site, qu'il soit servi
// a la racine d'un domaine ou dans un sous-dossier. Le dossier d'une aventure
// (`base`) porte le hash de son contenu : il ne change jamais une fois publie.

import { Injectable, signal } from "@angular/core";

export interface CatalogEntry {
  id: string;
  title: string;
  author: string;
  version: string;
  lang: string;
  description: string;
  license: string;         // nom de la licence du contenu, "" si non renseigne
  cover: string | null;
  base: string;            // dossier de l'aventure : adventures/<id>/<hash>
  story: string;           // <base>/story.json
  hash: string;            // contenu du dossier : change des que l'aventure change
  sections: number;
  images: number;
  files: string[];         // tous les fichiers du dossier, pour le telechargement hors ligne
  bytes: number;           // taille de ces fichiers
}

export interface Catalog {
  format: number;
  adventures: CatalogEntry[];
}

const CATALOG_FORMAT = 1;

@Injectable({ providedIn: "root" })
export class CatalogService {
  readonly catalog = signal<Catalog | null>(null);
  readonly error = signal<string | null>(null);
  private loading: Promise<void> | null = null;

  /** Charge le catalogue une seule fois ; les appels suivants attendent le meme. */
  load(): Promise<void> {
    this.loading ??= this.fetchCatalog();
    return this.loading;
  }

  entry(id: string): CatalogEntry | undefined {
    return this.catalog()?.adventures.find((a) => a.id === id);
  }

  private async fetchCatalog(): Promise<void> {
    try {
      const res = await fetch("catalog.json");
      if (!res.ok) throw new Error(`catalog.json introuvable (HTTP ${res.status})`);
      const data = (await res.json()) as Catalog;
      if (data.format !== CATALOG_FORMAT) {
        throw new Error(`format de catalogue ${data.format} non pris en charge`);
      }
      this.catalog.set(data);
    } catch (e) {
      this.loading = null;    // permet de reessayer
      this.error.set(e instanceof Error ? e.message : String(e));
    }
  }
}
