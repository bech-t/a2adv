// state.ts -- etat runtime du joueur + evaluation conditions/effets, miroir
// de state.c. Fonctionne sur les structures decodees par section.ts
// (CondBlock/EffectList) plutot que sur un curseur d'octets -- cf. l'entete
// de section.ts pour pourquoi.

import type { CondBlock, EffectList } from "./section";
import type { StoryData } from "./story";
import { flagDefaultBit, itemDefaultBit } from "./story";
import {
  CMP_EQ,
  CMP_GE,
  CMP_GT,
  CMP_LE,
  CMP_LT,
  CMP_NE,
  NO_GOTO,
  OP_CLR_FLAG,
  OP_FLAG_CLR,
  OP_FLAG_SET,
  OP_GIVE_ITEM,
  OP_GOTO,
  OP_HAS_ITEM,
  OP_NO_ITEM,
  OP_SCORE_ADD,
  OP_SET_FLAG,
  OP_SOUND,
  OP_STAT_ADD,
  OP_STAT_CMP,
  OP_STAT_MAX,
  OP_STAT_SET,
  OP_STAT_SETMAX,
  OP_STAT_SUB,
  OP_TAKE_ITEM,
  OP_TOG_FLAG,
} from "./format";

export class GameState {
  statVal: number[];
  statMax: number[]; // max COURANT (mutable via ~ setmax, cf. OP_STAT_SETMAX)
  itemBits: Uint8Array;
  flagBits: Uint8Array;
  score = 0;
  moves = 0;
  private readonly story: StoryData;

  constructor(story: StoryData) {
    this.story = story;
    this.statVal = story.statInit.slice();
    this.statMax = story.statMaxDef.slice();
    this.itemBits = new Uint8Array(Math.ceil(story.nItems / 8));
    this.flagBits = new Uint8Array(Math.ceil(story.nFlags / 8));
    for (let i = 0; i < story.nItems; ++i) this.setItemBit(i, itemDefaultBit(story, i));
    for (let i = 0; i < story.nFlags; ++i) this.setFlagBit(i, flagDefaultBit(story, i));
  }

  itemGet(i: number): boolean {
    return ((this.itemBits[i >> 3] >> (i & 7)) & 1) !== 0;
  }
  flagGet(i: number): boolean {
    return ((this.flagBits[i >> 3] >> (i & 7)) & 1) !== 0;
  }
  private setItemBit(i: number, v: number): void {
    const mask = 1 << (i & 7);
    if (v) this.itemBits[i >> 3] |= mask;
    else this.itemBits[i >> 3] &= ~mask & 0xff;
  }
  private setFlagBit(i: number, v: number): void {
    const mask = 1 << (i & 7);
    if (v) this.flagBits[i >> 3] |= mask;
    else this.flagBits[i >> 3] &= ~mask & 0xff;
  }

  /** Remet a 0 les flags LOCAUX ([localBase, nFlags)) -- cf.
   * state_clear_locals(), appele a chaque changement de fichier/chapitre. */
  clearLocals(): void {
    for (let i = this.story.localBase; i < this.story.nFlags; ++i) this.setFlagBit(i, 0);
  }

  /** Sommes des modificateurs de combat de tous les objets PORTES. */
  gearAtk(): number {
    return this.sumGear(this.story.itemAtk);
  }
  gearDmg(): number {
    return this.sumGear(this.story.itemDmg);
  }
  gearArmor(): number {
    return this.sumGear(this.story.itemArmor);
  }
  private sumGear(mods: number[]): number {
    let s = 0;
    for (let i = 0; i < this.story.nItems; ++i) if (this.itemGet(i)) s += mods[i];
    return s;
  }

  private evalAtom(op: number, a0: number, a1: number, a2: number): boolean {
    switch (op) {
      case OP_FLAG_SET:
        return this.flagGet(a0);
      case OP_FLAG_CLR:
        return !this.flagGet(a0);
      case OP_HAS_ITEM:
        return this.itemGet(a0);
      case OP_NO_ITEM:
        return !this.itemGet(a0);
      case OP_STAT_CMP: {
        const v = this.statVal[a0];
        switch (a1) {
          case CMP_EQ:
            return v === a2;
          case CMP_NE:
            return v !== a2;
          case CMP_LT:
            return v < a2;
          case CMP_LE:
            return v <= a2;
          case CMP_GT:
            return v > a2;
          case CMP_GE:
            return v >= a2;
        }
        return false;
      }
    }
    return false;
  }

  evalCond(cond: CondBlock): boolean {
    if (cond.clauses.length === 0) return true;
    return cond.clauses.some((clause) =>
      clause.every((a) => this.evalAtom(a.op, a.a0, a.a1, a.a2)),
    );
  }

  private clampSet(idx: number, nv: number): void {
    if (nv < this.story.statMin[idx]) nv = this.story.statMin[idx];
    if (nv > this.statMax[idx]) nv = this.statMax[idx];
    this.statVal[idx] = nv;
  }

  /** Applique un effect_list ; renvoie l'index de section d'un GOTO
   * rencontre (short-circuit, comme state_apply_effects), ou NO_GOTO.
   * `playSound` est optionnel : NO-OP si non fourni (ex. lors d'une reprise
   * de sauvegarde ou d'un test hors navigateur). */
  applyEffects(list: EffectList, playSound?: (id: number) => void): number {
    let target = NO_GOTO;
    for (const eff of list) {
      if (!this.evalCond(eff.guard)) continue;
      switch (eff.op) {
        case OP_SET_FLAG:
          this.setFlagBit(eff.a0, 1);
          break;
        case OP_CLR_FLAG:
          this.setFlagBit(eff.a0, 0);
          break;
        case OP_TOG_FLAG:
          this.setFlagBit(eff.a0, this.flagGet(eff.a0) ? 0 : 1);
          break;
        case OP_GIVE_ITEM:
          this.setItemBit(eff.a0, 1);
          break;
        case OP_TAKE_ITEM:
          this.setItemBit(eff.a0, 0);
          break;
        case OP_STAT_ADD:
          this.clampSet(eff.a0, this.statVal[eff.a0] + eff.a1);
          break;
        case OP_STAT_SUB:
          this.clampSet(eff.a0, this.statVal[eff.a0] - eff.a1);
          break;
        case OP_STAT_SET:
          this.clampSet(eff.a0, eff.a1);
          break;
        case OP_SOUND:
          playSound?.(eff.a0);
          break;
        case OP_SCORE_ADD:
          if (this.story.scoreOn) this.score += eff.a0;
          break;
        case OP_STAT_MAX: // ~ restore STAT : au max courant
          this.statVal[eff.a0] = this.statMax[eff.a0];
          break;
        case OP_STAT_SETMAX: // ~ setmax STAT N : fixe le max
          this.statMax[eff.a0] = eff.a1;
          if (this.statVal[eff.a0] > eff.a1) this.statVal[eff.a0] = eff.a1;
          break;
        case OP_GOTO:
          return eff.a0 | (eff.a1 << 8); // saut immediat, effets suivants ignores
      }
    }
    return target;
  }
}
