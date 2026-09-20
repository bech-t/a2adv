// story.ts -- chargement du JSON produit par compiler/a2c/webjson.py
// (`python3 -m a2c.webjson <adv> -o story.json`) et mise en forme pour le
// reste du moteur : StoryData (preambule) + GameData (sections + chapitres).
//
// Avant cette version, ce fichier decodait STORY.DAT/APP.LNG (le format
// binaire lu par les players natifs), section par section, avec un curseur
// d'octets (cf. l'ancien reader.ts). Le JSON produit par webjson.py contient
// deja la MEME information -- resolue, indices calcules, flags locaux
// tries -- par le meme compilateur (compiler/a2c/symbols.py reste l'UNIQUE
// endroit qui fait ce travail ; webjson.py ne fait que le serialiser en
// JSON plutot qu'en octets empaquetes, cf. sa docstring). Charger ce JSON
// est donc un JSON.parse, pas un nouveau decodeur a ecrire -- seule cette
// fonction change, `state.ts`/`combat.ts`/`engine.ts` ne voient aucune
// difference : StoryData et SectionBody gardent exactement la meme forme
// qu'avant.

import type { SectionBody } from "./section";

export interface StoryData {
  scoreOn: boolean;
  movesOn: boolean;
  nSections: number;
  nStats: number;
  nItems: number;
  nFlags: number;
  nIntro: number;
  start: number;
  localBase: number;
  statInit: number[];
  statMin: number[];
  statMaxDef: number[];
  statHidden: number; // bit i = stat i masquee du bandeau d'etat
  itemDefault: Uint8Array; // bitset, ceil(nItems/8) octets
  flagDefault: Uint8Array; // bitset, ceil(nFlags/8) octets
  statName: string[];
  itemLabel: string[];
  title: string;
  adventureVersion: string; // @version, "" si absente
  introIdx: number[];
  itemAtk: number[]; // signes
  itemDmg: number[];
  itemArmor: number[];
  combatAtt: number; // 0xFF = pas de stat d'attaque declaree
  combatHp: number; // 0xFF = pas de stat de PV declaree
  combatBaseDmg: number;
}

export interface GameData {
  story: StoryData;
  sections: SectionBody[];
  /** Meme longueur que sections : numero de CHAPITRE (@chapter) de chaque
   * section -- remplace l'ancien fileOfSection (1 fichier = 1 chapitre chez
   * les players natifs, cf. story.c) : ici il n'y a plus de fichiers du
   * tout, juste ce numero, utilise pour la meme chose (state.clearLocals()
   * au changement de chapitre, cf. engine.ts). */
  sectionChapter: number[];
  uiStrings: string[]; // deja fusionnees avec les surcharges @ui (webjson.py)
}

/** Fournie par l'appelant : fetch().then(r => r.text()) dans le navigateur
 * (assets/loader.ts), lecture de fichier dans les tests Node (cf.
 * src/test/hostplay.test.ts) -- meme separation d'intention que diskio.h
 * cote natif, juste une seule fonction puisqu'un fichier entier tient
 * toujours en memoire ici. */
export type StoryFetcher = () => Promise<string>;

// Forme du JSON tel qu'ecrit par compiler/a2c/webjson.py -- volontairement
// PAS exportee : c'est un detail d'encodage du fichier, seules StoryData/
// GameData (ci-dessus) sont l'API que le reste du moteur consomme.
interface RuntimeJson {
  scoreOn: boolean;
  movesOn: boolean;
  start: number;
  localBase: number;
  title: string;
  adventureVersion: string;
  stats: { name: string; init: number; min: number; max: number; hidden: boolean }[];
  items: { label: string; defaultOn: boolean; atk: number; dmg: number; armor: number }[];
  flags: { defaultOn: boolean }[];
  introIndex: number[];
  combatAttackIndex: number;
  combatHpIndex: number;
  combatBaseDmg: number;
  uiStrings: string[];
  sections: (SectionBody & { chapter: number })[];
}

/** Empaquete un tableau de booleens en bitset (bit i = element i) -- meme
 * disposition que items_default/flags_default cote STORY.DAT, pour que
 * story.ts garde des bitsets (itemDefaultBit/flagDefaultBit ci-dessous et
 * state.ts n'ont pas besoin de changer). */
function packBitset(bits: boolean[]): Uint8Array {
  const out = new Uint8Array(Math.ceil(bits.length / 8));
  bits.forEach((on, i) => {
    if (on) out[i >> 3] |= 1 << (i & 7);
  });
  return out;
}

export async function loadStory(fetchStory: StoryFetcher): Promise<GameData> {
  const raw = JSON.parse(await fetchStory()) as RuntimeJson;

  const story: StoryData = {
    scoreOn: raw.scoreOn,
    movesOn: raw.movesOn,
    nSections: raw.sections.length,
    nStats: raw.stats.length,
    nItems: raw.items.length,
    nFlags: raw.flags.length,
    nIntro: raw.introIndex.length,
    start: raw.start,
    localBase: raw.localBase,
    statInit: raw.stats.map((s) => s.init),
    statMin: raw.stats.map((s) => s.min),
    statMaxDef: raw.stats.map((s) => s.max),
    statHidden: raw.stats.reduce((mask, s, i) => (s.hidden ? mask | (1 << i) : mask), 0),
    itemDefault: packBitset(raw.items.map((it) => it.defaultOn)),
    flagDefault: packBitset(raw.flags.map((fl) => fl.defaultOn)),
    statName: raw.stats.map((s) => s.name),
    itemLabel: raw.items.map((it) => it.label),
    title: raw.title,
    adventureVersion: raw.adventureVersion,
    introIdx: raw.introIndex.slice(),
    itemAtk: raw.items.map((it) => it.atk),
    itemDmg: raw.items.map((it) => it.dmg),
    itemArmor: raw.items.map((it) => it.armor),
    combatAtt: raw.combatAttackIndex,
    combatHp: raw.combatHpIndex,
    combatBaseDmg: raw.combatBaseDmg,
  };

  return {
    story,
    sections: raw.sections,
    sectionChapter: raw.sections.map((s) => s.chapter),
    uiStrings: raw.uiStrings,
  };
}

export function isStatHidden(story: StoryData, i: number): boolean {
  return ((story.statHidden >> i) & 1) !== 0;
}

function bitset(bytes: Uint8Array, i: number): number {
  return (bytes[i >> 3] >> (i & 7)) & 1;
}
export function itemDefaultBit(story: StoryData, i: number): number {
  return bitset(story.itemDefault, i);
}
export function flagDefaultBit(story: StoryData, i: number): number {
  return bitset(story.flagDefault, i);
}
