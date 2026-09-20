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

Cote web, `player/webng/src/app/engine/story.ts` (`loadStoryJson`) fait l'inverse :
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
caracteres, une limite que le DOM n'a pas (cf. player/webng/src/app/
engine/). Le texte JSON garde donc sa typographie d'origine.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from . import model as M
from .errors import A2Error
from .parser import parse, parse_lang
from .symbols import Symbols, resolve, substitute_stat_refs

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
    return {"clauses": [[_atom(a, sym) for a in clause] for clause in cond.clauses]}


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


def _display_text(text: str, sym: Symbols) -> str:
    """%NOM% -> reference de stat, *...* -> octet-bascule inverse (invisible) --
    meme codage que cote binaire (cf. encoder.py:_encode_section), deja
    valide par resolve(). RichText.tsx/rich-text.ts cote web attendent deja
    ce codage pour '*' ; l'extension %NOM% suit le meme principe."""
    return substitute_stat_refs(text, sym).replace("*", chr(M.TXT_INV_TOGGLE))


def _text(t: M.TextSegment, sym: Symbols) -> dict:
    body = _display_text(t.text, sym)
    return {"cond": _cond(t.cond, sym), "style": t.style, "text": body}


def _choice(c: M.Choice, sym: Symbols) -> dict:
    return {"cond": _cond(c.cond, sym), "effects": _effects(c.effects, sym),
            "target": c.target_index, "label": _display_text(c.label, sym)}


def _combat(cb: M.Combat, sym: Symbols) -> dict:
    return {
        "att": cb.att, "hp": cb.hp, "dmg": cb.dmg, "armor": cb.armor,
        "eimg": cb.image_asset, "win": cb.win_index, "lose": cb.lose_index,
        "flee": cb.flee_index, "name": cb.name,
        "winFx": _effects(cb.win_effects, sym),
        "loseFx": _effects(cb.lose_effects, sym),
        "fleeFx": _effects(cb.flee_effects, sym),
        "winMsg": _display_text(cb.win_msg, sym),
        "loseMsg": _display_text(cb.lose_msg, sym),
        "fleeMsg": _display_text(cb.flee_msg, sym),
    }


def _input(ip: M.Input, sym: Symbols) -> dict:
    return {
        "prompt": _display_text(ip.prompt, sym), "maxlen": ip.maxlen,
        "answers": list(ip.answers),
        "correct": ip.correct_index, "wrong": ip.wrong_index,
        "correctFx": _effects(ip.correct_effects, sym),
        "wrongFx": _effects(ip.wrong_effects, sym),
    }


def _section(sec: M.Section, sym: Symbols) -> dict:
    return {
        "mode": int(sec.mode), "ending": int(sec.ending),
        "image": sec.image_asset,
        "splash": ({"asset": sec.splash_asset, "secs": sec.splash_secs,
                    "always": sec.splash_always} if sec.splash else None),
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
        "flags": [{"defaultOn": on} for on in story.flag_defaults()],
        "introIndex": list(story.intro_index),
        "combatAttackIndex": story.combat_attack_index,
        "combatHpIndex": story.combat_hp_index,
        "combatBaseDmg": story.combat_base_dmg,
        "uiStrings": merged_ui,
        "sections": [_section(sec, sym) for sec in story.sections],
    }


def load_ui_strings(lang: str, lang_dir: Path = LANG_DIR) -> dict[str, str]:
    """Chaines d'interface du player web : le socle `<lang>.lng`, surcharge
    par `web/<lang>.lng` s'il existe (libelles adaptes au tactile et a la
    casse mixte du navigateur, cf. lang/README.md). Le fichier web ne
    redonne que les cles qui different."""
    base = lang_dir / f"{lang}.lng"
    if not base.exists():
        raise A2Error(f"@lang {lang} : fichier de langue introuvable ({base})")
    _, ui_strings, _ = parse_lang(base.read_text(encoding="utf-8"))
    overlay = lang_dir / "web" / f"{lang}.lng"
    if overlay.exists():
        _, over, _ = parse_lang(overlay.read_text(encoding="utf-8"))
        ui_strings.update(over)
    return ui_strings


def compile_story(src: Path, lang_dir: Path = LANG_DIR) -> tuple[M.Story, dict]:
    """Parse + resout une source .adv : (Story resolue, JSON du player web).
    La Story est rendue aussi pour ses metadonnees (`title`, `assets`...)."""
    story = parse(src.read_text(encoding="utf-8"))
    resolve(story)
    return story, resolved_to_dict(story, load_ui_strings(story.lang, lang_dir))


def build_web_json(adv_text: str, lang_dir: Path) -> dict:
    """Parse + resout une source .adv et produit son JSON pour le player web,
    en allant chercher son socle .lng (`@lang`) dans `lang_dir`."""
    story = parse(adv_text)
    resolve(story)
    return resolved_to_dict(story, load_ui_strings(story.lang, lang_dir))


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="a2c.webjson",
        description="Compile une source .adv vers le JSON attendu par le "
                    "player web (player/webng/src/app/engine). Pour un site "
                    "complet (catalogue, images), voir a2c.site.")
    ap.add_argument("source", help="fichier .adv source")
    ap.add_argument("-o", "--out", help="fichier de sortie (defaut: story.json)")
    args = ap.parse_args(argv)

    src = Path(args.source)
    if not src.exists():
        print(f"a2c.webjson: fichier introuvable: {src}", file=sys.stderr)
        return 2
    try:
        _, data = compile_story(src)
    except A2Error as e:
        print(f"a2c.webjson: {src.name}: {e}", file=sys.stderr)
        return 1

    out = Path(args.out) if args.out else Path("story.json")
    out.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"a2c.webjson: {src} -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
