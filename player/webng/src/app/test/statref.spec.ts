// statref.spec.ts -- verifie que Engine expand %NOM% (TXT_STAT_REF, cf. engine.ts:
// substituteStatRefs) dans le texte narratif et les libelles de choix.
// GameData construit a la main (pas de story.json) : cible precisement
// l'expansion elle-meme, sans dependre d'une aventure reelle.

import { Engine, type SaveData, type SaveStore } from "../engine/engine";
import { OP_STAT_ADD, TXT_STAT_REF } from "../engine/format";
import type { GameData, StoryData } from "../engine/story";
import type { SectionBody } from "../engine/section";

const NO_COND = { clauses: [] };

class MemorySaveStore implements SaveStore {
  private data: SaveData | null = null;
  read(): SaveData | null {
    return this.data;
  }
  write(data: SaveData): void {
    this.data = data;
  }
}

function statRef(index: number): string {
  return String.fromCharCode(TXT_STAT_REF, index);
}

function makeGameData(): GameData {
  const story: StoryData = {
    scoreOn: false,
    movesOn: false,
    nSections: 1,
    nStats: 1,
    nItems: 0,
    nFlags: 0,
    nIntro: 0,
    start: 0,
    localBase: 0,
    statInit: [5],
    statMin: [0],
    statMaxDef: [10],
    statHidden: 0,
    itemDefault: new Uint8Array(0),
    flagDefault: new Uint8Array(0),
    statName: ["JOURS"],
    itemLabel: [],
    title: "Test",
    adventureVersion: "",
    introIdx: [],
    itemAtk: [],
    itemDmg: [],
    itemArmor: [],
    combatAtt: 0xff,
    combatHp: 0xff,
    combatBaseDmg: 0,
  };

  const start: SectionBody = {
    mode: 0,
    ending: 0,
    image: 0xffff,
    splash: null,
    combat: null,
    input: null,
    onEnter: [],
    onExit: [],
    texts: [{ cond: NO_COND, style: 0, text: `Il vous reste ${statRef(0)} jours.` }],
    choices: [
      {
        cond: NO_COND,
        // ~ add JOURS 3, puis retour a la meme section (goto 0) : verifie
        // que le rendu SUIVANT reflete la valeur mise a jour, pas une
        // substitution figee au chargement.
        effects: [{ guard: NO_COND, op: OP_STAT_ADD, a0: 0, a1: 3, a2: 0 }],
        target: 0,
        label: `Attendre ${statRef(0)} jours`,
      },
    ],
  };

  return {
    story,
    sections: [start],
    sectionChapter: [0],
    uiStrings: new Array(60).fill(""),
  };
}

describe("%NOM% (TXT_STAT_REF)", () => {
  it("expand la valeur courante dans le texte narratif et le libelle de choix", () => {
    const engine = new Engine(makeGameData(), new MemorySaveStore(), () => {});
    engine.startNewGame(1);
    const view = engine.getView();
    expect(view.kind).toBe("scene");
    if (view.kind !== "scene") throw new Error("unreachable");
    expect(view.scene.texts[0].text).toBe("Il vous reste 5 jours.");
    expect(view.scene.choices[0].label).toBe("Attendre 5 jours");
  });

  it("reflete un changement de stat au rendu suivant (pas de cache fige)", () => {
    const engine = new Engine(makeGameData(), new MemorySaveStore(), () => {});
    engine.startNewGame(1); // statVal[0] = statInit[0] = 5
    engine.choose(0); // ~ add JOURS 3 -> 8, puis goto la meme section
    const view = engine.getView();
    if (view.kind !== "scene") throw new Error("unreachable");
    expect(view.scene.texts[0].text).toBe("Il vous reste 8 jours.");
    expect(view.scene.choices[0].label).toBe("Attendre 8 jours");
  });
});
