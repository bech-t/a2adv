// save.ts -- sauvegardes locales (localStorage), en plusieurs emplacements par
// aventure, avec export/import sous forme de fichier JSON.
//
// Pas de format binaire ProDOS a reproduire ici (cf. save.c cote natif,
// SAVE0.DAT) : l'etat a sauvegarder (stats/objets/flags/score/moves/section)
// reste petit quelle que soit la taille de l'aventure, un objet JSON suffit.
// Chaque emplacement garde aussi la date et le hash de l'aventure qui l'a
// ecrit : le hash dit si la sauvegarde vient d'une autre version du texte.

import type { SaveData, SaveSlotInfo, SaveStore } from "../engine/engine";

export const SLOT_COUNT = 3;

/** Contenu d'un emplacement. */
export interface SaveSlot {
  savedAt: string;   // date ISO ; "" si inconnue (sauvegarde d'avant les emplacements)
  hash: string;      // hash de l'aventure qui l'a ecrite ; "" si inconnu
  data: SaveData;
}

/** Fichier d'export d'un emplacement. */
export interface SaveFile extends SaveSlot {
  app: "a2adv";
  format: 1;
  adventure: string;
}

const FILE_FORMAT = 1;

function slotKey(advId: string, slot: number): string {
  return `a2adv:save:${advId}:${slot}`;
}

/** Cle de l'unique sauvegarde d'avant les emplacements. */
function legacyKey(advId: string): string {
  return `a2adv:save:${advId}`;
}

function parseSlot(raw: string | null): SaveSlot | null {
  if (!raw) return null;
  try {
    const slot = JSON.parse(raw) as SaveSlot;
    return slot && typeof slot === "object" && slot.data ? slot : null;
  } catch {
    return null;
  }
}

/** L'ancienne sauvegarde unique devient l'emplacement 1 (s'il est libre). */
function migrateLegacy(advId: string): void {
  try {
    const raw = localStorage.getItem(legacyKey(advId));
    if (raw === null) return;
    if (localStorage.getItem(slotKey(advId, 1)) === null) {
      const data = JSON.parse(raw) as SaveData;
      const slot: SaveSlot = { savedAt: "", hash: "", data };
      localStorage.setItem(slotKey(advId, 1), JSON.stringify(slot));
    }
    localStorage.removeItem(legacyKey(advId));
  } catch {
    // stockage indisponible ou contenu illisible : on laisse tel quel
  }
}

export function readSlot(advId: string, slot: number): SaveSlot | null {
  migrateLegacy(advId);
  try {
    return parseSlot(localStorage.getItem(slotKey(advId, slot)));
  } catch {
    return null; // stockage indisponible (navigation privee...) : comme s'il etait vide
  }
}

/** Ecrit un emplacement ; false si le navigateur refuse (quota, navigation privee). */
export function writeSlot(advId: string, slot: number, value: SaveSlot): boolean {
  try {
    localStorage.setItem(slotKey(advId, slot), JSON.stringify(value));
    return true;
  } catch {
    return false;
  }
}

export function clearSlot(advId: string, slot: number): void {
  try {
    localStorage.removeItem(slotKey(advId, slot));
  } catch {
    // rien a faire
  }
}

/** Les SLOT_COUNT emplacements (null = vide), dans l'ordre. */
export function listSlots(advId: string): (SaveSlot | null)[] {
  return Array.from({ length: SLOT_COUNT }, (_, i) => readSlot(advId, i + 1));
}

export function hasAnySave(advId: string): boolean {
  return listSlots(advId).some((s) => s !== null);
}

/** Description lisible d'un emplacement ; "" s'il est vide. */
export function slotSummary(slot: SaveSlot | null): string {
  if (!slot) return "";
  const parts: string[] = [];
  if (slot.data.moves > 0) parts.push(`${slot.data.moves} mouvements`);
  if (slot.data.score > 0) parts.push(`score ${slot.data.score}`);
  if (slot.savedAt) {
    parts.push(
      new Date(slot.savedAt).toLocaleString("fr-FR", {
        day: "numeric",
        month: "short",
        hour: "2-digit",
        minute: "2-digit",
      }),
    );
  }
  return parts.join(" · ") || "partie sauvegardée";
}

/** Port d'E/S du moteur : `Engine` lit et ecrit la partie de l'emplacement
 * courant (`slot` au depart), et peut en changer via select(). */
export function localSaveStore(advId: string, slot: number, hash: string): SaveStore {
  let current = slot;
  return {
    read: () => readSlot(advId, current)?.data ?? null,
    write: (data: SaveData) =>
      void writeSlot(advId, current, { savedAt: new Date().toISOString(), hash, data }),
    slots: (): SaveSlotInfo[] =>
      listSlots(advId).map((s, i) => ({
        slot: i + 1,
        summary: slotSummary(s),
        current: i + 1 === current,
      })),
    select: (n: number) => {
      if (Number.isInteger(n) && n >= 1 && n <= SLOT_COUNT) current = n;
    },
  };
}

export function toSaveFile(advId: string, slot: SaveSlot): SaveFile {
  return { app: "a2adv", format: FILE_FORMAT, adventure: advId, ...slot };
}

export function saveFileName(advId: string, slot: number): string {
  return `a2adv-${advId}-emplacement${slot}.json`;
}

const isNumber = (v: unknown): v is number => typeof v === "number" && Number.isFinite(v);
const isNumberArray = (v: unknown): v is number[] => Array.isArray(v) && v.every(isNumber);
const isByteArray = (v: unknown): v is number[] =>
  isNumberArray(v) && v.every((n) => Number.isInteger(n) && n >= 0 && n <= 255);

/** Lit un fichier d'export pour l'aventure `advId`. Leve une Error au message
 * lisible si ce n'est pas une sauvegarde de cette aventure. */
export function parseSaveFile(text: string, advId: string): SaveSlot {
  let file: Partial<SaveFile>;
  try {
    file = JSON.parse(text) as Partial<SaveFile>;
  } catch {
    throw new Error("ce fichier n'est pas une sauvegarde");
  }
  if (!file || file.app !== "a2adv" || typeof file.data !== "object" || file.data === null) {
    throw new Error("ce fichier n'est pas une sauvegarde");
  }
  if (file.format !== FILE_FORMAT) {
    throw new Error("format de sauvegarde non pris en charge");
  }
  if (file.adventure !== advId) {
    throw new Error(`cette sauvegarde est celle de « ${String(file.adventure)} »`);
  }
  const d = file.data as Partial<SaveData>;
  const valid =
    Number.isInteger(d.section) &&
    (d.section as number) >= 0 &&
    isNumberArray(d.statVal) &&
    isNumberArray(d.statMax) &&
    isByteArray(d.itemBits) &&
    isByteArray(d.flagBits) &&
    isNumber(d.score) &&
    isNumber(d.moves);
  if (!valid) throw new Error("sauvegarde illisible ou incomplète");
  return {
    savedAt: typeof file.savedAt === "string" ? file.savedAt : "",
    hash: typeof file.hash === "string" ? file.hash : "",
    data: d as SaveData,
  };
}
