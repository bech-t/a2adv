// engine.ts -- orchestration haut niveau, miroir de main.c (play_section,
// run_intro) + scombat.c/sinput.c (les DEUX ecrans interactifs qu'un
// simple retour de fonction ne peut pas representer : un combat/une saisie
// s'etalent sur plusieurs actions joueur). Plutot qu'une boucle bloquante
// qui lit le clavier (scr_getkey), ce fichier expose une machine a etats :
// `view` decrit ce qu'il y a a l'ecran, les methodes publiques sont les
// actions que l'UI peut declencher. Pas d'import Angular ici, ni ailleurs
// dans engine/ : testable en Node seul (cf. engine/hostplay.spec.ts,
// l'equivalent `make hosttest`) ; l'UI consomme cette API sans jamais la
// modifier (cf. ui/game.ts).
//
// Simplifications deliberees par rapport a main.c (aucune ne change les
// regles du jeu) :
//  - "sauter un effect_list sans l'appliquer" (state_skip_effects) n'a pas
//    d'equivalent ici : section.ts decode TOUT le corps a l'avance, donc
//    "sauter" un effet, c'est simplement ne jamais appeler applyEffects
//    dessus.
//  - La sauvegarde/le chargement et le retour au menu ne passent plus par
//    un "code touche special" (KEY_SAVE/LOAD/QUIT) glisse parmi les choix
//    de la scene : ce sont des methodes directes (save/continueGame/
//    returnToMenu), plus naturel cote UI a boutons qu'une invite clavier.
//    L'inventaire (KEY_INVENTORY) n'a meme plus besoin de passer par le
//    moteur : `state`/`story` sont deja exposes en lecture, un composant UI
//    local peut afficher l'inventaire sans aucune action moteur dediee.
//  - Mode mixte (image+texte simultanes) vs image plein ecran puis texte :
//    la distinction existait pour menager un switch de resolution materiel
//    (cf. player/atarist/src/scr.c). Le DOM n'a pas cette contrainte :
//    l'image (si presente) s'affiche au-dessus du texte dans les deux cas.

import { Combat, Rng, type CombatOutcome, type CombatRoundInfo } from "./combat";
import {
  CB_FLEE,
  CB_LOSE,
  CB_WIN,
  END_LOSE,
  END_NONE,
  END_WIN,
  GAME_OVER,
  NO_GOTO,
  Snd,
  TXT_STAT_REF,
  Ui,
} from "./format";
import type { EffectList, SectionBody, TextParagraph } from "./section";
import { GameState } from "./state";
import type { GameData, StoryData } from "./story";

export interface SaveData {
  section: number;
  statVal: number[];
  statMax: number[];
  itemBits: number[];
  flagBits: number[];
  score: number;
  moves: number;
}

/** Port d'E/S injecte par l'appelant (assets/loader.ts pour le vrai
 * localStorage, un simple objet en memoire pour les tests). Meme esprit que
 * diskio.h : le moteur ne sait pas OU ca vit. */
export interface SaveStore {
  read(): SaveData | null;
  write(data: SaveData): void;
}

// number, pas Snd : l'operande OP_SOUND est un octet brut lu dans
// STORY.DAT (state.ts:applyEffects), pas une valeur connue a la
// compilation -- Snd sert a nommer les constantes cote appelant
// (playSound(Snd.WIN)), pas a contraindre ce que le binaire peut contenir.
export type PlaySound = (id: number) => void;

export interface StatusInfo {
  stats: { name: string; value: number; hidden: boolean }[];
  score: number | null;
  moves: number | null;
}

export interface SceneView {
  status: StatusInfo;
  image: number | null; // asset numerique (NO_IMAGE -> null), cf. img_name()
  texts: TextParagraph[];
  choices: { index: number; label: string }[]; // deja filtres par visibilite
}

export interface CombatView {
  status: StatusInfo; // ui_status() complet, cf. scombat.c:run_combat
  enemyName: string;
  enemyImage: number | null;
  enemyHp: number;
  heroHp: number;
  canFlee: boolean;
  intro: TextParagraph[] | null; // texte d'accroche, seulement au 1er rendu
  last: CombatRoundInfo | null; // round precedent (null avant le 1er coup)
  outcome: CombatOutcome | null; // non-null : combat termine, en attente de continue()
  outcomeMessage: string | null;
}

export interface InputView {
  status: StatusInfo;
  texts: TextParagraph[];
  prompt: string;
  maxlen: number;
}

/** @splash : image plein ecran avant le texte de la section `idx`.
 * secs = 0 : attend un clic/une touche ; sinon ~secs secondes (clic pour passer).
 * Une image introuvable se saute d'elle-meme (cf. ui/splash.ts). */
export interface SplashView {
  image: number;
  secs: number;
  idx: number;
}

export interface IntroView {
  index: number;
  count: number;
  image: number | null;
  texts: TextParagraph[];
}

export interface EndingView {
  // Valeur brute lue dans l'en-tete de section (END_WIN/END_LOSE/END_NONE) :
  // simple number, pas une union de litteraux -- ce n'est pas une constante
  // connue a la compilation, mais un octet decode dans un STORY.DAT.
  ending: number;
  deadEnd: boolean;
}

export type View =
  | { kind: "menu"; title: string; version: string; canContinue: boolean }
  | { kind: "intro"; intro: IntroView }
  | { kind: "splash"; splash: SplashView }
  | { kind: "scene"; scene: SceneView }
  | { kind: "combat"; combat: CombatView }
  | { kind: "input"; input: InputView }
  | { kind: "ending"; ending: EndingView }
  | { kind: "error"; message: string };

type Listener = () => void;

export class Engine {
  readonly story: StoryData;
  private readonly sections: SectionBody[];
  private readonly sectionChapter: number[];
  private readonly uiStr: string[];
  private readonly rng = new Rng();
  private readonly saveStore: SaveStore;
  private readonly playSound: PlaySound;

  state: GameState;
  private currentChapter = 0;
  private view: View;
  private listeners = new Set<Listener>();

  // Derniere section dont le @splash a ete montre (-1 : aucune) : pas rejoue en
  // revenant dans cette section (carrefour), cf. simage.c:show_splash.
  private splashLast = -1;

  // Contexte du corps de section couramment affiche (normal ou saisie).
  private curIdx = 0;
  private curBody: SectionBody | null = null;

  // Contexte combat en cours.
  private combat: Combat | null = null;
  private combatBody: SectionBody | null = null;

  constructor(data: GameData, saveStore: SaveStore, playSound: PlaySound) {
    this.story = data.story;
    this.sections = data.sections;
    this.sectionChapter = data.sectionChapter;
    this.saveStore = saveStore;
    this.playSound = playSound;
    this.uiStr = data.uiStrings;

    this.state = new GameState(this.story);
    this.view = this.menuView();
  }

  ui(key: Ui): string {
    return this.uiStr[key] ?? "";
  }

  getView(): View {
    return this.view;
  }
  subscribe(fn: Listener): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }
  private notify(): void {
    for (const fn of this.listeners) fn();
  }
  private setView(v: View): void {
    this.view = v;
    this.notify();
  }

  private menuView(): View {
    return {
      kind: "menu",
      title: this.story.title,
      version: this.story.adventureVersion,
      canContinue: this.saveStore.read() !== null,
    };
  }

  hasSave(): boolean {
    return this.saveStore.read() !== null;
  }

  // --- Demarrage --------------------------------------------------------

  startNewGame(seed: number): void {
    this.state = new GameState(this.story);
    this.currentChapter = 0;
    this.splashLast = -1;
    this.rng.seed(seed | 1);
    if (this.story.nIntro > 0) {
      this.setView({ kind: "intro", intro: this.buildIntro(0) });
    } else {
      this.goto(this.story.start, false);
    }
  }

  continueGame(): boolean {
    const save = this.saveStore.read();
    if (!save || !this.isCompatible(save)) return false;
    this.state = new GameState(this.story);
    this.state.statVal = save.statVal.slice();
    this.state.statMax = save.statMax.slice();
    this.state.itemBits = Uint8Array.from(save.itemBits);
    this.state.flagBits = Uint8Array.from(save.flagBits);
    this.state.score = save.score;
    this.state.moves = save.moves;
    this.currentChapter = this.sectionChapter[save.section] ?? 0;
    this.goto(save.section, true);
    return true;
  }

  /** Une sauvegarde d'une autre version de l'aventure (autre nombre de
   * sections, de caracteristiques, d'objets ou de drapeaux) ne se charge pas. */
  private isCompatible(save: SaveData): boolean {
    const s = this.story;
    return (
      Number.isInteger(save.section) &&
      save.section >= 0 &&
      save.section < this.sections.length &&
      save.statVal.length === s.nStats &&
      save.statMax.length === s.nStats &&
      save.itemBits.length === Math.ceil(s.nItems / 8) &&
      save.flagBits.length === Math.ceil(s.nFlags / 8)
    );
  }

  // --- Intro --------------------------------------------------------------

  private buildIntro(index: number): IntroView {
    const idx = this.story.introIdx[index];
    const body = this.sections[idx];
    // render_scene() ignore ENTIEREMENT on_enter/on_exit pour une scene
    // d'intro (state_skip_effects x2, jamais applique, cf. main.c) : rien a
    // appeler ici, juste lire l'en-tete + les textes.
    return {
      index,
      count: this.story.nIntro,
      // une scene d'intro n'a pas d'ecran de splash : son @splash devient son image
      image: body.image !== 0xffff ? body.image : (body.splash?.asset ?? null),
      texts: this.substituteTexts(body.texts.filter((t) => this.state.evalCond(t.cond))),
    };
  }

  introNext(): void {
    if (this.view.kind !== "intro") return;
    const next = this.view.intro.index + 1;
    if (next >= this.story.nIntro) {
      this.goto(this.story.start, false);
    } else {
      this.setView({ kind: "intro", intro: this.buildIntro(next) });
    }
  }

  /** Fin d'un @splash (touche, delai ou image introuvable) : le texte suit. */
  splashDone(): void {
    const v = this.view;
    if (v.kind !== "splash") return;
    this.goto(v.splash.idx, true);
  }

  introSkip(): void {
    this.goto(this.story.start, false);
  }

  // --- Boucle principale ---------------------------------------------------

  /** Charge et joue la section `idx`, jusqu'a produire une vue interactive
   * (scene/combat/saisie/fin) ou revenir au menu. `resume` : reprise de
   * sauvegarde, n'applique pas on_enter (deja fait avant la sauvegarde). */
  private goto(idx: number, resume: boolean): void {
    for (;;) {
      if (idx === GAME_OVER) {
        this.setView(this.menuView());
        return;
      }
      if (idx >= this.sections.length) {
        this.setView({ kind: "error", message: this.ui(Ui.SECTION_ERR) });
        return;
      }

      const chapter = this.sectionChapter[idx];
      if (chapter !== this.currentChapter) {
        this.currentChapter = chapter;
        this.state.clearLocals(); // cf. story.c:story_load_section (1 fichier = 1 chapitre, natif)
      }

      const body = this.sections[idx];
      const resumed = resume;

      if (resume) {
        resume = false;
      } else {
        const g = this.state.applyEffects(body.onEnter, this.playSound);
        if (g !== NO_GOTO) {
          idx = g;
          continue; // redirection silencieuse, pas de rendu, pas de mouvement compte
        }
        if (this.story.movesOn) ++this.state.moves;
      }

      this.curIdx = idx;
      this.curBody = body;

      // @splash : jamais a la reprise ; splashDone() rentre a nouveau ici avec
      // resume=true (on_enter et mouvement deja comptes), donc sans rejouer l'image.
      if (!resumed && body.splash && (body.splash.always || idx !== this.splashLast)) {
        this.splashLast = idx;
        this.setView({
          kind: "splash",
          splash: { image: body.splash.asset, secs: body.splash.secs, idx },
        });
        return;
      }

      if (body.combat) {
        this.enterCombat(body);
        return;
      }
      if (body.input) {
        this.setView({
          kind: "input",
          input: {
            status: this.status(),
            texts: this.substituteTexts(body.texts.filter((t) => this.state.evalCond(t.cond))),
            prompt: this.substituteStatRefs(body.input.prompt),
            maxlen: body.input.maxlen,
          },
        });
        return;
      }

      const visible = body.choices.filter((c) => this.state.evalCond(c.cond));
      const isEnding = body.ending !== END_NONE || body.choices.length === 0;

      if (isEnding) {
        if (body.ending === END_WIN) this.playSound(Snd.WIN);
        else if (body.ending === END_LOSE) this.playSound(Snd.LOSE);
        this.setView({ kind: "ending", ending: { ending: body.ending, deadEnd: false } });
        return;
      }
      if (visible.length === 0) {
        this.setView({ kind: "ending", ending: { ending: END_NONE, deadEnd: true } });
        return;
      }

      this.setView({
        kind: "scene",
        scene: {
          status: this.status(),
          image: body.image === 0xffff ? null : body.image,
          texts: this.substituteTexts(body.texts.filter((t) => this.state.evalCond(t.cond))),
          choices: visible.map((c, i) => ({ index: i, label: this.substituteStatRefs(c.label) })),
        },
      });
      return;
    }
  }

  private status(): StatusInfo {
    const stats = this.story.statName.map((name, i) => ({
      name,
      value: this.state.statVal[i],
      hidden: ((this.story.statHidden >> i) & 1) !== 0,
    }));
    return {
      stats,
      score: this.story.scoreOn ? this.state.score : null,
      moves: this.story.movesOn ? this.state.moves : null,
    };
  }

  // --- Reference de stat inline (%NOM% du .adv) ---------------------------
  // Le compilateur encode chaque %NOM% en TXT_STAT_REF suivi de l'index de
  // la stat (cf. compiler/a2c/symbols.py:substitute_stat_refs) ; ici, on
  // l'expand en la valeur COURANTE, une seule fois au moment ou la vue est
  // construite -- rich-text.ts ne voit donc jamais ce marqueur (contrairement
  // a TXT_INV_TOGGLE, qu'il interprete lui-meme, cf. son entete).
  private substituteStatRefs(text: string): string {
    let out = "";
    for (let i = 0; i < text.length; ) {
      if (text.charCodeAt(i) === TXT_STAT_REF) {
        out += String(this.state.statVal[text.charCodeAt(i + 1)]);
        i += 2;
      } else {
        out += text[i];
        ++i;
      }
    }
    return out;
  }

  private substituteTexts(texts: TextParagraph[]): TextParagraph[] {
    return texts.map((t) => ({ ...t, text: this.substituteStatRefs(t.text) }));
  }

  /** Fin d'ecran (banniere de fin / impasse) : retour au menu. */
  acknowledgeEnding(): void {
    this.setView(this.menuView());
  }

  // --- Scene normale : choix ----------------------------------------------

  choose(visibleIndex: number): void {
    if (this.view.kind !== "scene" || !this.curBody) return;
    const visible = this.curBody.choices.filter((c) => this.state.evalCond(c.cond));
    const choice = visible[visibleIndex];
    if (!choice) return;

    const g1 = this.state.applyEffects(choice.effects, this.playSound);
    const g2 = this.state.applyEffects(this.curBody.onExit, this.playSound);
    const target = g2 !== NO_GOTO ? g2 : g1 !== NO_GOTO ? g1 : choice.target;
    this.goto(target, false);
  }

  // --- Combat ---------------------------------------------------------------

  private enterCombat(body: SectionBody): void {
    if (!body.combat) return;
    this.combatBody = body;
    this.combat = new Combat(this.rng, this.state, this.story);
    this.combat.begin(body.combat.att, body.combat.hp, body.combat.dmg, body.combat.armor);
    this.setView({
      kind: "combat",
      combat: {
        status: this.status(),
        enemyName: body.combat.name,
        enemyImage:
          body.combat.eimg !== 0xffff ? body.combat.eimg : body.image !== 0xffff ? body.image : null,
        enemyHp: this.combat.enemyHp,
        heroHp: this.combat.heroHp(),
        canFlee: body.combat.flee !== NO_GOTO,
        intro: this.substituteTexts(body.texts.filter((t) => this.state.evalCond(t.cond))),
        last: null,
        outcome: null,
        outcomeMessage: null,
      },
    });
  }

  private resolveCombat(outcome: CombatOutcome): void {
    if (!this.combat || !this.combatBody?.combat) return;
    const cb = this.combatBody.combat;
    let target: number;
    let fx: EffectList;
    let msg: string;
    if (outcome === CB_WIN) {
      target = cb.win;
      fx = cb.winFx;
      msg = cb.winMsg;
      this.playSound(Snd.WIN);
    } else if (outcome === CB_FLEE) {
      target = cb.flee;
      fx = cb.fleeFx;
      msg = cb.fleeMsg;
    } else {
      target = cb.lose;
      fx = cb.loseFx;
      msg = cb.loseMsg;
      this.playSound(Snd.LOSE);
    }
    // Le GOTO eventuel de ces deux listes est ignore (comme main.c:
    // play_section jette le retour de ces deux state_apply_effects) : seuls
    // les effets de bord (son, flags, stats...) comptent ici.
    this.state.applyEffects(fx, this.playSound);
    this.state.applyEffects(this.combatBody.onExit, this.playSound);

    if (msg) {
      this.setView({
        kind: "combat",
        combat: {
          status: this.status(),
          enemyName: cb.name,
          enemyImage: null,
          enemyHp: this.combat.enemyHp,
          heroHp: this.combat.heroHp(),
          canFlee: false,
          intro: null,
          last: this.combat.last,
          outcome,
          // substitue APRES applyEffects(fx, ...) ci-dessus : un %NOM%
          // affiche la valeur POST-issue (ex. winFx qui ajuste la stat
          // affichee dans winMsg), pas celle d'avant le gain/la perte.
          outcomeMessage: this.substituteStatRefs(msg),
        },
      });
      this.pendingCombatTarget = target;
      return;
    }
    this.combat = null;
    this.combatBody = null;
    this.goto(target, false);
  }

  private pendingCombatTarget: number | null = null;

  /** Ferme l'ecran de message d'issue de combat (cf. ui_wait_key dans
   * main.c) et avance vers la section suivante. */
  continueAfterCombat(): void {
    if (this.pendingCombatTarget === null) return;
    const target = this.pendingCombatTarget;
    this.pendingCombatTarget = null;
    this.combat = null;
    this.combatBody = null;
    this.goto(target, false);
  }

  combatAttack(): void {
    if (this.view.kind !== "combat" || !this.combat || !this.combatBody?.combat) return;
    const outcome = this.combat.attack();
    if (outcome === CB_WIN || outcome === CB_LOSE) {
      this.resolveCombat(outcome);
      return;
    }
    this.setView({
      kind: "combat",
      combat: {
        status: this.status(),
        enemyName: this.combatBody.combat.name,
        enemyImage: null,
        enemyHp: this.combat.enemyHp,
        heroHp: this.combat.heroHp(),
        canFlee: this.combatBody.combat.flee !== NO_GOTO,
        intro: null,
        last: this.combat.last,
        outcome: null,
        outcomeMessage: null,
      },
    });
  }

  combatFlee(): void {
    if (this.view.kind !== "combat" || !this.combat) return;
    const outcome = this.combat.flee();
    this.resolveCombat(outcome);
  }

  // --- Saisie -----------------------------------------------------------

  submitAnswer(raw: string): void {
    if (this.view.kind !== "input" || !this.curBody?.input) return;
    const norm = normalizeInput(raw);
    const inp = this.curBody.input;
    const ok = inp.answers.some((a) => normalizeInput(a) === norm);

    const target = ok ? inp.correct : inp.wrong;
    const fx = ok ? inp.correctFx : inp.wrongFx;
    // Meme regle que le combat : le GOTO eventuel de ces listes est ignore.
    this.state.applyEffects(fx, this.playSound);
    this.state.applyEffects(this.curBody.onExit, this.playSound);
    this.goto(target, false);
  }

  // --- Sauvegarde ---------------------------------------------------------

  save(): void {
    this.saveStore.write({
      section: this.curIdx,
      statVal: this.state.statVal.slice(),
      statMax: this.state.statMax.slice(),
      itemBits: Array.from(this.state.itemBits),
      flagBits: Array.from(this.state.flagBits),
      score: this.state.score,
      moves: this.state.moves,
    });
  }

  returnToMenu(): void {
    this.setView(this.menuView());
  }
}

/** Majuscules + espaces de bord supprimes -- cf. sinput.c:norm_input. Les
 * reponses stockees dans STORY.DAT sont deja normalisees cote compilateur
 * (translit.py:to_match_key) : seule la saisie joueur a besoin de l'etre ici. */
function normalizeInput(s: string): string {
  return s.toUpperCase().trim();
}
