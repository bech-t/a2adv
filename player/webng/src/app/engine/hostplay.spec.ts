// hostplay.spec.ts -- rejoue combat_demo en Node pur, sans navigateur.
// Verifie que engine/ (zero dependance framework) charge le VRAI JSON
// produit par a2c.webjson et applique les bonnes regles, independamment du
// framework d'affichage. Le JSON est regenere a chaque lancement : jamais un
// fixture fige qui se perime silencieusement quand a2c/webjson.py change.

import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

import { Engine, type SaveData, type SaveStore } from "./engine";
import { END_LOSE, END_WIN, Snd } from "./format";
import { loadStory } from "./story";

// resolve() plutot que fileURLToPath(import.meta.url) : le builder de test
// Angular (@angular/build:unit-test) transforme ce fichier avant de
// l'executer, import.meta.url n'y pointe pas vers un chemin file:// -- le
// process reste lance depuis la racine du projet (webng/), donc
// process.cwd() est fiable ici.
const COMBAT_DEMO = resolve(process.cwd(), "../../adventures/combat_demo/combat_demo.adv");

function readStoryJson(): Promise<string> {
  const out = join(mkdtempSync(join(tmpdir(), "combat-")), "story.json");
  execFileSync("python3", ["-m", "a2c.webjson", COMBAT_DEMO, "-o", out], {
    cwd: COMPILER_DIR,
  });
  return Promise.resolve(readFileSync(out, "utf-8"));
}

class MemorySaveStore implements SaveStore {
  private data: SaveData | null = null;
  read(): SaveData | null {
    return this.data;
  }
  write(data: SaveData): void {
    this.data = data;
  }
}

async function setup() {
  const gameData = await loadStory(readStoryJson);
  const sounds: number[] = [];
  const engine = new Engine(gameData, new MemorySaveStore(), (id) => sounds.push(id));
  return { engine, sounds };
}

describe("combat_demo", () => {
  it("decode l'en-tete et le preambule sans erreur", async () => {
    const { engine } = await setup();
    expect(engine.story.title).toBe("Démo Combat");
    expect(engine.story.nStats).toBe(2);
    expect(engine.story.statName).toEqual(["HABILETE", "ENDURANCE"]);
    expect(engine.story.nItems).toBe(1);
    expect(engine.story.itemLabel).toEqual(["Épée courte"]);
    expect(engine.story.combatBaseDmg).toBe(2);
  });

  it("affiche la scene d'entree avec le bon choix visible", async () => {
    const { engine } = await setup();
    engine.startNewGame(1);
    const view = engine.getView();
    expect(view.kind).toBe("scene");
    if (view.kind !== "scene") throw new Error("unreachable");
    expect(view.scene.choices.map((c) => c.label)).toEqual([
      "Arracher l'épée du cadavre",
      "Affronter le gobelin",
    ]);
  });

  it("ramasser l'epee joue SND_PICKUP, donne l'objet, et fait disparaitre le choix", async () => {
    const { engine, sounds } = await setup();
    engine.startNewGame(1);
    engine.choose(0); // "Arracher l'epee du cadavre"

    expect(sounds).toContain(Snd.PICKUP);
    expect(engine.state.itemGet(0)).toBe(true);

    const view = engine.getView();
    expect(view.kind).toBe("scene");
    if (view.kind !== "scene") throw new Error("unreachable");
    // {not has epee} devient faux : un seul choix reste visible.
    expect(view.scene.choices.map((c) => c.label)).toEqual(["Affronter le gobelin"]);
  });

  it("fuir au 1er round inflige exactement le coup gratuit et revient a l'entree", async () => {
    const { engine } = await setup();
    engine.startNewGame(1);
    engine.choose(0); // ramasse l'epee (atk+1/dmg+2, armor+0)
    engine.choose(0); // "Affronter le gobelin" -> entre en combat

    let view = engine.getView();
    expect(view.kind).toBe("combat");
    if (view.kind !== "combat") throw new Error("unreachable");
    expect(view.combat.enemyHp).toBe(9);
    expect(view.combat.heroHp).toBe(14); // ENDURANCE initiale (@stat ENDURANCE 14 0 20)
    expect(view.combat.canFlee).toBe(true);

    engine.combatFlee(); // degats = max(1, 3 - armure(0)) = 3, deterministe (pas de de)

    view = engine.getView();
    // @flee entree, sans message d'issue -> retour immediat a la scene.
    expect(view.kind).toBe("scene");
    if (view.kind !== "scene") throw new Error("unreachable");
    expect(view.scene.choices.map((c) => c.label)).toEqual(["Affronter le gobelin"]);
    expect(engine.state.statVal[1]).toBe(11); // 14 - 3
  });

  it("un combat mene jusqu'au bout se termine par une fin coherente (victoire ou defaite)", async () => {
    const { engine, sounds } = await setup();
    engine.startNewGame(2);
    engine.choose(0); // epee
    engine.choose(0); // combat

    let view = engine.getView();
    let guard = 0;
    while (view.kind === "combat" && guard++ < 100) {
      engine.combatAttack();
      view = engine.getView();
      if (view.kind === "combat" && view.combat.outcome !== null) {
        engine.continueAfterCombat();
        view = engine.getView();
      }
    }
    expect(guard).toBeLessThan(100); // sinon : combat qui ne termine jamais, cf. combat.c (>=1 degat garanti)

    expect(view.kind).toBe("ending");
    if (view.kind !== "ending") throw new Error("unreachable");

    if (view.ending.ending === END_WIN) {
      expect(engine.state.score).toBe(100); // ~ score 100 du winFx de combat_gobelin
      expect(sounds).toContain(Snd.WIN);
    } else {
      expect(view.ending.ending).toBe(END_LOSE);
      expect(engine.state.statVal[1]).toBe(0); // ENDURANCE tombee a 0
      expect(sounds).toContain(Snd.LOSE);
    }

    // Dans les deux cas, un retour au menu doit rester possible.
    engine.acknowledgeEnding();
    expect(engine.getView().kind).toBe("menu");
  });

  it("sauvegarde/chargement restaure exactement l'etat courant", async () => {
    const { engine } = await setup();
    engine.startNewGame(3);
    engine.choose(0); // epee (score inchange ici, juste l'objet)
    engine.save();

    const savedItems = Array.from(engine.state.itemBits);

    // Nouvelle "session" (nouvel Engine, meme SaveStore) : verifie que la
    // sauvegarde survit independamment de l'instance moteur.
    const gameData = await loadStory(readStoryJson);
    const store = new MemorySaveStore();
    store.write({
      section: 0,
      statVal: engine.state.statVal,
      statMax: engine.state.statMax,
      itemBits: savedItems,
      flagBits: Array.from(engine.state.flagBits),
      score: engine.state.score,
      moves: engine.state.moves,
    });
    // section=0 n'est pas forcement "entree" une fois compile -- on ne
    // verifie ici que la restauration de l'ETAT, pas la reprise de section
    // (deja couverte indirectement par continueGame() lui-meme).
    const engine2 = new Engine(gameData, store, () => {});
    expect(engine2.hasSave()).toBe(true);
    engine2.continueGame();
    expect(Array.from(engine2.state.itemBits)).toEqual(savedItems);
  });

  it("refuse une sauvegarde d'une autre version de l'aventure", async () => {
    const { engine } = await setup();
    engine.startNewGame(3);
    engine.save();
    const good = engine.state;

    const gameData = await loadStory(readStoryJson);
    const base = {
      section: 0,
      statVal: good.statVal.slice(),
      statMax: good.statMax.slice(),
      itemBits: Array.from(good.itemBits),
      flagBits: Array.from(good.flagBits),
      score: 0,
      moves: 0,
    };
    const bad: Partial<SaveData>[] = [
      { section: 9999 },                                      // section inexistante
      { statVal: [...base.statVal, 1] },                      // une stat de plus
      { statMax: base.statMax.slice(1) },                     // une stat de moins
      { itemBits: [...base.itemBits, 0] },                    // trop d'objets
      { flagBits: [...base.flagBits, 0] },                    // trop de drapeaux
    ];
    for (const patch of bad) {
      const store = new MemorySaveStore();
      store.write({ ...base, ...patch });
      const engine2 = new Engine(gameData, store, () => {});
      expect(engine2.continueGame()).toBe(false);
      expect(engine2.getView().kind).toBe("menu");
    }
    const ok = new MemorySaveStore();
    ok.write(base);
    expect(new Engine(gameData, ok, () => {}).continueGame()).toBe(true);
  });
});

// --- @splash ---------------------------------------------------------------

const COMPILER_DIR = resolve(process.cwd(), "../../compiler");

const SPLASH_ADV = `
@title Splash
@start a
:: a
@splash titre 2
Premiere.
* [suite] -> b
:: b
Deuxieme.
* [retour] -> a
* [autre] -> c
:: c
@splash toujours always
Troisieme.
* [retour] -> c
* [a] -> a
`;

async function splashEngine() {
  const dir = mkdtempSync(join(tmpdir(), "splash-"));
  const adv = join(dir, "s.adv");
  writeFileSync(adv, SPLASH_ADV);
  const out = join(dir, "story.json");
  execFileSync("python3", ["-m", "a2c.webjson", adv, "-o", out], {
    cwd: COMPILER_DIR,
  });
  const gameData = await loadStory(() => Promise.resolve(readFileSync(out, "utf-8")));
  return new Engine(gameData, new MemorySaveStore(), () => {});
}

describe("@splash", () => {
  it("s'affiche avant le texte, puis ne se rejoue pas en revenant dans la section", async () => {
    const engine = await splashEngine();
    engine.startNewGame(1);
    let v = engine.getView();
    expect(v.kind).toBe("splash");
    if (v.kind !== "splash") throw new Error("unreachable");
    expect(v.splash.secs).toBe(2);
    engine.splashDone();
    v = engine.getView();
    expect(v.kind).toBe("scene");
    if (v.kind !== "scene") throw new Error("unreachable");
    expect(v.scene.texts.map((t) => t.text)).toEqual(["Premiere."]);
    expect(engine.state.moves).toBe(1); // compte une seule fois

    engine.choose(0); // -> b (pas de splash)
    engine.choose(0); // -> a : c'est la derniere section dont on a montre le splash
    expect(engine.getView().kind).toBe("scene");
  });

  it("`always` se rejoue a chaque arrivee, et jamais a la reprise d'une sauvegarde", async () => {
    const engine = await splashEngine();
    engine.startNewGame(1);
    engine.splashDone(); // a
    engine.choose(0); // b
    engine.choose(1); // c : splash always
    expect(engine.getView().kind).toBe("splash");
    engine.splashDone();
    engine.choose(0); // c -> c
    expect(engine.getView().kind).toBe("splash"); // rejoue
    engine.splashDone();
    engine.save();
    engine.returnToMenu();
    engine.continueGame();
    expect(engine.getView().kind).toBe("scene"); // reprise : pas de splash
  });
});

// --- conditions composees (OU de ET, not, else) ----------------------------

const COND_ADV = `
@title Conditions
@start a
@flag x
@flag y
@item i "Objet"
@stat N 5 0 9
:: a
~ set x
* [go] -> b
:: b
{(flag x or flag y) and has i} A
{else and not flag y} B
{else} C
{not (flag x and flag y)} D
{not stat N >= 6} E
* {(flag x and not has i) or flag y} [1] -> c
* {else} [2] -> c
* {not flag x or flag y} [3] -> c
:: c
~ give i
* [suite] -> d
:: d
{(flag x or flag y) and has i} A
{else and not flag y} B
{else} C
* {(flag x and not has i) or flag y} [1] -> d
* {else} [2] -> d
`;

async function condEngine() {
  const dir = mkdtempSync(join(tmpdir(), "cond-"));
  const adv = join(dir, "c.adv");
  writeFileSync(adv, COND_ADV);
  const out = join(dir, "story.json");
  execFileSync("python3", ["-m", "a2c.webjson", adv, "-o", out], {
    cwd: COMPILER_DIR,
  });
  const gameData = await loadStory(() => Promise.resolve(readFileSync(out, "utf-8")));
  return new Engine(gameData, new MemorySaveStore(), () => {});
}

describe("conditions composees", () => {
  it("evalue OU de ET, not et else comme le player natif", async () => {
    const engine = await condEngine();
    engine.startNewGame(1);
    engine.choose(0); // -> b : x vrai, y faux, objet absent
    let v = engine.getView();
    if (v.kind !== "scene") throw new Error("scene attendue");
    expect(v.scene.texts.map((t) => t.text)).toEqual(["B", "D", "E"]);
    expect(v.scene.choices.map((c) => c.label)).toEqual(["1"]);
    engine.choose(0); // -> c
    engine.choose(0); // -> d : l'objet est possede
    v = engine.getView();
    if (v.kind !== "scene") throw new Error("scene attendue");
    expect(v.scene.texts.map((t) => t.text)).toEqual(["A"]);
    expect(v.scene.choices.map((c) => c.label)).toEqual(["2"]);
  });
});
