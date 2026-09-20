// loader.ts -- charge le JSON d'une aventure (son URL vient du catalogue,
// cf. catalog.ts) et resout le chemin d'une image. Une seule fonction de
// lecture (fetch), le fichier entier tient en memoire (cf. story.ts).
// Les chemins sont relatifs a la page (<base href>).

import type { StoryFetcher } from "../engine/story";

export function storyFetcher(storyUrl: string): StoryFetcher {
  return async () => {
    const res = await fetch(storyUrl);
    if (!res.ok) throw new Error(`histoire introuvable (HTTP ${res.status})`);
    return res.text();
  };
}

/** Chemin d'une image numerotee (IMGnn.webp, cf. compiler/a2c/site.py) dans
 * le dossier de l'aventure (`base` du catalogue). Un 404 est tolere par
 * l'appelant (onError) : une aventure sans export web de ses visuels reste
 * jouable en texte. */
export function imagePath(base: string, asset: number): string {
  const n = String(asset).padStart(2, "0");
  return `${base}/img/IMG${n}.webp`;
}
