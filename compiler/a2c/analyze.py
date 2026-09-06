"""Analyse statique d'une aventure : détecte les défauts de conception.

Vérifie sur le graphe des sections (conditions ignorées = reachabilité optimiste) :
  - sections inatteignables depuis @start ;
  - culs-de-sac (section sans choix qui n'est pas une fin) ;
  - joignabilité d'au moins une fin victoire et une fin défaite ;
  - objets / flags déclarés mais jamais utilisés ;
  - objets requis (has/take) jamais octroyés (give) ni possédés au départ ;
  - flags testés mais jamais posés (condition toujours fausse) ;
  - conditions sur une CARACTERISTIQUE numérique jamais satisfiables : la
    reachabilité par nom (ci-dessus) ne voit que les flags/objets, jamais les
    bornes numeriques — un choix garde par `stat X >= N` peut etre liste comme
    « atteignable » alors qu'aucun chemin ne peut faire monter X jusque-la.

    LIMITE CONNUE, IMPORTANTE : la borne calculee est « le meilleur/pire cas
    tous chemins confondus atteignant cette section », PAS « le meilleur cas
    parmi les chemins qui satisfont aussi les AUTRES atomes de la condition ».
    Un garde `{flag F and stat X >= N}` ou F et un X eleve ne sont jamais
    obtenus par le MEME chemin (typiquement : une piste alternative donne F a
    bas cout en X, une autre garde X haut sans jamais poser F) ne sera PAS
    detecte, puisque la meilleure valeur de X vue par le solveur peut venir
    d'un chemin qui ne pose pas F. Cette approximation reste utile pour
    l'immense majorite des gardes reels (un seul atome numerique, sans
    flag correle) mais ne remplace pas une relecture attentive d'un garde
    combinant explicitement un flag ET un seuil de caracteristique.

Usage : python3 -m a2c.analyze <aventure.adv>
"""

from __future__ import annotations

import sys
from collections import deque
from pathlib import Path

from .model import Cmp, Ending, Story
from .parser import parse
from .symbols import resolve


def _edges(sec) -> list[str]:
    """Cibles sortantes d'une section (choix + goto d'effets + issues de combat)."""
    out = []
    for e in sec.on_enter:
        if e.op == "goto":
            out.append(e.name)
    for c in sec.choices:
        out.append(c.target)
        for e in c.effects:
            if e.op == "goto":
                out.append(e.name)
    if sec.combat is not None:              # issues de combat = liens sortants
        out.append(sec.combat.win)
        out.append(sec.combat.lose)
        if sec.combat.flee:
            out.append(sec.combat.flee)
    if sec.input is not None:               # issues de saisie = liens sortants
        out.append(sec.input.correct)
        out.append(sec.input.wrong)
    return out


def _stat_range(story: Story, by_name: dict, stat, maximize: bool) -> dict:
    """Meilleure (maximize=True) ou pire (False) valeur ATTEIGNABLE de `stat`
    en entree de chaque section, par relaxation sur le graphe (Bellman-Ford :
    on ameliore tant qu'un chemin fait mieux, borne par les valeurs entieres
    0..255 donc toujours convergent).

    Approximation deliberement simple et documentee : un effet CONDITIONNEL
    n'est suivi que s'il va dans le sens qu'on calcule (un gain conditionnel
    compte pour le meilleur cas, pas pour le pire ; symetrique pour une perte).
    Un effet INCONDITIONNEL est toujours suivi, dans les deux sens : il est
    subi par construction, quel que soit le chemin.
    """
    lo, hi = stat.lo, stat.hi

    def clamp(v, mx):
        return max(lo, min(mx, v))

    def favorable(delta):
        return delta >= 0 if maximize else delta <= 0

    def apply(state, effects):
        val, mx = state
        for e in effects:
            if e.name != stat.name:
                continue
            forced = e.cond.always
            if e.op == "add":
                if forced or favorable(e.value):
                    val = clamp(val + e.value, mx)
            elif e.op == "sub":
                if forced or favorable(-e.value):
                    val = clamp(val - e.value, mx)
            elif e.op == "setstat":
                nv = clamp(e.value, mx)
                if forced or favorable(nv - val):
                    val = nv
            elif e.op == "setmax":
                if forced or favorable(e.value - mx):
                    mx = e.value
                    val = clamp(val, mx)
            elif e.op == "restore":
                if forced or favorable(mx - val):
                    val = mx
        return (val, mx)

    def better(a, b):
        return a[0] > b[0] if maximize else a[0] < b[0]

    def edges(sec):
        out = []
        for c in sec.choices:
            out.append((c.target, list(c.effects) + list(sec.on_exit)))
        cb = sec.combat
        if cb is not None:
            for tgt, effs in ((cb.win, cb.win_effects), (cb.lose, cb.lose_effects),
                              (cb.flee, cb.flee_effects)):
                if tgt:
                    out.append((tgt, list(effs) + list(sec.on_exit)))
        ip = sec.input
        if ip is not None:
            for tgt, effs in ((ip.correct, ip.correct_effects),
                              (ip.wrong, ip.wrong_effects)):
                if tgt:
                    out.append((tgt, list(effs) + list(sec.on_exit)))
        return out

    best: dict[str, tuple[int, int]] = {}
    q = deque()
    guard = 0
    for r in [story.start] + list(story.intro):
        sec = by_name.get(r)
        if sec is None:
            continue
        v = apply((stat.init, hi), sec.on_enter)
        if r not in best or better(v, best[r]):
            best[r] = v
            q.append(r)
    while q and guard < 200000:
        guard += 1
        name = q.popleft()
        sec = by_name.get(name)
        if sec is None:
            continue
        cur = best[name]
        for tgt, effs in edges(sec):
            tsec = by_name.get(tgt)
            if tsec is None:
                continue
            v = apply(apply(cur, effs), tsec.on_enter)
            if tgt not in best or better(v, best[tgt]):
                best[tgt] = v
                q.append(tgt)
    return best


def analyze(story: Story) -> int:
    by_name = {s.name: s for s in story.sections}
    issues = 0

    # --- reachabilité depuis @start (+ scènes d'intro comme racines) ---
    seen = set()
    q = deque([story.start] + list(story.intro))
    while q:
        name = q.popleft()
        if name in seen or name not in by_name:
            continue
        seen.add(name)
        q.extend(_edges(by_name[name]))

    unreachable = [s.name for s in story.sections if s.name not in seen]
    if unreachable:
        issues += len(unreachable)
        print(f"[!] {len(unreachable)} section(s) INATTEIGNABLE(s) : "
              + ", ".join(unreachable))

    # --- culs-de-sac (pas de choix, pas une fin, pas une scène d'intro) --
    intro = set(story.intro)

    def _has_goto(sec, only_unconditional=False):
        return any(e.op == "goto" and (not only_unconditional or not e.cond.atoms)
                   for e in sec.on_enter)

    # Une section d'AIGUILLAGE (que des `~ goto` en @on_enter, cf. spec §6.1)
    # n'est pas un cul-de-sac : son `goto` est sa sortie.
    dead = [s.name for s in story.sections
            if not s.choices and s.ending == Ending.NONE
            and s.combat is None and s.input is None and s.name not in intro
            and not _has_goto(s)]
    if dead:
        issues += len(dead)
        print(f"[!] {len(dead)} cul(s)-de-sac (aucun choix, pas @ending) : "
              + ", ".join(dead))

    # ... mais un aiguillage dont TOUS les goto sont gardes peut ne rien
    # declencher et laisser le joueur bloque. Il lui faut un cas par defaut.
    leaky = [s.name for s in story.sections
             if not s.choices and s.ending == Ending.NONE
             and s.combat is None and s.input is None and s.name not in intro
             and _has_goto(s) and not _has_goto(s, only_unconditional=True)]
    if leaky:
        issues += len(leaky)
        print(f"[!] {len(leaky)} aiguillage(s) sans goto inconditionnel "
              f"(si aucune garde n'est vraie, le joueur reste bloque) : "
              + ", ".join(leaky))

    # --- joignabilité des fins ----------------------------------------
    vic = [s.name for s in story.sections
           if s.ending == Ending.WIN and s.name in seen]
    defeats = [s.name for s in story.sections
           if s.ending == Ending.LOSE and s.name in seen]
    if not vic:
        issues += 1
        print("[!] aucune fin VICTOIRE atteignable")
    if not defeats:
        print("[i] aucune fin DEFAITE atteignable (ok si volontaire)")

    # --- usage des objets / flags -------------------------------------
    granted, consumed, checked_i = set(), set(), set()
    written_f, checked_f = set(), set()
    for s in story.sections:
        effs = list(s.on_enter) + list(s.on_exit) \
            + [ef for c in s.choices for ef in c.effects]
        if s.combat is not None:
            effs += (s.combat.win_effects + s.combat.lose_effects
                     + s.combat.flee_effects)
        if s.input is not None:
            effs += s.input.correct_effects + s.input.wrong_effects
        for e in effs:
            if e.op == "give":
                granted.add(e.name)
            elif e.op == "take":
                consumed.add(e.name)
            elif e.op in ("set", "clear", "toggle"):
                written_f.add(e.name)
        conds = [c.cond for c in s.choices] + [t.cond for t in s.texts]
        for cond in conds:
            for a in cond.atoms:
                if a.op in ("has", "not_has"):
                    checked_i.add(a.name)
                elif a.op in ("flag", "not_flag"):
                    checked_f.add(a.name)

    used_items = granted | consumed | checked_i
    unused_items = [it.name for it in story.items
                    if it.name not in used_items and not it.default_on]
    if unused_items:
        print(f"[i] objet(s) declare(s) mais jamais utilise(s) : "
              + ", ".join(unused_items))

    # objets requis mais jamais obtenables (ni give, ni possede au depart)
    default_items = {it.name for it in story.items if it.default_on}
    need = consumed | {a for a in checked_i}
    unobtainable = sorted(n for n in need
                          if n not in granted and n not in default_items)
    if unobtainable:
        issues += len(unobtainable)
        print(f"[!] objet(s) requis mais jamais octroye(s) : "
              + ", ".join(unobtainable))

    unused_flags = [f.name for f in story.flags
                    if f.name not in (written_f | checked_f) and not f.default_on]
    if unused_flags:
        print(f"[i] flag(s) declare(s) mais jamais utilise(s) : "
              + ", ".join(unused_flags))

    default_flags = {f.name for f in story.flags if f.default_on}
    dead_flags = sorted(n for n in checked_f
                        if n not in written_f and n not in default_flags)
    if dead_flags:
        print(f"[i] flag(s) teste(s) mais jamais poses (condition morte) : "
              + ", ".join(dead_flags))

    # --- bornes numeriques : une condition sur une stat jamais satisfiable --
    stat_findings = []
    for stat in story.stats:
        atoms_here = []
        for s in story.sections:
            if s.name not in seen:
                continue
            for cond in [c.cond for c in s.choices] + [t.cond for t in s.texts]:
                for a in cond.atoms:
                    if a.op == "stat" and a.name == stat.name:
                        atoms_here.append((s.name, a))
        if not atoms_here:
            continue
        needs_hi = any(a.cmp in (Cmp.GE, Cmp.GT) for _, a in atoms_here)
        needs_lo = any(a.cmp in (Cmp.LE, Cmp.LT) for _, a in atoms_here)
        hi_bounds = _stat_range(story, by_name, stat, maximize=True) if needs_hi else {}
        lo_bounds = _stat_range(story, by_name, stat, maximize=False) if needs_lo else {}
        for sname, a in atoms_here:
            if a.cmp == Cmp.GE and sname in hi_bounds and hi_bounds[sname][0] < a.value:
                stat_findings.append(
                    f"{sname}: stat {stat.name} >= {a.value} (max atteignable = {hi_bounds[sname][0]})")
            elif a.cmp == Cmp.GT and sname in hi_bounds and hi_bounds[sname][0] <= a.value:
                stat_findings.append(
                    f"{sname}: stat {stat.name} > {a.value} (max atteignable = {hi_bounds[sname][0]})")
            elif a.cmp == Cmp.LE and sname in lo_bounds and lo_bounds[sname][0] > a.value:
                stat_findings.append(
                    f"{sname}: stat {stat.name} <= {a.value} (min atteignable = {lo_bounds[sname][0]})")
            elif a.cmp == Cmp.LT and sname in lo_bounds and lo_bounds[sname][0] >= a.value:
                stat_findings.append(
                    f"{sname}: stat {stat.name} < {a.value} (min atteignable = {lo_bounds[sname][0]})")
    if stat_findings:
        issues += len(stat_findings)
        print(f"[!] {len(stat_findings)} condition(s) sur caracteristique "
              "JAMAIS SATISFIABLE(s) (borne inatteignable sur tout chemin optimal) :")
        for f in stat_findings:
            print(f"      {f}")

    # --- resume --------------------------------------------------------
    print(f"\n{len(story.sections)} sections, {len(seen)} atteignables ; "
          f"fins victoire={len(vic)} defaite={len(defeats)} ; "
          f"{'OK' if issues == 0 else str(issues) + ' probleme(s)'}.")
    return issues


def main(argv: list[str] | None = None) -> int:
    argv = argv if argv is not None else sys.argv[1:]
    if len(argv) != 1:
        print("usage: python3 -m a2c.analyze <aventure.adv>", file=sys.stderr)
        return 2
    src = Path(argv[0])
    story = parse(src.read_text(encoding="utf-8"))
    resolve(story)
    return 0 if analyze(story) == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
