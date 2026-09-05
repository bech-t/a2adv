"""CLI du compilateur : a2c <source.adv> -o <dossier_sortie>."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import __version__
from .encoder import encode_assets, encode_lang, encode_story
from .errors import A2Error
from .parser import parse, parse_lang
from .symbols import resolve

# Socles d'interface : lang/<code>.lng a la racine du depot (cf. spec §6.1).
LANG_DIR = Path(__file__).resolve().parents[2] / "lang"


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="a2c", description="Compilateur DSL .adv -> STORY.DAT + ASSETS.IDX")
    ap.add_argument("source", help="fichier .adv source")
    ap.add_argument("-o", "--out", default="build",
                    help="dossier de sortie (défaut: build)")
    ap.add_argument("--max-file", type=lambda s: int(s, 0), default=None,
                    help="taille max d'un STORYn.DAT en octets (force le "
                         "sous-decoupage ; utile pour tester le multi-fichiers)")
    # Casse du texte encodé. Le generateur de caracteres des machines cibles
    # n'a aucun glyphe accentue : les accents sont retires dans tous les cas
    # (cf. translit.py). Reste la casse, et elle depend de l'ecran — d'ou le
    # choix laisse a l'auteur plutot qu'une regle figee.
    ap.add_argument("--majuscules", dest="upper", action="store_true",
                    help="force tout le texte en MAJUSCULES (rendu d'origine, "
                         "affichable sur n'importe quel Apple II) ; par défaut "
                         "la casse du source est conservée, ce qui suppose un "
                         "//e à carte 80 colonnes pour être lisible")
    ap.add_argument("--summary", action="store_true",
                    help="affiche un résumé de l'aventure compilée")
    ap.add_argument("--version", action="version",
                    version=f"a2c {__version__}")
    args = ap.parse_args(argv)

    src = Path(args.source)
    if not src.exists():
        print(f"a2c: fichier introuvable: {src}", file=sys.stderr)
        return 2

    try:
        story = parse(src.read_text(encoding="utf-8"))
        warnings = resolve(story)
        if args.max_file is not None:
            story_files = encode_story(story, max_file=args.max_file,
                                       upper=args.upper)
        else:
            story_files = encode_story(story, upper=args.upper)
        assets_bin = encode_assets(story)
        # APP.LNG : socle d'interface choisi par le @lang de l'aventure. C'est
        # ce qui rend impossible une disquette dont l'interface et l'histoire
        # ne parlent pas la meme langue.
        lng_src = LANG_DIR / f"{story.lang}.lng"
        if not lng_src.exists():
            raise A2Error(f"@lang {story.lang} : fichier de langue introuvable "
                          f"({lng_src}). Langues disponibles : "
                          + (", ".join(sorted(p.stem for p in LANG_DIR.glob("*.lng")))
                             or "(aucune)"))
        lang_code, lang_strings = parse_lang(lng_src.read_text(encoding="utf-8"))
        lang_bin = encode_lang(lang_code, lang_strings, upper=args.upper)
    except A2Error as e:
        print(f"a2c: {src.name}: {e}", file=sys.stderr)
        return 1

    for w in warnings:
        print(f"a2c: attention: {w}", file=sys.stderr)

    # STORY0.DAT (+ STORY1.DAT, ... si multi-fichiers par chapitre). STORY0 porte
    # header + preambule + index global ; ASSETS.IDX reste unique et partage.
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    total = 0
    for i, blob in enumerate(story_files):
        (out / f"STORY{i:02d}.DAT").write_bytes(blob)   # 2 chiffres : 00..99
        total += len(blob)
    (out / "ASSETS.IDX").write_bytes(assets_bin)
    (out / "APP.LNG").write_bytes(lang_bin)
    # IMAGES.MAP : correspondance id `@image` -> index IMGnn.HGR (= ordre de
    # premiere apparition dans le source). Le Makefile s'en sert pour
    # regenerer les fichiers numerotes a partir des fichiers NOMMES
    # (img/named/<ID>.HGR) : l'auteur n'a jamais a compter/nommer un index a
    # la main, et reordonner les @image ne desynchronise plus rien en
    # silence (cf. spec, notes de portage).
    images_map = "".join(f"{i:02d} {name}\n" for i, name in enumerate(story.assets))
    (out / "IMAGES.MAP").write_text(images_map, encoding="ascii")

    names = ", ".join(f"STORY{i:02d}.DAT" for i in range(len(story_files)))
    print(f"a2c: {src.name} -> {names} ({total} o), "
          f"{out}/ASSETS.IDX ({len(assets_bin)} o), "
          f"{out}/APP.LNG ({len(lang_bin)} o, {story.lang})")
    if args.summary:
        _print_summary(story)
    return 0


def _print_summary(story) -> None:
    print(f"  titre    : {story.title!r}")
    print(f"  auteur   : {story.author!r}")
    print(f"  départ   : {story.start} (index {story.start_index})")
    print(f"  sections : {len(story.sections)}")
    print(f"  stats    : {', '.join(s.name for s in story.stats)}")
    print(f"  objets   : {', '.join(i.name for i in story.items)}")
    print(f"  flags    : {', '.join(f.name for f in story.flags)}")
    print(f"  images   : {', '.join(story.assets) or '(aucune)'}")


if __name__ == "__main__":
    raise SystemExit(main())
