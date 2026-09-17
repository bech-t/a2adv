"""Story RESOLUE (post `symbols.resolve`) -> JSON pour le player web.

A ne pas confondre avec `jsonconv.py` : celui-la exporte le modele SOURCE
(noms symboliques, pre-resolution), pour editer/round-tripper le `.adv`.
Celui-ci exporte la MEME information que `encoder.py` met dans STORY.DAT/
APP.LNG -- noms resolus en indices, flags locaux tries en fin de table,
reponses `@ask` normalisees, marqueurs `*...*` deja convertis en octet-
bascule -- mais en JSON au lieu d'octets empaquetes. `symbols.resolve` reste
l'UNIQUE endroit qui fait le travail de resolution/validation ; ce module ne
fait QUE le serialiser, comme `encoder.py` le fait pour le binaire (memes
tables d'opcodes, dupliquees ici car `encoder.py` les garde privees).

Cote web, `player/web/src/engine/story.ts` (`loadStoryJson`) fait l'inverse :
JSON -> les memes structures en memoire (`StoryData`/`SectionBody`) que
produisait jusqu'ici le decodage du binaire -- le reste du moteur TS
(state.ts/combat.ts/engine.ts) ne voit aucune difference.

Les noms de champs sont en camelCase (pas la convention Python usuelle) :
c'est un format de sortie pour le moteur TS, pas une structure Python
idiomatique -- le JSON produit doit se lire comme du TS quand on ouvre les
deux fichiers cote a cote.

Contrairement a l'encodeur binaire, le texte n'est PAS passe par
`translit.normalize_display` (aplatissement des ligatures/guillemets
courbes) : cet aplatissement existe uniquement parce qu'aucune police
materielle cible (Apple II, Atari ST, DOS/VGA) ne sait rendre ces
caracteres, une limite que le DOM n'a pas (cf. player/web/src/engine/
reader.ts). Le texte JSON garde donc sa typographie d'origine.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

from . import model as M
from .errors import A2Error
from .parser import parse, parse_lang
from .symbols import Symbols, resolve

LANG_DIR = Path(__file__).resolve().parents[2] / "lang"

_ATOM_OP = {
    "flag": M.OP_FLAG_SET, "not_flag": M.OP_FLAG_CLR,
    "has": M.OP_HAS_ITEM, "not_has": M.OP_NO_ITEM, "stat": M.OP_STAT_CMP,
}
_EFFECT_OP = {
    "set": M.OP_SET_FLAG, "clear": M.OP_CLR_FLAG, "toggle": M.OP_TOG_FLAG,
    "give": M.OP_GIVE_ITEM, "take": M.OP_TAKE_ITEM,
    "add": M.OP_STAT_ADD, "sub": M.OP_STAT_SUB, "setstat": M.OP_STAT_SET,
    "goto": M.OP_GOTO, "sound": M.OP_SOUND, "score": M.OP_SCORE_ADD,
    "restore": M.OP_STAT_MAX, "setmax": M.OP_STAT_SETMAX,
}


def _atom(a: M.Atom, sym: Symbols) -> dict:
    op = _ATOM_OP[a.op]
    if a.op in ("flag", "not_flag"):
        return {"op": op, "a0": sym.flags[a.name], "a1": 0, "a2": 0}
    if a.op in ("has", "not_has"):
        return {"op": op, "a0": sym.items[a.name], "a1": 0, "a2": 0}
    return {"op": op, "a0": sym.stats[a.name], "a1": int(a.cmp), "a2": a.value}


def _cond(cond: M.Condition, sym: Symbols) -> dict:
    return {"conn": cond.connective, "atoms": [_atom(a, sym) for a in cond.atoms]}


def _effect(e: M.Effect, sym: Symbols) -> dict:
    op = _EFFECT_OP[e.op]
    if e.op in ("set", "clear", "toggle"):
        base = {"a0": sym.flags[e.name], "a1": 0, "a2": 0}
    elif e.op in ("give", "take"):
        base = {"a0": sym.items[e.name], "a1": 0, "a2": 0}
    elif e.op in ("add", "sub", "setstat", "setmax"):
        base = {"a0": sym.stats[e.name], "a1": e.value, "a2": 0}
    elif e.op == "restore":
        base = {"a0": sym.stats[e.name], "a1": 0, "a2": 0}
    elif e.op == "sound":
        base = {"a0": M.SOUND_INDEX[e.name], "a1": 0, "a2": 0}
    elif e.op == "score":
        base = {"a0": e.value & 0xFF, "a1": 0, "a2": 0}
    else:  # goto : indice de section sur 16 bits (a0=lo, a1=hi), cf. encoder.py
        idx = sym.sections[e.name]
        base = {"a0": idx & 0xFF, "a1": (idx >> 8) & 0xFF, "a2": 0}
    return {"guard": _cond(e.cond, sym), "op": op, **base}


def _effects(effects: list[M.Effect], sym: Symbols) -> list[dict]:
    return [_effect(e, sym) for e in effects]


def _text(t: M.TextSegment, sym: Symbols) -> dict:
    # marqueurs inline *...* -> octet-bascule inverse (invisible), cf.
    # encoder.py:_encode_text -- RichText.tsx cote web attend deja ce codage.
    body = t.text.replace("*", chr(M.TXT_INV_TOGGLE))
    return {"cond": _cond(t.cond, sym), "style": t.style, "text": body}


def _choice(c: M.Choice, sym: Symbols) -> dict:
    return {"cond": _cond(c.cond, sym), "effects": _effects(c.effects, sym),
            "target": c.target_index, "label": c.label}


def _combat(cb: M.Combat, sym: Symbols) -> dict:
    return {
        "att": cb.att, "hp": cb.hp, "dmg": cb.dmg, "armor": cb.armor,
        "eimg": cb.image_asset, "win": cb.win_index, "lose": cb.lose_index,
        "flee": cb.flee_index, "name": cb.name,
        "winFx": _effects(cb.win_effects, sym),
        "loseFx": _effects(cb.lose_effects, sym),
        "fleeFx": _effects(cb.flee_effects, sym),
        "winMsg": cb.win_msg, "loseMsg": cb.lose_msg, "fleeMsg": cb.flee_msg,
    }


def _input(ip: M.Input, sym: Symbols) -> dict:
    return {
        "prompt": ip.prompt, "maxlen": ip.maxlen, "answers": list(ip.answers),
        "correct": ip.correct_index, "wrong": ip.wrong_index,
        "correctFx": _effects(ip.correct_effects, sym),
        "wrongFx": _effects(ip.wrong_effects, sym),
    }


def _section(sec: M.Section, sym: Symbols) -> dict:
    return {
        "mode": int(sec.mode), "ending": int(sec.ending),
        "image": sec.image_asset,
        "combat": _combat(sec.combat, sym) if sec.combat else None,
        "input": _input(sec.input, sym) if sec.input else None,
        "onEnter": _effects(sec.on_enter, sym),
        "onExit": _effects(sec.on_exit, sym),
        "texts": [_text(t, sym) for t in sec.texts],
        "choices": [_choice(c, sym) for c in sec.choices],
        "chapter": sec.chapter,
    }


def resolved_to_dict(story: M.Story, ui_strings: dict[str, str]) -> dict:
    """`story` doit deja avoir ete resolue (`symbols.resolve(story)`).
    `ui_strings` : le socle .lng complet (toutes les cles UI_KEYS) ; les
    surcharges `@ui` de l'aventure sont fusionnees ICI, une seule fois -- le
    moteur web n'a plus besoin de merger un socle + des surcharges a
    l'execution, `uiStrings` est deja la table finale."""
    sym = Symbols(story)
    merged_ui = [ui_strings[key] for key, _default in M.UI_KEYS]
    for i, (key, _default) in enumerate(M.UI_KEYS):
        if key in story.ui:
            merged_ui[i] = story.ui[key]

    return {
        "scoreOn": story.score_on,
        "movesOn": story.moves_on,
        "start": story.start_index,
        "localBase": story.local_base,
        "title": story.title,
        "adventureVersion": story.version,
        "stats": [{"name": s.name, "init": s.init, "min": s.lo, "max": s.hi,
                   "hidden": s.hidden} for s in story.stats],
        "items": [{"label": it.label or it.name, "defaultOn": it.default_on,
                   "atk": it.atk, "dmg": it.dmg, "armor": it.armor}
                 for it in story.items],
        "flags": [{"defaultOn": fl.default_on} for fl in story.flags],
        "introIndex": list(story.intro_index),
        "combatAttackIndex": story.combat_attack_index,
        "combatHpIndex": story.combat_hp_index,
        "combatBaseDmg": story.combat_base_dmg,
        "uiStrings": merged_ui,
        "sections": [_section(sec, sym) for sec in story.sections],
    }


def copy_web_images(assets: list[str], src_dir: Path, out_dir: Path) -> list[str]:
    """Copie les images NOMMEES vers IMGnn.png, numerotees dans l'ordre de
    premiere apparition (`story.assets`, resolu par `symbols.resolve` --
    meme ordre qu'IMAGES.MAP cote natif, cf. cli.py). Source attendue :
    `<adv>/img/web/<ID EN MAJUSCULES>.png`, a fournir a la main -- meme
    principe que `img/named/<ID>.HGR` pour l'Apple II ou
    `img/atarist/<ID>.PI1` pour l'Atari ST (cf. leurs Makefile respectifs) :
    aucune conversion de palette/resolution n'a de sens pour le web (le
    navigateur affiche du PNG nativement), donc pas d'outil `img2web.py` --
    juste une image deja prete, numerotee automatiquement ici pour ne pas
    desynchroniser un index a la main si l'ordre des `@image` change.

    Une image manquante n'est qu'un avertissement (retourne dans la liste),
    jamais une erreur : une aventure sans export web de ses visuels reste
    jouable en texte (cf. player/*/src/**/assets/loader.ts, onError sur
    <img>)."""
    if not assets:
        return []
    warnings = []
    found: list[tuple[int, Path]] = []
    for i, name in enumerate(assets):
        src = src_dir / f"{name.upper()}.png"
        if src.exists():
            found.append((i, src))
        else:
            warnings.append(f"image manquante pour @image {name} : {src}")
    if found:
        out_dir.mkdir(parents=True, exist_ok=True)
        for i, src in found:
            shutil.copyfile(src, out_dir / f"IMG{i:02d}.png")
    return warnings


def build_web_json(adv_text: str, lang_dir: Path) -> dict:
    """Parse + resout une source .adv et produit son JSON pour le player web,
    en allant chercher son socle .lng (`@lang`) dans `lang_dir`."""
    story = parse(adv_text)
    resolve(story)
    lng_path = lang_dir / f"{story.lang}.lng"
    if not lng_path.exists():
        raise A2Error(f"@lang {story.lang} : fichier de langue introuvable "
                      f"({lng_path})")
    _, ui_strings, _ = parse_lang(lng_path.read_text(encoding="utf-8"))
    return resolved_to_dict(story, ui_strings)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="a2c.webjson",
        description="Compile une source .adv vers le JSON attendu par le "
                    "player web (player/web/src/engine), et copie ses "
                    "images web (img/web/*.png, cf. copy_web_images) a cote "
                    "du JSON produit.")
    ap.add_argument("source", help="fichier .adv source")
    ap.add_argument("-o", "--out", help="fichier de sortie (defaut: story.json)")
    args = ap.parse_args(argv)

    src = Path(args.source)
    if not src.exists():
        print(f"a2c.webjson: fichier introuvable: {src}", file=sys.stderr)
        return 2

    # Pas d'appel a build_web_json ici : il faut l'objet Story RESOLU (pour
    # story.assets, cf. copy_web_images plus bas), pas seulement le dict
    # final qu'il retourne -- meme sequence parse/resolve que build_web_json,
    # dupliquee ici pour cette seule raison.
    try:
        story = parse(src.read_text(encoding="utf-8"))
        resolve(story)
        lng_path = LANG_DIR / f"{story.lang}.lng"
        if not lng_path.exists():
            raise A2Error(f"@lang {story.lang} : fichier de langue introuvable "
                          f"({lng_path})")
        _, ui_strings, _ = parse_lang(lng_path.read_text(encoding="utf-8"))
        data = resolved_to_dict(story, ui_strings)
    except A2Error as e:
        print(f"a2c.webjson: {src.name}: {e}", file=sys.stderr)
        return 1

    out = Path(args.out) if args.out else Path("story.json")
    out.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"a2c.webjson: {src} -> {out}")

    img_warnings = copy_web_images(story.assets, src.parent / "img" / "web", out.parent / "img")
    for w in img_warnings:
        print(f"a2c.webjson: attention: {w}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
