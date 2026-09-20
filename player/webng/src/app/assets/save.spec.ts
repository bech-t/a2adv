import { beforeEach, describe, expect, it } from "vitest";
import type { SaveData } from "../engine/engine";
import {
  SLOT_COUNT,
  clearSlot,
  hasAnySave,
  listSlots,
  localSaveStore,
  parseSaveFile,
  readSlot,
  toSaveFile,
  writeSlot,
} from "./save";

const DATA: SaveData = {
  section: 4,
  statVal: [10, 3],
  statMax: [10, 12],
  itemBits: [5],
  flagBits: [0, 1],
  score: 20,
  moves: 7,
};

describe("emplacements de sauvegarde", () => {
  beforeEach(() => localStorage.clear());

  it("commencent vides, et s'ecrivent independamment", () => {
    expect(listSlots("a")).toEqual(Array(SLOT_COUNT).fill(null));
    expect(hasAnySave("a")).toBe(false);

    expect(writeSlot("a", 2, { savedAt: "2026-09-20T10:00:00Z", hash: "h1", data: DATA })).toBe(true);
    expect(readSlot("a", 2)?.data).toEqual(DATA);
    expect(readSlot("a", 1)).toBeNull();
    expect(readSlot("b", 2)).toBeNull();
    expect(hasAnySave("a")).toBe(true);

    clearSlot("a", 2);
    expect(hasAnySave("a")).toBe(false);
  });

  it("le port du moteur ecrit la date et le hash dans l'emplacement choisi", () => {
    const store = localSaveStore("a", 3, "h9");
    expect(store.read()).toBeNull();
    store.write(DATA);
    expect(store.read()).toEqual(DATA);
    const slot = readSlot("a", 3);
    expect(slot?.hash).toBe("h9");
    expect(Number.isNaN(Date.parse(slot?.savedAt ?? ""))).toBe(false);
    expect(readSlot("a", 1)).toBeNull();
  });

  it("l'ancienne sauvegarde unique devient l'emplacement 1", () => {
    localStorage.setItem("a2adv:save:a", JSON.stringify(DATA));
    expect(readSlot("a", 1)?.data).toEqual(DATA);
    expect(localStorage.getItem("a2adv:save:a")).toBeNull();
    // et n'ecrase pas un emplacement 1 deja utilise
    localStorage.setItem("a2adv:save:b", JSON.stringify({ ...DATA, score: 1 }));
    writeSlot("b", 1, { savedAt: "", hash: "", data: DATA });
    expect(readSlot("b", 1)?.data.score).toBe(20);
  });

  it("un contenu illisible vaut un emplacement vide", () => {
    localStorage.setItem("a2adv:save:a:1", "{pas du json");
    expect(readSlot("a", 1)).toBeNull();
  });
});

describe("fichier d'export", () => {
  const slot = { savedAt: "2026-09-20T10:00:00Z", hash: "h1", data: DATA };

  it("fait l'aller-retour", () => {
    const text = JSON.stringify(toSaveFile("a", slot));
    expect(parseSaveFile(text, "a")).toEqual(slot);
  });

  it("refuse ce qui n'est pas une sauvegarde de cette aventure", () => {
    const file = toSaveFile("a", slot);
    const cases: [string, string][] = [
      ["pas du json", "n'est pas une sauvegarde"],
      ["{}", "n'est pas une sauvegarde"],
      [JSON.stringify({ ...file, format: 2 }), "format"],
      [JSON.stringify({ ...file, adventure: "b" }), "celle de « b »"],
      [JSON.stringify({ ...file, data: { ...DATA, statVal: "x" } }), "illisible"],
      [JSON.stringify({ ...file, data: { ...DATA, itemBits: [300] } }), "illisible"],
      [JSON.stringify({ ...file, data: { ...DATA, section: -1 } }), "illisible"],
    ];
    for (const [text, why] of cases) {
      expect(() => parseSaveFile(text, "a")).toThrow(why);
    }
  });

  it("tolere l'absence de date et de hash", () => {
    const file = { ...toSaveFile("a", slot), savedAt: undefined, hash: undefined };
    expect(parseSaveFile(JSON.stringify(file), "a")).toEqual({ savedAt: "", hash: "", data: DATA });
  });
});
