"""Simulateur de parties : joue une aventure au hasard, des milliers de fois.

Complète `a2c.analyze` : l'analyse statique ignore les conditions, alors que
le simulateur les évalue pour de bon (drapeaux, objets, caractéristiques
bornées, drapeaux locaux remis à zéro à chaque chapitre). Il repère ce que
l'analyse ne voit pas :

  - impasses : une section avec des choix, mais aucun dont la condition soit
    vraie pour l'état courant du joueur (le player affiche alors
    `(AUCUNE ISSUE POSSIBLE)`) ;
  - boucles : une partie qui ne se termine pas ;
  - équilibrage : répartition des fins, et distribution finale d'une
    caractéristique au choix (`--stat`), typiquement un compteur de temps.

Le simulateur ne remplace pas de jouer : il ne lit pas le texte, ne fuit
jamais un combat, et tire les réponses de `@ask` au hasard (`--p-correct`).

Usage : python3 -m a2c.simulate <aventure.adv> [options]
        (voir `--help`)
"""

from __future__ import annotations

import argparse
import random
import sys
from collections import Counter
from pathlib import Path

from .model import Cmp, Ending, Story
from .parser import parse
from .symbols import resolve

_CMP = {
    Cmp.EQ: lambda a, b: a == b, Cmp.NE: lambda a, b: a != b,
    Cmp.LT: lambda a, b: a < b, Cmp.LE: lambda a, b: a <= b,
    Cmp.GT: lambda a, b: a > b, Cmp.GE: lambda a, b: a >= b,
}

END_WIN, END_LOSE, END_PLAIN = "victoire", "défaite", "fin"
DEAD_END, LOOP = "impasse", "boucle"
_ENDINGS = {Ending.WIN: END_WIN, Ending.LOSE: END_LOSE, Ending.NONE: END_PLAIN}


class _State:
    """État d'un joueur : caractéristiques, plafonds, drapeaux, objets."""

    def __init__(self, story: Story):
        self.value = {d.name: d.init for d in story.stats}
        self.cap = {d.name: d.hi for d in story.stats}
        self.flags = {f.name for f in story.flags if f.default_on}
        self.items = {i.name for i in story.items if i.default_on}
        self.chapter = 0


class Simulator:
    def __init__(self, story: Story, p_correct: float = 0.5):
        self.story = story
        self.p_correct = p_correct
        self.sections = {s.name: s for s in story.sections}
        self.floor = {d.name: d.lo for d in story.stats}
        self.local_flags = {f.name for f in story.flags if f.is_local}
        self.item_mods = {i.name: (i.atk, i.dmg, i.armor) for i in story.items}

    # --- évaluation des conditions et effets ---------------------------------

    def _atom(self, st: _State, a) -> bool:
        if a.op == "flag":
            return a.name in st.flags
        if a.op == "not_flag":
            return a.name not in st.flags
        if a.op == "has":
            return a.name in st.items
        if a.op == "not_has":
            return a.name not in st.items
        return _CMP[a.cmp](st.value[a.name], a.value)

    def _cond(self, st: _State, cond) -> bool:
        if not cond.atoms:
            return True
        results = [self._atom(st, a) for a in cond.atoms]
        return any(results) if cond.connective == 1 else all(results)

    def _clamp(self, st: _State, name: str) -> None:
        st.value[name] = max(self.floor[name], min(st.cap[name], st.value[name]))

    def _effect(self, st: _State, e) -> None:
        if not self._cond(st, e.cond):
            return
        op = e.op
        if op == "set":
            st.flags.add(e.name)
        elif op == "clear":
            st.flags.discard(e.name)
        elif op == "toggle":
            st.flags ^= {e.name}
        elif op == "give":
            st.items.add(e.name)
        elif op == "take":
            st.items.discard(e.name)
        elif op in ("add", "sub", "setstat"):
            if op == "add":
                st.value[e.name] += e.value
            elif op == "sub":
                st.value[e.name] -= e.value
            else:
                st.value[e.name] = e.value
            self._clamp(st, e.name)
        elif op == "setmax":
            st.cap[e.name] = e.value
            self._clamp(st, e.name)
        elif op == "restore":
            st.value[e.name] = st.cap[e.name]

    def _apply(self, st: _State, effects) -> str | None:
        """Applique les effets ; renvoie la cible du dernier `goto` actif."""
        target = None
        for e in effects:
            self._effect(st, e)
            if e.op == "goto" and self._cond(st, e.cond):
                target = e.name
        return target

    # --- combat ---------------------------------------------------------------

    def _fight(self, st: _State, c, rng: random.Random) -> bool:
        """Résout un combat jusqu'au bout (jamais de fuite). True = victoire."""
        story = self.story
        atk = st.value[story.combat_attack]
        dmg, armor = story.combat_base_dmg, 0
        for item in st.items:
            a, d, r = self.item_mods[item]
            atk, dmg, armor = atk + a, dmg + d, armor + r
        enemy_hp = c.hp
        while True:
            hero = rng.randint(1, 6) + rng.randint(1, 6) + atk
            foe = rng.randint(1, 6) + rng.randint(1, 6) + c.att
            if hero > foe:
                enemy_hp -= max(1, dmg - c.armor)
            elif foe > hero:
                st.value[story.combat_hp] -= max(1, c.dmg - armor)
                self._clamp(st, story.combat_hp)
            if enemy_hp <= 0:
                return True
            if st.value[story.combat_hp] <= 0:
                return False

    # --- une partie -----------------------------------------------------------

    def play(self, pick, rng: random.Random, max_steps: int = 3000):
        """Joue une partie. Renvoie (issue, section finale, état, parcours)."""
        st = _State(self.story)
        pending = list(self.story.intro) + [self.story.start]
        cur = pending.pop(0)
        trail: list[str] = []
        for _ in range(max_steps):
            sec = self.sections[cur]
            if sec.chapter != st.chapter:
                st.chapter = sec.chapter
                st.flags -= self.local_flags
            trail.append(cur)
            jump = self._apply(st, sec.on_enter)
            if jump:
                cur = jump
                continue
            if sec.combat is not None:
                c = sec.combat
                won = self._fight(st, c, rng)
                self._apply(st, c.win_effects if won else c.lose_effects)
                cur = c.win if won else c.lose
                continue
            if sec.input is not None:
                i = sec.input
                right = rng.random() < self.p_correct
                self._apply(st, i.correct_effects if right else i.wrong_effects)
                cur = i.correct if right else i.wrong
                continue
            options = [c for c in sec.choices if self._cond(st, c.cond)]
            if not options:
                if pending:
                    cur = pending.pop(0)
                    continue
                if not sec.choices:
                    return _ENDINGS[sec.ending], cur, st, trail
                return DEAD_END, cur, st, trail
            choice = pick(options, trail, rng)
            jump = self._apply(st, choice.effects)
            self._apply(st, sec.on_exit)
            cur = jump or choice.target
        return LOOP, cur, st, trail


# --- stratégies de choix ---------------------------------------------------

def make_picker(strategy: str, avoid: list[str]):
    """Fabrique la fonction qui choisit parmi les options valides.

    `random` : uniforme. `explorer` : préfère les sections les moins visitées
    (tirage uniforme 1 fois sur 6, pour ne pas rester bloqué sur un ordre
    fixe). `avoid` : sous-chaînes de noms de sections à éviter tant qu'une
    autre option existe (typiquement les hôtels, ou les mauvais choix connus).
    """
    def pick(options, trail, rng):
        pool = [c for c in options if not any(a in c.target for a in avoid)]
        pool = pool or options
        if strategy == "explorer" and rng.random() > 1 / 6:
            seen = Counter(trail)
            least = min(seen[c.target] for c in pool)
            pool = [c for c in pool if seen[c.target] == least]
        return rng.choice(pool)
    return pick


# --- ligne de commande -----------------------------------------------------

def _report(sim: Simulator, args) -> int:
    pick = make_picker(args.strategy, args.avoid)
    rng = random.Random(args.seed)
    outcomes: Counter = Counter()
    stat_at_end: dict[str, Counter] = {}
    stuck: dict[str, list[str]] = {}
    for _ in range(args.runs):
        outcome, cur, st, trail = sim.play(pick, rng, args.max_steps)
        outcomes[outcome] += 1
        if outcome in (DEAD_END, LOOP):
            stuck.setdefault(cur, trail[-args.trail:])
        elif args.stat:
            stat_at_end.setdefault(outcome, Counter())[st.value[args.stat]] += 1

    print(f"{args.runs} parties, stratégie {args.strategy}, "
          f"graine {args.seed}, réponses justes {args.p_correct:.0%}")
    for outcome, n in outcomes.most_common():
        print(f"  {outcome:<9} {n:>6}  ({n / args.runs:.1%})")
    if args.stat:
        for outcome, dist in sorted(stat_at_end.items()):
            row = ", ".join(f"{v}: {n}" for v, n in sorted(dist.items()))
            print(f"  {args.stat} en fin de partie ({outcome}) -> {row}")
    for name, trail in stuck.items():
        print(f"  BLOQUÉ en '{name}' après : {' > '.join(trail)}")
    return 1 if stuck else 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="python3 -m a2c.simulate",
        description="Joue une aventure .adv au hasard pour repérer les "
                    "impasses, les boucles et juger l'équilibrage.")
    ap.add_argument("adv", type=Path, help="aventure .adv")
    ap.add_argument("-n", "--runs", type=int, default=1000,
                    help="nombre de parties (défaut 1000)")
    ap.add_argument("--seed", type=int, default=1,
                    help="graine aléatoire : même graine, mêmes parties")
    ap.add_argument("--strategy", choices=("random", "explorer"),
                    default="explorer",
                    help="random : choix uniforme ; explorer : préfère les "
                         "sections peu visitées (défaut)")
    ap.add_argument("--p-correct", type=float, default=0.5, metavar="P",
                    help="probabilité de répondre juste à un @ask (0..1, "
                         "défaut 0.5)")
    ap.add_argument("--avoid", nargs="*", default=[], metavar="TEXTE",
                    help="sous-chaînes de noms de sections à éviter tant "
                         "qu'une autre option existe (ex : hotel nuit)")
    ap.add_argument("--stat", metavar="NOM",
                    help="affiche la distribution de cette caractéristique "
                         "en fin de partie")
    ap.add_argument("--max-steps", type=int, default=3000,
                    help="sections max par partie avant de conclure à une "
                         "boucle (défaut 3000)")
    ap.add_argument("--trail", type=int, default=8, metavar="N",
                    help="sections précédentes affichées pour une impasse "
                         "(défaut 8)")
    args = ap.parse_args(argv)

    story = parse(args.adv.read_text(encoding="utf-8"))
    resolve(story)
    if args.stat and args.stat not in {d.name for d in story.stats}:
        ap.error(f"caractéristique inconnue : {args.stat}")
    if not 0 <= args.p_correct <= 1:
        ap.error("--p-correct doit être entre 0 et 1")
    return _report(Simulator(story, args.p_correct), args)


if __name__ == "__main__":
    sys.exit(main())
