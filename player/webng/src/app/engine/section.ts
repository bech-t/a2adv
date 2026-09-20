// section.ts -- forme en memoire du corps d'une section, miroir de scene.c
// (scene_read_header/scene_render_texts) + de la partie "corps de section"
// de main.c:play_section.
//
// Difference deliberee avec le player natif : la ou story.c/scene.c
// streament le corps octet par octet avec un curseur mutable (b_seek/b_u8),
// une section arrive ici deja entierement decodee -- `engine/story.ts`
// (loadStory) construit un SectionBody directement depuis le JSON produit
// par compiler/a2c/webjson.py, champ a champ (memes noms), sans curseur ni
// format binaire a lire. evalCond/applyEffects (state.ts) restent des
// fonctions PURES sur ces structures, jouees au moment ou l'UI en a besoin
// (visibilite d'un paragraphe/choix a chaque rendu, effets au moment d'une
// action joueur) -- meme sequencement que le player natif, juste sans
// curseur partage.

export interface CondAtom {
  op: number;
  a0: number;
  a1: number;
  a2: number;
}

/** OU de ET (cf. a2c/cond.py, state.c:state_eval_cond) : vrai si UNE clause a
 * tous ses atomes vrais. clauses=[] : pas de condition, toujours vrai. */
export interface CondBlock {
  clauses: CondAtom[][];
}

export interface Effect {
  guard: CondBlock;
  op: number;
  a0: number;
  a1: number;
  a2: number;
}
export type EffectList = Effect[];

export interface TextParagraph {
  cond: CondBlock;
  style: number;
  text: string; // Latin-1 deja decode ; peut contenir des \x01 (TXT_INV_TOGGLE)
}

export interface Choice {
  cond: CondBlock;
  effects: EffectList;
  target: number;
  label: string;
}

export interface CombatBlock {
  att: number;
  hp: number;
  dmg: number;
  armor: number;
  eimg: number;
  win: number;
  lose: number;
  flee: number;
  name: string;
  winFx: EffectList;
  loseFx: EffectList;
  fleeFx: EffectList;
  winMsg: string;
  loseMsg: string;
  fleeMsg: string;
}

export interface InputBlock {
  prompt: string;
  maxlen: number;
  answers: string[]; // deja normalisees cote compilateur (cf. translit.py:to_match_key)
  correct: number;
  wrong: number;
  correctFx: EffectList;
  wrongFx: EffectList;
}

export interface SectionBody {
  mode: number;
  ending: number;
  image: number;
  /** @splash : image plein ecran avant le texte, facultative (null : aucune). */
  splash: { asset: number; secs: number; always: boolean } | null;
  combat: CombatBlock | null;
  input: InputBlock | null;
  onEnter: EffectList;
  onExit: EffectList;
  texts: TextParagraph[];
  choices: Choice[]; // vide pour une section combat/saisie
}

/** Longueur visible d'un texte (hors octets TXT_INV_TOGGLE), utile pour un
 * rendu qui voudrait mesurer avant de decouper en <span> -- cf. ui.c:ui_wrap. */
export function visibleLength(text: string): number {
  let n = 0;
  for (let i = 0; i < text.length; ++i) if (text.charCodeAt(i) !== 1) ++n;
  return n;
}
