"""Migration des flags locaux : du préambule vers leurs chapitres.

Un flag `local` se déclare dans le chapitre qui l'utilise (après son
`@chapter`) et n'existe que dans ce chapitre. Cet outil convertit une
aventure écrite avec des `@flag NOM local` dans le préambule :

  - chaque déclaration du préambule est retirée ;
  - une déclaration est ajoutée sous le `@chapter` de **chaque** chapitre où
    le flag est utilisé. Un local est remis à zéro à chaque changement de
    chapitre : un exemplaire par chapitre se comporte exactement comme
    l'ancien flag unique, la migration ne change donc rien au jeu ;
  - un flag jamais utilisé est supprimé (avertissement).

Avertissements utiles : un local lu dans un chapitre où il n'est jamais posé
est toujours faux, presque toujours une erreur d'écriture.

Usage : python3 -m a2c.migrate_locals aventure.adv [--write]
Sans --write, affiche seulement ce qui changerait.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from .errors import A2Error
from .parser import parse

_DECL = re.compile(r"^(\s*)@flag\s+(\S+)((?:\s+\S+)*?)\s+local\b\s*(?:#\s*(.*))?$")


def _flag_refs(sec):
    """(nom, ecrit) pour chaque reference de flag d'une section ; ecrit est
    vrai pour un effet set/clear/toggle, faux pour une simple lecture."""
    def from_cond(cond):
        for a in cond.atoms:
            if a.op in ("flag", "not_flag"):
                yield a.name, False

    def from_effect(e):
        yield from from_cond(e.cond)
        if e.op in ("set", "clear", "toggle"):
            yield e.name, True

    for e in sec.on_enter + sec.on_exit:
        yield from from_effect(e)
    for t in sec.texts:
        yield from from_cond(t.cond)
    for c in sec.choices:
        yield from from_cond(c.cond)
        for e in c.effects:
            yield from from_effect(e)
    if sec.combat is not None:
        for e in sec.combat.win_effects + sec.combat.lose_effects + sec.combat.flee_effects:
            yield from from_effect(e)
    if sec.input is not None:
        for e in sec.input.correct_effects + sec.input.wrong_effects:
            yield from from_effect(e)


def migrate(text: str) -> tuple[str, list[str]]:
    """Renvoie (nouveau texte, messages). Le texte est inchange si le
    préambule ne déclare aucun flag local."""
    lines = text.split("\n")
    notes: list[str] = []

    first_section = next((i for i, l in enumerate(lines) if l.lstrip().startswith("::")),
                         len(lines))
    decls: list[tuple[int, str, str]] = []          # (ligne, nom, commentaire de fin)
    for i in range(first_section):
        m = _DECL.match(lines[i])
        if m:
            decls.append((i, m.group(2), m.group(4) or ""))
    if not decls:
        return text, notes

    drop = {i for i, _, _ in decls}
    kept = [l for i, l in enumerate(lines) if i not in drop]

    story = parse("\n".join(kept))
    names = [n for _, n, _ in decls]

    # usage par chapitre : nom -> chapitre -> (lu, ecrit)
    usage: dict[str, dict[int, list[bool]]] = {n: {} for n in names}
    for sec in story.sections:
        for name, written in _flag_refs(sec):
            if name in usage:
                cell = usage[name].setdefault(sec.chapter, [False, False])
                cell[1 if written else 0] = True

    per_chapter: dict[int, list[tuple[str, str]]] = {}
    trails = {n: t for _, n, t in decls}
    for name in names:
        chapters = usage[name]
        if not chapters:
            notes.append(f"'{name}' n'est jamais utilisé : supprimé")
            continue
        if 0 in chapters:
            raise A2Error(f"'{name}' est utilisé avant le premier @chapter : "
                          "ajouter un @chapter avant la première section, ou "
                          "en faire un flag global")
        for c, (read, written) in sorted(chapters.items()):
            per_chapter.setdefault(c, []).append((name, trails[name]))
            if read and not written:
                title = story.chapters[c]
                notes.append(f"'{name}' est lu dans le chapitre « {title} » sans "
                             "y être jamais posé : toujours faux, à vérifier")

    # insertion sous chaque @chapter (l'indice de chapitre suit l'ordre d'apparition)
    out: list[str] = []
    chapter = 0
    for l in kept:
        out.append(l)
        if l.lstrip().startswith("@chapter"):
            chapter += 1
            for name, trail in per_chapter.get(chapter, []):
                out.append(f"@flag {name} local" + (f"  # {trail}" if trail else ""))
    moved = sum(len(v) for v in per_chapter.values())
    notes.append(f"{len(decls)} flag(s) local(aux) du préambule -> {moved} "
                 f"déclaration(s) dans {len(per_chapter)} chapitre(s)")
    return "\n".join(out), notes


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="python3 -m a2c.migrate_locals",
        description="Déplace les @flag local du préambule vers leurs chapitres.")
    ap.add_argument("source", type=Path, help="fichier .adv")
    ap.add_argument("--write", action="store_true",
                    help="réécrit le fichier (sinon, simple compte rendu)")
    args = ap.parse_args(argv)

    text = args.source.read_text(encoding="utf-8")
    try:
        new, notes = migrate(text)
    except A2Error as e:
        print(f"a2c: {args.source}: {e}", file=sys.stderr)
        return 1
    for n in notes:
        print(f"  {n}")
    if new == text:
        print("  rien à migrer")
        return 0
    if args.write:
        args.source.write_text(new, encoding="utf-8")
        print(f"  {args.source} réécrit")
    else:
        print("  (simulation : relancer avec --write pour appliquer)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
