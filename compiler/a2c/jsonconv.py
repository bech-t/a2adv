"""Conversion bidirectionnelle .adv <-> JSON.

Le JSON reflete le modele source (les memes champs que ceux remplis par le
parser), pas le binaire compile : `target_index`, `image_asset` et les
autres champs resolus par `symbols.resolve` n'y figurent pas, ils sont
recalcules a la compilation comme d'habitude.

Aller (.adv -> JSON) : `parse()` puis `story_to_dict`.
Retour (JSON -> .adv) : `dict_to_story` puis `render_adv`, qui reconstruit un
texte `.adv` valide. Ce texte n'est pas forcement identique octet pour octet
a une source ecrite a la main (la mise en forme -- indentation, largeur de
ligne -- ne survit pas au passage par le modele), mais il est semantiquement
equivalent et se recompile normalement via `a2c`.

Les commentaires `#` sont conserves aux endroits ou ils apparaissent
reellement dans les aventures existantes : en tete de fichier/declaration
(@stat/@item/@flag/@ui/directives scalaires du preambule), en tete de
section (::) et de choix (*), et en fin de ligne sur ces memes constructions
ainsi que sur les effets (~). Un commentaire ailleurs (avant un paragraphe
de texte, un @combat/@ask, une issue @win/@lose/@flee/@correct/@wrong, a
l'interieur d'un paragraphe) reste perdu : ca n'arrive dans aucune aventure
du depot, et gerer ce cas demanderait un point d'attache dedie qui n'existe
pas encore dans le modele.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from .errors import A2Error
from .model import (
    CMP_FROM_TEXT, STYLE_CENTER, STYLE_INVERSE, Atom, Choice, Combat,
    Condition, Effect, Ending, FlagDecl, Input, ItemDecl, Mode, Section,
    StatDecl, Story, TextSegment,
)
from .parser import parse, parse_lang

# Socle d'interface partage entre les aventures (cf. cli.py) : meme calcul.
LANG_DIR = Path(__file__).resolve().parents[2] / "lang"

CMP_TO_TEXT = {v: k for k, v in CMP_FROM_TEXT.items()}
_MODE_TO_TEXT = {Mode.FULL_TEXT: "full_text", Mode.IMAGE_TEXT: "image_text",
                 Mode.FULL_IMAGE: "full_image"}
_TEXT_TO_MODE = {v: k for k, v in _MODE_TO_TEXT.items()}
_ENDING_TO_TEXT = {Ending.NONE: None, Ending.WIN: "win", Ending.LOSE: "lose"}
_TEXT_TO_ENDING = {v: k for k, v in _ENDING_TO_TEXT.items()}


# --- .adv -> dict JSON -------------------------------------------------

def _with_comment(d: dict, lead: list[str], trail: str) -> dict:
    """Ajoute lead/trail a un dict deja construit, seulement s'ils existent."""
    if lead:
        d["lead"] = list(lead)
    if trail:
        d["trail"] = trail
    return d


def _cond_to_json(cond: Condition) -> dict:
    """Condition simple (un seul ET, ou un seul OU d'atomes) : forme historique
    {connective, atoms}. Condition composee : {clauses: [[atomes]...]}. `src`
    garde l'expression ecrite (parentheses, not, else) pour la reecrire."""
    d: dict[str, Any]
    if len(cond.clauses) == 1:
        d = {"connective": "and", "atoms": [_atom_to_json(a) for a in cond.clauses[0]]}
    elif all(len(c) == 1 for c in cond.clauses):
        d = {"connective": "or", "atoms": [_atom_to_json(c[0]) for c in cond.clauses]}
    else:
        d = {"clauses": [[_atom_to_json(a) for a in c] for c in cond.clauses]}
    if cond.src:
        d["src"] = cond.src
    return d


def _atom_to_json(a: Atom) -> dict:
    d: dict[str, Any] = {"op": a.op, "name": a.name}
    if a.op == "stat":
        d["cmp"] = CMP_TO_TEXT[a.cmp]
        d["value"] = a.value
    return d


def _effect_to_json(e: Effect) -> dict:
    d: dict[str, Any] = {"op": e.op, "name": e.name}
    if e.op in ("setstat", "add", "sub", "score", "setmax"):
        d["value"] = e.value
    if e.cond.atoms:
        d["cond"] = _cond_to_json(e.cond)
    if e.trail:
        d["trail"] = e.trail
    return d


def _text_to_json(t: TextSegment) -> dict:
    d: dict[str, Any] = {"text": t.text}
    if t.style:
        d["style"] = t.style
    if t.cond.atoms:
        d["cond"] = _cond_to_json(t.cond)
    return d


def _choice_to_json(c: Choice) -> dict:
    d: dict[str, Any] = {"label": c.label, "target": c.target}
    if c.cond.atoms:
        d["cond"] = _cond_to_json(c.cond)
    if c.effects:
        d["effects"] = [_effect_to_json(e) for e in c.effects]
    return _with_comment(d, c.lead, c.trail)


def _outcome_to_json(target: str, msg: str, effects: list[Effect]) -> dict:
    d: dict[str, Any] = {"target": target}
    if msg:
        d["msg"] = msg
    if effects:
        d["effects"] = [_effect_to_json(e) for e in effects]
    return d


def _combat_to_json(c: Combat) -> dict:
    d: dict[str, Any] = {"name": c.name, "att": c.att, "hp": c.hp,
                          "dmg": c.dmg, "armor": c.armor}
    if c.image:
        d["image"] = c.image
    d["win"] = _outcome_to_json(c.win, c.win_msg, c.win_effects)
    d["lose"] = _outcome_to_json(c.lose, c.lose_msg, c.lose_effects)
    if c.flee:
        d["flee"] = _outcome_to_json(c.flee, c.flee_msg, c.flee_effects)
    return d


def _input_to_json(i: Input) -> dict:
    return {"prompt": i.prompt, "maxlen": i.maxlen, "answers": list(i.answers),
            "correct": _outcome_to_json(i.correct, "", i.correct_effects),
            "wrong": _outcome_to_json(i.wrong, "", i.wrong_effects)}


def _section_to_json(s: Section) -> dict:
    d: dict[str, Any] = {"name": s.name, "chapter": s.chapter,
                          "mode": _MODE_TO_TEXT[s.mode]}
    if s.image:
        d["image"] = s.image
    if s.splash:
        d["splash"] = {"id": s.splash, "secs": s.splash_secs,
                       "always": s.splash_always}
    if s.ending != Ending.NONE:
        d["ending"] = _ENDING_TO_TEXT[s.ending]
    if s.on_enter:
        d["on_enter"] = [_effect_to_json(e) for e in s.on_enter]
    if s.on_exit:
        d["on_exit"] = [_effect_to_json(e) for e in s.on_exit]
    if s.texts:
        d["texts"] = [_text_to_json(t) for t in s.texts]
    if s.combat is not None:
        d["combat"] = _combat_to_json(s.combat)
    if s.input is not None:
        d["input"] = _input_to_json(s.input)
    if s.choices:
        d["choices"] = [_choice_to_json(c) for c in s.choices]
    return _with_comment(d, s.lead, s.trail)


def story_to_dict(story: Story) -> dict:
    """Convertit un modele parse (Story) en dict serialisable JSON."""
    return {
        "title": story.title,
        "version": story.version,
        "author": story.author,
        "description": story.description,
        "license": story.license,
        "start": story.start,
        "lang": story.lang,
        "score_on": story.score_on,
        "moves_on": story.moves_on,
        "combat_attack": story.combat_attack,
        "combat_hp": story.combat_hp,
        "combat_basedmg": story.combat_base_dmg,
        "stats": [_with_comment({"name": s.name, "init": s.init, "min": s.lo,
                                "max": s.hi, "hidden": s.hidden}, s.lead, s.trail)
                 for s in story.stats],
        "items": [_with_comment({"name": i.name, "label": i.label,
                                 "default_on": i.default_on, "atk": i.atk,
                                 "dmg": i.dmg, "armor": i.armor}, i.lead, i.trail)
                 for i in story.items],
        "flags": [_with_comment(_flag_json(f), f.lead, f.trail)
                 for f in story.flags],
        "intro": list(story.intro),
        "ui": dict(story.ui),
        "chapters": list(story.chapters),
        "sections": [_section_to_json(s) for s in story.sections],
        # commentaires des directives scalaires (@title/@author/.../@ui k):
        # cf. l'en-tete du module pour ce qui est couvert ou non.
        "comments": {k: dict(v) for k, v in story.directive_comments.items()},
    }


def adv_to_json(text: str) -> dict:
    """Parse un source .adv et le convertit en dict JSON."""
    return story_to_dict(parse(text))


# --- dict JSON -> .adv --------------------------------------------------

def _json_to_atom(a: dict) -> Atom:
    atom = Atom(op=a["op"], name=a["name"])
    if a["op"] == "stat":
        atom.cmp = CMP_FROM_TEXT[a["cmp"]]
        atom.value = a.get("value", 0)
    return atom


def _json_to_cond(d: dict | None) -> Condition:
    cond = Condition()
    if not d:
        return cond
    cond.src = d.get("src", "")
    if "clauses" in d:
        cond.clauses = [[_json_to_atom(a) for a in c] for c in d["clauses"]]
    else:
        atoms = [_json_to_atom(a) for a in d.get("atoms", [])]
        if d.get("connective") == "or":
            cond.clauses = [[a] for a in atoms]
        elif atoms:
            cond.clauses = [atoms]
    return cond


def _json_to_effect(d: dict) -> Effect:
    e = Effect(op=d["op"], name=d.get("name", ""), value=d.get("value", 0))
    e.cond = _json_to_cond(d.get("cond"))
    e.trail = d.get("trail", "")
    return e


def _json_to_text(d: dict) -> TextSegment:
    return TextSegment(text=d["text"], cond=_json_to_cond(d.get("cond")),
                        style=d.get("style", 0))


def _json_to_choice(d: dict) -> Choice:
    return Choice(label=d["label"], target=d["target"],
                  cond=_json_to_cond(d.get("cond")),
                  effects=[_json_to_effect(e) for e in d.get("effects", [])],
                  lead=list(d.get("lead", [])), trail=d.get("trail", ""))


def _json_to_combat(d: dict) -> Combat:
    c = Combat(name=d["name"], att=d.get("att", 0), hp=d.get("hp", 1),
              dmg=d.get("dmg", 2), armor=d.get("armor", 0),
              image=d.get("image"))
    win, lose = d["win"], d["lose"]
    c.win, c.win_msg = win["target"], win.get("msg", "")
    c.win_effects = [_json_to_effect(e) for e in win.get("effects", [])]
    c.lose, c.lose_msg = lose["target"], lose.get("msg", "")
    c.lose_effects = [_json_to_effect(e) for e in lose.get("effects", [])]
    flee = d.get("flee")
    if flee:
        c.flee, c.flee_msg = flee["target"], flee.get("msg", "")
        c.flee_effects = [_json_to_effect(e) for e in flee.get("effects", [])]
    return c


def _json_to_input(d: dict) -> Input:
    i = Input(prompt=d["prompt"], maxlen=d.get("maxlen", 20),
              answers=list(d.get("answers", [])))
    correct, wrong = d["correct"], d["wrong"]
    i.correct = correct["target"]
    i.correct_effects = [_json_to_effect(e) for e in correct.get("effects", [])]
    i.wrong = wrong["target"]
    i.wrong_effects = [_json_to_effect(e) for e in wrong.get("effects", [])]
    return i


def _json_to_section(d: dict) -> Section:
    s = Section(name=d["name"], chapter=d.get("chapter", 0),
               mode=_TEXT_TO_MODE[d.get("mode", "full_text")],
               image=d.get("image"),
               ending=_TEXT_TO_ENDING[d.get("ending")],
               lead=list(d.get("lead", [])), trail=d.get("trail", ""))
    if d.get("splash"):
        sp = d["splash"]
        s.splash, s.splash_secs = sp["id"], sp.get("secs", 0)
        s.splash_always = sp.get("always", False)
    s.on_enter = [_json_to_effect(e) for e in d.get("on_enter", [])]
    s.on_exit = [_json_to_effect(e) for e in d.get("on_exit", [])]
    s.texts = [_json_to_text(t) for t in d.get("texts", [])]
    s.choices = [_json_to_choice(c) for c in d.get("choices", [])]
    if d.get("combat"):
        s.combat = _json_to_combat(d["combat"])
    if d.get("input"):
        s.input = _json_to_input(d["input"])
    return s


def dict_to_story(d: dict) -> Story:
    """Reconstruit un modele Story a partir d'un dict JSON (inverse de `story_to_dict`)."""
    story = Story(
        title=d.get("title", ""), version=d.get("version", ""),
        author=d.get("author", ""), description=d.get("description", ""),
        license=d.get("license", ""),
        start=d.get("start", ""),
        lang=d.get("lang", "fr"), score_on=d.get("score_on", True),
        moves_on=d.get("moves_on", True),
        combat_attack=d.get("combat_attack", ""),
        combat_hp=d.get("combat_hp", ""),
        combat_base_dmg=d.get("combat_basedmg", 2),
    )
    story.stats = [StatDecl(s["name"], s["init"], s.get("min", 0),
                            s.get("max", 255), hidden=s.get("hidden", False),
                            lead=list(s.get("lead", [])), trail=s.get("trail", ""))
                  for s in d.get("stats", [])]
    story.items = [ItemDecl(i["name"], i.get("label", i["name"]),
                            i.get("default_on", False), atk=i.get("atk", 0),
                            dmg=i.get("dmg", 0), armor=i.get("armor", 0),
                            lead=list(i.get("lead", [])), trail=i.get("trail", ""))
                  for i in d.get("items", [])]
    story.flags = [FlagDecl(f["name"], f.get("default_on", False),
                            is_local=f.get("local", False),
                            chapter=f.get("chapter", 0) if f.get("local") else 0,
                            base=f["name"],
                            lead=list(f.get("lead", [])), trail=f.get("trail", ""))
                  for f in d.get("flags", [])]
    story.intro = list(d.get("intro", []))
    story.ui = dict(d.get("ui", {}))
    story.chapters = list(d.get("chapters", [""]))
    story.sections = [_json_to_section(s) for s in d.get("sections", [])]
    story.directive_comments = {k: dict(v) for k, v in d.get("comments", {}).items()}
    return story


# --- rendu du modele en texte .adv --------------------------------------

# Premiers caracteres qui, en debut de ligne narrative, seraient interpretes
# comme une autre construction du DSL (commentaire/section/directive/effet/
# choix/condition) au lieu de texte litteral. On echappe en prefixant une
# condition vide "{}" : le parser la reconnait comme "toujours vraie" et
# bascule sur la branche texte sans y perdre le contenu.
def _needs_escape(content: str) -> bool:
    if not content:
        return False
    if content[0] in "@~#{":
        return True
    if content.startswith("::"):
        return True
    return content == "*" or content.startswith("* ")


def _fmt_atom(a: Atom) -> str:
    if a.op == "flag":
        return f"flag {a.name}"
    if a.op == "not_flag":
        return f"not flag {a.name}"
    if a.op == "has":
        return f"has {a.name}"
    if a.op == "not_has":
        return f"not has {a.name}"
    if a.op == "stat":
        return f"stat {a.name} {CMP_TO_TEXT[a.cmp]} {a.value}"
    raise A2Error(f"atome de condition inconnu: {a.op}")


def _fmt_cond(cond: Condition) -> str:
    if not cond.clauses:
        return ""
    if cond.src:                       # tel qu'ecrit (parentheses, not, else)
        return cond.src
    if len(cond.clauses) == 1:
        return " and ".join(_fmt_atom(a) for a in cond.clauses[0])
    if all(len(c) == 1 for c in cond.clauses):
        return " or ".join(_fmt_atom(c[0]) for c in cond.clauses)
    return " or ".join("(" + " and ".join(_fmt_atom(a) for a in c) + ")"
                       for c in cond.clauses)


def _fmt_effect(e: Effect) -> str:
    guard = f"{{{_fmt_cond(e.cond)}}} " if e.cond.atoms else ""
    if e.op == "set":
        body = f"set {e.name}"
    elif e.op == "setstat":
        body = f"set {e.name} {e.value}"
    elif e.op in ("clear", "toggle", "give", "take", "goto", "sound", "restore"):
        body = f"{e.op} {e.name}"
    elif e.op in ("add", "sub", "setmax"):
        body = f"{e.op} {e.name} {e.value}"
    elif e.op == "score":
        body = f"score {e.value}"
    else:
        raise A2Error(f"effet inconnu: {e.op}")
    trail = f"  # {e.trail}" if e.trail else ""
    return f"~ {guard}{body}{trail}"


def _flag_json(f: FlagDecl) -> dict:
    d: dict[str, Any] = {"name": f.base or f.name, "default_on": f.default_on,
                         "local": f.is_local}
    if f.is_local:
        d["chapter"] = f.chapter
    return d


def _decl_lines(lead: list[str], trail: str, text: str) -> list[str]:
    """Une ligne de directive/declaration, avec son eventuel commentaire :
    lead en lignes '#' au-dessus, trail ajoute en fin de la ligne elle-meme."""
    out = [f"# {l}" for l in lead]
    out.append(text + (f"  # {trail}" if trail else ""))
    return out


def _fmt_text_line(t: TextSegment) -> str:
    prefix = ""
    if t.style & STYLE_CENTER:
        prefix += "="
    if t.style & STYLE_INVERSE:
        prefix += "!"
    if prefix:
        prefix += " "
    if t.cond.atoms:
        return f"{{{_fmt_cond(t.cond)}}} {prefix}{t.text}"
    content = t.text
    if not prefix and _needs_escape(content):
        content = "{}" + content
    return f"{prefix}{content}"


def _fmt_choice(c: Choice) -> list[str]:
    cond = f"{{{_fmt_cond(c.cond)}}} " if c.cond.atoms else ""
    head = _decl_lines(c.lead, c.trail, f"* {cond}[{c.label}] -> {c.target}")
    return head + [_fmt_effect(e) for e in c.effects]


def _fmt_combat_header(c: Combat) -> str:
    parts = [f'@combat "{c.name}"', f"att={c.att}", f"hp={c.hp}",
             f"dmg={c.dmg}", f"armor={c.armor}"]
    if c.image:
        parts.append(f"image={c.image}")
    return " ".join(parts)


def _fmt_outcome(key: str, target: str, msg: str, effects: list[Effect]) -> list[str]:
    head = f"{key} {target}" + (f' "{msg}"' if msg else "")
    return [head] + [_fmt_effect(e) for e in effects]


def _fmt_ask_header(i: Input) -> str:
    parts = [f'@ask "{i.prompt}"']
    if i.maxlen != 20:
        parts.append(f"maxlen={i.maxlen}")
    return " ".join(parts)


def _fmt_section(s: Section) -> list[str]:
    lines = _decl_lines(s.lead, s.trail, f"::{s.name}")
    if s.mode != Mode.FULL_TEXT:
        lines.append(f"@mode {_MODE_TO_TEXT[s.mode]}")
    if s.image:
        lines.append(f"@image {s.image}")
    if s.splash:
        lines.append(f"@splash {s.splash}" + (f" {s.splash_secs}" if s.splash_secs else "")
                     + (" always" if s.splash_always else ""))
    if s.ending != Ending.NONE:
        lines.append(f"@ending {_ENDING_TO_TEXT[s.ending]}")
    if s.combat is not None:
        lines.append(_fmt_combat_header(s.combat))
    # les '~' qui suivent restent rattachees a on_enter tant qu'aucun bloc
    # d'issue (@win/@lose/@flee/@correct/@wrong) n'a change l'attache.
    lines += [_fmt_effect(e) for e in s.on_enter]
    if s.combat is not None:
        c = s.combat
        lines += _fmt_outcome("@win", c.win, c.win_msg, c.win_effects)
        lines += _fmt_outcome("@lose", c.lose, c.lose_msg, c.lose_effects)
        if c.flee:
            lines += _fmt_outcome("@flee", c.flee, c.flee_msg, c.flee_effects)
    if s.input is not None:
        i = s.input
        lines.append(_fmt_ask_header(i))
        lines += [f'@answer "{ans}"' for ans in i.answers]
        lines += _fmt_outcome("@correct", i.correct, "", i.correct_effects)
        lines += _fmt_outcome("@wrong", i.wrong, "", i.wrong_effects)
    # une ligne vide entre chaque texte : le modele traite deja chaque
    # TextSegment comme un paragraphe distinct, alors qu'a la reanalyse deux
    # lignes "plates" consecutives (pas de condition/style) sans separateur
    # seraient rejointes en un seul paragraphe.
    for t in s.texts:
        lines.append("")
        lines.append(_fmt_text_line(t))
    if s.on_exit:
        lines.append("@on_exit")
        lines += [_fmt_effect(e) for e in s.on_exit]
    for c in s.choices:
        lines += _fmt_choice(c)
    return lines


def render_adv(story: Story) -> str:
    """Reconstruit un texte .adv a partir d'un modele Story.

    Le resultat n'est pas garanti identique octet pour octet a une source
    ecrite a la main (commentaires et mise en forme non conserves), mais
    reste un .adv valide et semantiquement equivalent, compilable tel quel.
    """
    def directive(key: str, text: str) -> list[str]:
        dc = story.directive_comments.get(key, {})
        return _decl_lines(dc.get("lead", []), dc.get("trail", ""), text)

    lines: list[str] = directive("title", f'@title "{story.title}"')
    if story.author:
        lines += directive("author", f'@author "{story.author}"')
    if story.description:
        lines += directive("description", f'@description "{story.description}"')
    if story.license:
        lines += directive("license", f'@license "{story.license}"')
    if story.version:
        lines += directive("version", f'@version "{story.version}"')
    lines += directive("lang", f"@lang {story.lang}")
    if story.start:
        lines += directive("start", f"@start {story.start}")
    for st in story.stats:
        tail = f" {st.lo} {st.hi}" if (st.lo, st.hi) != (0, 255) else ""
        hidden = " hidden" if st.hidden else ""
        lines += _decl_lines(st.lead, st.trail, f"@stat {st.name} {st.init}{tail}{hidden}")
    for it in story.items:
        parts = [f'@item {it.name} "{it.label}"']
        if it.default_on:
            parts.append("on")
        for k, v in (("atk", it.atk), ("dmg", it.dmg), ("armor", it.armor)):
            if v:
                parts.append(f"{k}={v}")
        lines += _decl_lines(it.lead, it.trail, " ".join(parts))
    for fl in story.flags:
        if not fl.is_local:            # les locaux se declarent dans leur chapitre
            parts = [f"@flag {fl.name}"]
            if fl.default_on:
                parts.append("on")
            lines += _decl_lines(fl.lead, fl.trail, " ".join(parts))
    if story.intro:
        lines += directive("intro", f"@intro {' '.join(story.intro)}")
    for key, val in story.ui.items():
        lines += directive(f"ui:{key}", f'@ui {key} "{val}"')
    # value != defaut -> la directive est necessaire ; value == defaut mais
    # un commentaire y est rattache (ex. "@combat_basedmg 2 # ...") -> on
    # l'emet quand meme, sinon le commentaire n'aurait nulle part ou aller.
    if not story.score_on or "score" in story.directive_comments:
        lines += directive("score", "@score " + ("on" if story.score_on else "off"))
    if not story.moves_on or "moves" in story.directive_comments:
        lines += directive("moves", "@moves " + ("on" if story.moves_on else "off"))
    if story.combat_attack:
        lines += directive("combat_attack", f"@combat_attack {story.combat_attack}")
    if story.combat_hp:
        lines += directive("combat_hp", f"@combat_hp {story.combat_hp}")
    if story.combat_base_dmg != 2 or "combat_basedmg" in story.directive_comments:
        lines += directive("combat_basedmg", f"@combat_basedmg {story.combat_base_dmg}")

    def chapter_marker(c: int) -> list[str]:
        title = story.chapters[c] if c < len(story.chapters) else ""
        out = ["", f'@chapter "{title}"']
        for fl in story.flags:
            if fl.is_local and fl.chapter == c:
                out += _decl_lines(fl.lead, fl.trail, f"@flag {fl.base or fl.name} local")
        return out

    last_chapter = 0
    for s in story.sections:
        while last_chapter < s.chapter:
            last_chapter += 1
            lines += chapter_marker(last_chapter)
        lines.append("")
        lines += _fmt_section(s)
    # chapitres sans section, mais qui portent des flags locaux
    for c in range(last_chapter + 1, len(story.chapters)):
        if any(fl.is_local and fl.chapter == c for fl in story.flags):
            lines += chapter_marker(c)

    return "\n".join(lines) + "\n"


def json_to_adv(data: dict) -> str:
    """Reconstruit un texte .adv a partir d'un dict JSON."""
    return render_adv(dict_to_story(data))


# --- .lng <-> JSON, et paquet "global" adv+lang -------------------------

def lang_to_dict(text: str) -> dict:
    """Parse un fichier .lng (socle d'interface) en dict JSON, commentaires
    inclus (meme convention que adv_to_json)."""
    code, ui, comments = parse_lang(text)
    return {"code": code, "ui": ui, "comments": comments}


def dict_to_lang(d: dict) -> str:
    """Inverse de `lang_to_dict` : reconstruit un texte .lng."""
    comments = d.get("comments", {})

    def directive(key: str, text: str) -> list[str]:
        dc = comments.get(key, {})
        return _decl_lines(dc.get("lead", []), dc.get("trail", ""), text)

    lines = directive("lang", f"@lang {d.get('code', 'fr')}")
    lines.append("")
    for key, val in d.get("ui", {}).items():
        lines += directive(f"ui:{key}", f'@ui {key} "{val}"')
    return "\n".join(lines) + "\n"


def bundle_to_dict(adv_text: str, lang_dir: Path = LANG_DIR) -> dict:
    """Convertit une aventure en un JSON "global" qui embarque a la fois le
    modele .adv et le contenu complet du socle .lng qu'elle reference
    (`@lang` de l'aventure). Un seul fichier a lire pour un outil externe,
    au prix d'une duplication du socle entre les aventures qui le partagent
    -- choix assume : ce JSON n'est qu'une vue combinee, `lang/<code>.lng`
    reste la source de verite utilisee par le compilateur."""
    story = parse(adv_text)
    lng_path = lang_dir / f"{story.lang}.lng"
    if not lng_path.exists():
        raise A2Error(f"@lang {story.lang} : fichier de langue introuvable "
                      f"({lng_path})")
    return {
        "adventure": story_to_dict(story),
        "lang": lang_to_dict(lng_path.read_text(encoding="utf-8")),
    }


def dict_to_bundle(data: dict) -> tuple[str, str]:
    """Inverse de `bundle_to_dict` : (texte .adv, texte .lng)."""
    return json_to_adv(data["adventure"]), dict_to_lang(data["lang"])


# --- CLI -----------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="a2c.jsonconv",
        description="Conversion bidirectionnelle .adv <-> JSON "
                    "(sens choisi d'apres l'extension du fichier source)")
    ap.add_argument("source", help="fichier .adv ou .json source")
    ap.add_argument("-o", "--out",
                    help="fichier de sortie (defaut: meme nom, extension opposee)")
    ap.add_argument("--bundle", action="store_true",
                    help="paquet 'global' {adventure, lang} qui embarque aussi "
                        "le contenu du socle .lng reference, au lieu de "
                        "l'aventure seule (sens .adv -> .json uniquement ; "
                        "detecte automatiquement au retour .json -> .adv)")
    args = ap.parse_args(argv)

    src = Path(args.source)
    if not src.exists():
        print(f"a2c.jsonconv: fichier introuvable: {src}", file=sys.stderr)
        return 2

    try:
        if src.suffix == ".adv":
            data = (bundle_to_dict(src.read_text(encoding="utf-8")) if args.bundle
                   else adv_to_json(src.read_text(encoding="utf-8")))
            out = Path(args.out) if args.out else src.with_suffix(".json")
            out.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n",
                           encoding="utf-8")
            print(f"a2c.jsonconv: {src} -> {out}")
        elif src.suffix == ".json":
            data = json.loads(src.read_text(encoding="utf-8"))
            out = Path(args.out) if args.out else src.with_suffix(".adv")
            if "adventure" in data:
                # paquet global : le .lng reconstruit est ecrit a cote de
                # l'.adv, jamais directement dans lang/ (un socle partage la
                # entre plusieurs aventures ; ecraser lang/<code>.lng sans
                # le demander explicitement serait une action a l'aveugle).
                adv_text, lang_text = dict_to_bundle(data)
                out.write_text(adv_text, encoding="utf-8")
                lng_out = out.with_suffix(".lng")
                lng_out.write_text(lang_text, encoding="utf-8")
                print(f"a2c.jsonconv: {src} -> {out}, {lng_out}")
            else:
                out.write_text(json_to_adv(data), encoding="utf-8")
                print(f"a2c.jsonconv: {src} -> {out}")
        else:
            print(f"a2c.jsonconv: extension non reconnue (.adv ou .json attendu): {src}",
                 file=sys.stderr)
            return 2
    except A2Error as e:
        print(f"a2c.jsonconv: {src.name}: {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
