// combat.ts -- coeur portable du combat (RNG + resolution d'un round),
// miroir de combat.c. Meme regles v1 : chaque round, heros et ennemi
// lancent 2d6 + leur attaque ; le plus haut score touche et inflige
// max(1, degats - armure adverse).

import type { GameState } from "./state";
import type { StoryData } from "./story";
import { CB_CONTINUE, CB_FLEE, CB_LOSE, CB_WIN } from "./format";

/** xorshift 16 bits -- PRNG volontairement simple et rapide, pas
 * cryptographique (cf. combat.c pour le meme choix et la meme mise en garde :
 * predictible si on connait l'etat, mais suffisant pour des jets de d6). */
export class Rng {
  private state: number;
  constructor(seed = 0xace1) {
    this.state = seed || 0xace1;
  }
  seed(s: number): void {
    this.state = s || 0xace1;
  }
  private next(): number {
    let x = this.state & 0xffff;
    x ^= (x << 7) & 0xffff;
    x ^= x >> 9;
    x ^= (x << 8) & 0xffff;
    this.state = x & 0xffff;
    return this.state;
  }
  d6(): number {
    return (this.next() % 6) + 1;
  }
}

export type CombatOutcome =
  | typeof CB_WIN
  | typeof CB_LOSE
  | typeof CB_FLEE
  | typeof CB_CONTINUE;

export interface CombatRoundInfo {
  pscore: number;
  escore: number;
  lastDmg: number;
  lastTo: 0 | 1 | 2; // 0 = ennemi touche, 1 = heros touche, 2 = aucun (egalite)
}

function damage(base: number, armor: number): number {
  const d = base - armor;
  return d < 1 ? 1 : d;
}

export class Combat {
  enemyHp = 0;
  private eAtt = 0;
  private eDmg = 0;
  private eArmor = 0;
  last: CombatRoundInfo = { pscore: 0, escore: 0, lastDmg: 0, lastTo: 2 };

  private readonly rng: Rng;
  private readonly state: GameState;
  private readonly story: StoryData;

  constructor(rng: Rng, state: GameState, story: StoryData) {
    this.rng = rng;
    this.state = state;
    this.story = story;
  }

  begin(att: number, hp: number, dmg: number, armor: number): void {
    this.eAtt = att;
    this.enemyHp = hp;
    this.eDmg = dmg;
    this.eArmor = armor;
    this.last = { pscore: 0, escore: 0, lastDmg: 0, lastTo: 2 };
  }

  heroHp(): number {
    return this.story.combatHp === 0xff ? 0 : this.state.statVal[this.story.combatHp];
  }

  private heroTake(d: number): void {
    if (this.story.combatHp === 0xff) return;
    const h = this.state.statVal[this.story.combatHp];
    this.state.statVal[this.story.combatHp] = d >= h ? 0 : h - d;
  }

  attack(): CombatOutcome {
    const patt = Math.max(
      0,
      (this.story.combatAtt === 0xff ? 0 : this.state.statVal[this.story.combatAtt]) +
        this.state.gearAtk(),
    );
    const pdmg = this.story.combatBaseDmg + this.state.gearDmg();
    const parmor = this.state.gearArmor();

    const ps = this.rng.d6() + this.rng.d6() + patt;
    const es = this.rng.d6() + this.rng.d6() + this.eAtt;
    this.last.pscore = Math.min(ps, 255);
    this.last.escore = Math.min(es, 255);

    if (ps > es) {
      const d = damage(pdmg, this.eArmor);
      this.enemyHp = d >= this.enemyHp ? 0 : this.enemyHp - d;
      this.last.lastDmg = d;
      this.last.lastTo = 0;
    } else if (es > ps) {
      const d = damage(this.eDmg, parmor);
      this.heroTake(d);
      this.last.lastDmg = d;
      this.last.lastTo = 1;
    } else {
      this.last.lastDmg = 0;
      this.last.lastTo = 2;
    }

    if (this.enemyHp === 0) return CB_WIN;
    if (this.heroHp() === 0) return CB_LOSE;
    return CB_CONTINUE;
  }

  flee(): CombatOutcome {
    const d = damage(this.eDmg, this.state.gearArmor());
    this.heroTake(d);
    this.last.lastDmg = d;
    this.last.lastTo = 1;
    return this.heroHp() === 0 ? CB_LOSE : CB_FLEE;
  }
}
