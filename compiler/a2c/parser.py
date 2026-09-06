"""Parser du DSL .adv (cf. spec §6.1).

Orienté lignes : chaque ligne est classée d'après son premier caractère non-blanc
(`#` commentaire, `::` section, `@` directive, `*` choix, `~` effet, `{...}` texte
conditionnel, sinon texte narratif).
"""

from __future__ import annotations

import re

from .errors import A2Error
from .model import (
    CMP_FROM_TEXT, STYLE_CENTER, STYLE_INVERSE, UI_KEY_SET, Atom, Choice,
    Combat, Condition, Effect, Ending, FlagDecl, Input, ItemDecl, Mode, Section,
    StatDecl, Story, TextSegment,
)

_ID = r"[A-Za-z_][A-Za-z0-9_]*"
_CHOICE_RE = re.compile(
    r"^\*\s*(?:\{(?P<cond>[^}]*)\})?\s*\[(?P<label>[^\]]*)\]\s*->\s*(?P<target>%s)\s*$" % _ID
)
_MODES = {"full_text": Mode.FULL_TEXT, "image_text": Mode.IMAGE_TEXT,
          "full_image": Mode.FULL_IMAGE}
_ENDINGS = {"win": Ending.WIN, "lose": Ending.LOSE}

# Mots-cles francais des premieres versions du format. Le reste du format
# etant en anglais, ils detonnaient ; on les reconnait encore pour rendre
# une erreur qui dit quoi ecrire, plutot qu'un "directive inconnue" sec.
_RENAMED = {"@victoire": "@win", "@defaite": "@lose", "@fuite": "@flee",
            "victoire": "win", "defaite": "lose", "fuite": "flee"}


def _strip_comment(line: str) -> str:
    """Retire un commentaire de fin de ligne (`#`) hors guillemets, pour les
    lignes structurelles (@, ::, *, ~). Les lignes de texte gardent leur `#`."""
    out, in_str = [], False
    for ch in line:
        if ch == '"':
            in_str = not in_str
        if ch == "#" and not in_str:
            break
        out.append(ch)
    return "".join(out).rstrip()


def _parse_style_prefix(content: str) -> tuple[int, str]:
    """Détecte un préfixe de style : suite de '='/'!' SUIVIE d'un espace
    (ex. '= ', '! ', '=! '). Renvoie (style, texte). Sans espace apres, le
    marqueur est du texte litteral (pas de style)."""
    i = 0
    while i < len(content) and content[i] in "=!":
        i += 1
    if i > 0 and i < len(content) and content[i] == " ":
        style = 0
        for ch in content[:i]:
            style |= STYLE_CENTER if ch == "=" else STYLE_INVERSE
        return style, content[i + 1:].lstrip()
    return 0, content


def _parse_bool_state(tok: str | None, line: int) -> bool:
    if tok is None:
        return False
    if tok == "on":
        return True
    if tok == "off":
        return False
    raise A2Error(f"état attendu 'on' ou 'off', reçu '{tok}'", line)


def parse(text: str) -> Story:
    story = Story()
    lines = text.splitlines()

    cur: Section | None = None       # section courante
    attach: list[Effect] | None = None   # cible des lignes '~' (on_enter ou choix)
    seen_section = False
    pending: dict | None = None      # paragraphe de texte en cours d'accumulation
    chapter = 0                      # chapitre courant (frontiere de decoupage fichier)

    def flush():
        """Termine le paragraphe courant : les lignes consecutives ont ete
        rejointes (re-justifiees), une ligne vide separe les paragraphes."""
        nonlocal pending
        if pending is not None and cur is not None:
            cur.texts.append(TextSegment(text=pending["text"], cond=pending["cond"],
                                         style=pending["style"], line=pending["line"]))
        pending = None

    for n, raw in enumerate(lines, start=1):
        stripped = raw.strip()
        if not stripped:
            flush()                  # ligne vide = fin de paragraphe
            continue
        first = stripped[0]

        # commentaire pleine ligne
        if first == "#":
            flush()
            continue

        # --- section ---------------------------------------------------
        if stripped.startswith("::"):
            flush()
            body = _strip_comment(stripped)
            name = body[2:].strip()
            if not re.fullmatch(_ID, name):
                raise A2Error(f"nom de section invalide: '{name}'", n)
            cur = Section(name=name, line=n, chapter=chapter)
            story.sections.append(cur)
            # par defaut, les '~' avant le 1er choix sont des effets d'entree
            # (on_enter implicite) ; @on_enter reste la forme explicite.
            attach = cur.on_enter
            seen_section = True
            continue

        # --- directive -------------------------------------------------
        if first == "@":
            flush()
            body = _strip_comment(stripped)
            # @chapter : frontiere de decoupage (nouveau fichier au compilateur).
            # Purement compile-time ; les sections suivantes changent de chapitre.
            if body.split(None, 1)[0] == "@chapter":
                chapter += 1
                m = re.search(r'"([^"]*)"', body)
                story.chapters.append(m.group(1) if m else "")
                continue
            _parse_directive(body, n, story, cur, seen_section,
                             set_attach=lambda a: None)
            # @on_enter / @on_exit redirigent les '~' suivants
            key0 = body.split(None, 1)[0]
            if key0 == "@on_enter":
                if cur is None:
                    raise A2Error("@on_enter hors d'une section", n)
                attach = cur.on_enter
            elif key0 == "@on_exit":
                if cur is None:
                    raise A2Error("@on_exit hors d'une section", n)
                attach = cur.on_exit
            elif key0 == "@win":
                attach = cur.combat.win_effects
            elif key0 == "@lose":
                attach = cur.combat.lose_effects
            elif key0 == "@flee":
                attach = cur.combat.flee_effects
            elif key0 == "@correct":
                attach = cur.input.correct_effects
            elif key0 == "@wrong":
                attach = cur.input.wrong_effects
            continue

        # --- choix -----------------------------------------------------
        # Un choix s'ecrit "* [libelle] -> cible" ou "* {cond} ..." : toujours
        # une etoile SUIVIE d'un espace. Une ligne de texte commencant par une
        # surbrillance "*mot*" (etoile collee a une lettre) reste du texte.
        if first == "*" and (len(stripped) == 1 or stripped[1] == " "):
            flush()
            if cur is None:
                raise A2Error("choix hors d'une section", n)
            m = _CHOICE_RE.match(_strip_comment(stripped))
            if not m:
                raise A2Error("syntaxe de choix invalide "
                              "(attendu: * {cond} [libellé] -> cible)", n)
            cond = _parse_condition(m.group("cond"), n)
            choice = Choice(label=m.group("label").strip(),
                            target=m.group("target"), cond=cond, line=n)
            cur.choices.append(choice)
            attach = choice.effects
            continue

        # --- effet -----------------------------------------------------
        if first == "~":
            flush()
            if cur is None or attach is None:
                raise A2Error("effet '~' hors d'une section", n)
            attach.append(_parse_effect(_strip_comment(stripped)[1:].strip(), n))
            continue

        # --- texte narratif (éventuellement conditionnel) --------------
        if cur is None:
            raise A2Error(f"texte hors d'une section: '{stripped}'", n)
        cond = Condition(line=n)
        content = stripped
        is_cond = False
        if first == "{":
            end = content.find("}")
            if end < 0:
                raise A2Error("condition de texte non fermée (manque '}')", n)
            cond = _parse_condition(content[1:end], n)
            content = content[end + 1:].strip()
            is_cond = True
        # préfixe de style : suite de '='/'!' suivie d'un espace (ex. "= ", "=! ")
        style, content = _parse_style_prefix(content)
        # Lignes consecutives PLAINES -> meme paragraphe (re-justifie). Une ligne
        # conditionnelle {..} ou stylée forme toujours son propre paragraphe.
        if (pending is not None and not is_cond and style == 0
                and not pending["cond"].atoms and pending["style"] == 0):
            pending["text"] += " " + content
        else:
            flush()
            pending = {"text": content, "cond": cond, "style": style, "line": n}

    flush()
    if not seen_section:
        raise A2Error("aucune section (::) dans le fichier")
    return story


def _parse_directive(body: str, n: int, story: Story, cur: Section | None,
                     seen_section: bool, set_attach) -> None:
    parts = body.split()
    key = parts[0]
    args = parts[1:]

    # directives de préambule (avant toute section)
    if key in ("@title", "@author", "@start", "@stat", "@item", "@flag",
               "@intro", "@ui", "@lang", "@score", "@moves",
               "@combat_attack", "@combat_hp", "@combat_basedmg"):
        if seen_section:
            raise A2Error(f"{key} doit figurer dans le préambule "
                          "(avant la première section)", n)

    if key == "@title":
        story.title = body[len(key):].strip().strip('"')
    elif key == "@author":
        story.author = body[len(key):].strip().strip('"')
    elif key == "@start":
        if len(args) != 1:
            raise A2Error("@start attend un nom de section", n)
        story.start = args[0]
    elif key == "@stat":
        _parse_stat(args, n, story)
    elif key == "@item":
        _parse_item(body, args, n, story)
    elif key == "@flag":
        _parse_flag(args, n, story)
    elif key == "@intro":
        if not args:
            raise A2Error("@intro attend une liste de scènes (noms de sections)", n)
        story.intro.extend(args)
    elif key == "@ui":
        if len(args) < 1 or args[0] not in UI_KEY_SET:
            raise A2Error("@ui attend: <clé connue> \"texte\"", n)
        m = re.search(r'"([^"]*)"', body)
        if not m:
            raise A2Error("@ui: texte attendu entre guillemets", n)
        story.ui[args[0]] = m.group(1)
    elif key == "@lang":
        # Choisit lang/<code>.lng comme socle d'interface (cf. spec §6.1).
        if len(args) != 1 or not re.fullmatch(r"[a-z]{2}", args[0]):
            raise A2Error("@lang attend un code de 2 lettres minuscules "
                          "(ex. @lang fr)", n)
        story.lang = args[0]
    elif key == "@score":
        story.score_on = _parse_on_off(args, n, "@score")
    elif key == "@moves":
        story.moves_on = _parse_on_off(args, n, "@moves")
    elif key == "@combat_attack":
        if len(args) != 1:
            raise A2Error("@combat_attack attend un nom de stat", n)
        story.combat_attack = args[0]
    elif key == "@combat_hp":
        if len(args) != 1:
            raise A2Error("@combat_hp attend un nom de stat", n)
        story.combat_hp = args[0]
    elif key == "@combat_basedmg":
        if len(args) != 1:
            raise A2Error("@combat_basedmg attend un nombre", n)
        story.combat_base_dmg = _int(args[0], n)
    elif key == "@combat":
        _require_section(cur, n)
        cur.combat = _parse_combat(body, n)
    elif key in ("@win", "@lose", "@flee"):
        _require_section(cur, n)
        if cur.combat is None:
            raise A2Error(f"{key} sans @combat dans la section", n)
        # message d'issue optionnel entre guillemets ; cible = 1er mot avant le "
        mq = re.search(r'"([^"]*)"', body)
        msg = mq.group(1) if mq else ""
        head = body[:mq.start()] if mq else body
        targs = head.split()[1:]
        if len(targs) != 1:
            raise A2Error(f"{key} attend un nom de section (+ texte optionnel \"...\")", n)
        if len(msg) > 255:
            raise A2Error(f"{key} : texte d'issue trop long (255 max)", n)
        if key == "@win":
            cur.combat.win = targs[0]; cur.combat.win_msg = msg
        elif key == "@lose":
            cur.combat.lose = targs[0]; cur.combat.lose_msg = msg
        else:
            cur.combat.flee = targs[0]; cur.combat.flee_msg = msg
    elif key == "@ask":
        _require_section(cur, n)
        cur.input = _parse_ask(body, n)
    elif key == "@answer":
        _require_section(cur, n)
        if cur.input is None:
            raise A2Error("@answer sans @ask dans la section", n)
        m = re.search(r'"([^"]*)"', body)
        ans = m.group(1) if m else body[len("@answer"):].strip()
        if not ans:
            raise A2Error("@answer attend une reponse", n)
        cur.input.answers.append(ans)
    elif key in ("@correct", "@wrong"):
        _require_section(cur, n)
        if cur.input is None:
            raise A2Error(f"{key} sans @ask dans la section", n)
        if len(args) != 1:
            raise A2Error(f"{key} attend un nom de section", n)
        if key == "@correct":
            cur.input.correct = args[0]
        else:
            cur.input.wrong = args[0]
    elif key == "@mode":
        _require_section(cur, n)
        if len(args) != 1 or args[0] not in _MODES:
            raise A2Error("@mode attend full_text | image_text | full_image", n)
        cur.mode = _MODES[args[0]]
    elif key == "@image":
        _require_section(cur, n)
        if len(args) != 1:
            raise A2Error("@image attend un id d'image", n)
        cur.image = args[0]
    elif key == "@ending":
        _require_section(cur, n)
        if len(args) != 1 or args[0] not in _ENDINGS:
            if args and args[0] in _RENAMED:
                raise A2Error(f"@ending {args[0]} a ete renomme en "
                              f"@ending {_RENAMED[args[0]]}", n)
            raise A2Error("@ending attend win | lose", n)
        cur.ending = _ENDINGS[args[0]]
    elif key == "@on_enter" or key == "@on_exit":
        _require_section(cur, n)
        # l'attache est réglée par l'appelant
    else:
        if key in _RENAMED:
            raise A2Error(f"{key} a ete renomme en {_RENAMED[key]} : le format "
                          f"est en anglais, ces trois mots-cles y faisaient "
                          f"exception", n)
        raise A2Error(f"directive inconnue: {key}", n)


def _require_section(cur: Section | None, n: int) -> None:
    if cur is None:
        raise A2Error("directive de section hors d'une section", n)


def _parse_stat(args: list[str], n: int, story: Story) -> None:
    # @stat NOM init [min max] [hidden]
    hidden = False
    if args and args[-1] == "hidden":
        hidden = True
        args = args[:-1]
    if len(args) not in (2, 4):
        raise A2Error("@stat attend: NOM init [min max] [hidden]", n)
    name = args[0]
    try:
        init = int(args[1])
        lo, hi = (int(args[2]), int(args[3])) if len(args) == 4 else (0, 255)
    except ValueError:
        raise A2Error("@stat: valeurs numériques attendues", n)
    for v in (init, lo, hi):
        if not 0 <= v <= 255:
            raise A2Error("@stat: valeurs hors [0,255]", n)
    if lo > hi:
        raise A2Error("@stat: min > max", n)
    story.stats.append(StatDecl(name, init, lo, hi, line=n, hidden=hidden))


def _parse_item(body: str, args: list[str], n: int, story: Story) -> None:
    if not args:
        raise A2Error("@item attend un id", n)
    name = args[0]
    # libellé optionnel entre guillemets, puis état on/off optionnel
    label, default = name, None
    m = re.search(r'"([^"]*)"', body)
    if m:
        label = m.group(1)
    tail = body
    if m:
        tail = body[m.end():]
    # tokens restants : on/off + modificateurs de combat atk=/dmg=/armor=
    atk = dmg = armor = 0
    for tok in tail.split():
        if tok in ("on", "off"):
            default = tok
        elif "=" in tok:
            key2, _, val = tok.partition("=")
            try:
                v = int(val, 0)
            except ValueError:
                raise A2Error(f"@item {name}: valeur invalide '{tok}'", n)
            if key2 == "atk":
                atk = v
            elif key2 == "dmg":
                dmg = v
            elif key2 == "armor":
                armor = v
            else:
                raise A2Error(f"@item {name}: attribut inconnu '{key2}' "
                              f"(attendus: atk, dmg, armor)", n)
        else:
            raise A2Error(f"@item {name}: token inattendu '{tok}'", n)
    if not (-128 <= atk <= 127 and -128 <= dmg <= 127 and -128 <= armor <= 127):
        raise A2Error(f"@item {name}: modificateur hors [-128,127]", n)
    story.items.append(ItemDecl(name, label, _parse_bool_state(default, n),
                                line=n, atk=atk, dmg=dmg, armor=armor))


def _parse_combat(body: str, n: int) -> Combat:
    # @combat "Nom" att=N hp=N dmg=N armor=N image=asset
    m = re.search(r'"([^"]*)"', body)
    if not m:
        raise A2Error("@combat attend un nom d'ennemi entre guillemets", n)
    c = Combat(name=m.group(1), line=n)
    for tok in body[m.end():].split():
        if "=" not in tok:
            raise A2Error(f"@combat: token inattendu '{tok}'", n)
        key2, _, val = tok.partition("=")
        if key2 == "image":
            c.image = val
        elif key2 in ("att", "hp", "dmg", "armor"):
            setattr(c, key2, _int(val, n))
        else:
            raise A2Error(f"@combat: attribut inconnu '{key2}' "
                          f"(att, hp, dmg, armor, image)", n)
    return c


def _parse_ask(body: str, n: int) -> Input:
    # @ask "invite" [maxlen=N]
    m = re.search(r'"([^"]*)"', body)
    if not m:
        raise A2Error("@ask attend une invite entre guillemets", n)
    inp = Input(prompt=m.group(1), line=n)
    for tok in body[m.end():].split():
        if tok.startswith("maxlen="):
            inp.maxlen = _int(tok.split("=", 1)[1], n)
        else:
            raise A2Error(f"@ask: token inattendu '{tok}'", n)
    if not (1 <= inp.maxlen <= 40):
        raise A2Error("@ask: maxlen doit etre entre 1 et 40", n)
    return inp


def parse_lang(text: str) -> tuple[str, dict[str, str]]:
    """Lit un fichier de langue `.lng` -> (code, {clé: texte}).

    Même famille que le `.adv` mais volontairement minimal : `@lang <code>` et
    une ligne `@ui <clé> "texte"` par chaine. Rien d'autre n'est accepté — un
    fichier de langue n'a pas de sections.
    """
    lang = "fr"
    strings: dict[str, str] = {}
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if parts[0] == "@lang":
            if len(parts) != 2 or not re.fullmatch(r"[a-z]{2}", parts[1]):
                raise A2Error("@lang attend un code de 2 lettres minuscules", n)
            lang = parts[1]
        elif parts[0] == "@ui":
            if len(parts) < 2 or parts[1] not in UI_KEY_SET:
                raise A2Error(f"@ui: clé inconnue '{parts[1] if len(parts) > 1 else ''}'", n)
            m = re.search(r'"([^"]*)"', line)
            if not m:
                raise A2Error("@ui: texte attendu entre guillemets", n)
            strings[parts[1]] = m.group(1)
        else:
            raise A2Error(f"ligne inattendue dans un fichier de langue : "
                          f"'{parts[0]}' (attendu @lang ou @ui)", n)
    return lang, strings


def _parse_flag(args: list[str], n: int, story: Story) -> None:
    # @flag NOM [on|off] [local]   ('local' : remis a 0 a chaque chapitre)
    is_local = False
    if args and args[-1] == "local":
        is_local = True
        args = args[:-1]
    if len(args) not in (1, 2):
        raise A2Error("@flag attend: NOM [on|off] [local]", n)
    default = args[1] if len(args) == 2 else None
    default_on = _parse_bool_state(default, n)
    if is_local and default_on:
        raise A2Error(f"@flag {args[0]}: un flag 'local' demarre toujours a off "
                      "(il est remis a 0 a chaque changement de chapitre)", n)
    story.flags.append(FlagDecl(args[0], default_on, line=n, is_local=is_local))


def _parse_condition(src: str | None, n: int) -> Condition:
    cond = Condition(line=n)
    if not src or not src.strip():
        return cond
    toks = src.split()
    i, connective, seen_and, seen_or = 0, 0, False, False
    while i < len(toks):
        atom, i = _parse_atom(toks, i, n)
        cond.atoms.append(atom)
        if i < len(toks):
            conn = toks[i]
            if conn == "and":
                seen_and = True
                connective = 0
            elif conn == "or":
                seen_or = True
                connective = 1
            else:
                raise A2Error(f"connecteur attendu 'and'/'or', reçu '{conn}'", n)
            i += 1
            if i >= len(toks):
                raise A2Error("condition incomplète après "
                              f"'{conn}'", n)
    if seen_and and seen_or:
        raise A2Error("mélange 'and'/'or' interdit en v0 "
                      "(pas de parenthèses)", n)
    cond.connective = connective
    return cond


def _parse_atom(toks: list[str], i: int, n: int) -> tuple[Atom, int]:
    t = toks[i]
    if t == "not":
        if i + 2 >= len(toks):
            raise A2Error("condition 'not' incomplète", n)
        kind = toks[i + 1]
        name = toks[i + 2]
        if kind == "flag":
            return Atom("not_flag", name, line=n), i + 3
        if kind == "has":
            return Atom("not_has", name, line=n), i + 3
        raise A2Error(f"'not' suivi de '{kind}' invalide (attendu flag/has)", n)
    if t == "flag":
        _need(toks, i + 1, n, "flag NOM")
        return Atom("flag", toks[i + 1], line=n), i + 2
    if t == "has":
        _need(toks, i + 1, n, "has ITEM")
        return Atom("has", toks[i + 1], line=n), i + 2
    if t == "stat":
        if i + 3 >= len(toks):
            raise A2Error("condition 'stat' incomplète (stat NOM OP N)", n)
        name, op, val = toks[i + 1], toks[i + 2], toks[i + 3]
        if op not in CMP_FROM_TEXT:
            raise A2Error(f"opérateur de comparaison invalide: '{op}'", n)
        try:
            value = int(val)
        except ValueError:
            raise A2Error(f"valeur numérique attendue, reçu '{val}'", n)
        return Atom("stat", name, cmp=CMP_FROM_TEXT[op], value=value, line=n), i + 4
    raise A2Error(f"atome de condition invalide: '{t}'", n)


def _need(toks: list[str], i: int, n: int, what: str) -> None:
    if i >= len(toks):
        raise A2Error(f"condition incomplète (attendu {what})", n)


def _parse_effect(src: str, n: int) -> Effect:
    # garde optionnelle : ~ {condition} effet
    cond = Condition(line=n)
    src = src.strip()
    if src.startswith("{"):
        end = src.find("}")
        if end < 0:
            raise A2Error("condition d'effet non fermée (manque '}')", n)
        cond = _parse_condition(src[1:end], n)
        src = src[end + 1:].strip()
    eff = _parse_effect_body(src, n)
    eff.cond = cond
    return eff


def _parse_effect_body(src: str, n: int) -> Effect:
    toks = src.split()
    if not toks:
        raise A2Error("effet vide", n)
    verb = toks[0]
    a = toks[1:]
    if verb in ("clear", "toggle"):
        _need1(a, n, verb)
        return Effect(verb, a[0], line=n)
    if verb == "set":
        # 'set FLAG' (1 arg) = flag ; 'set STAT N' (2 args) = stat
        if len(a) == 1:
            return Effect("set", a[0], line=n)
        if len(a) == 2:
            return Effect("setstat", a[0], value=_int(a[1], n), line=n)
        raise A2Error("effet 'set' invalide (set FLAG | set STAT N)", n)
    if verb in ("give", "take"):
        _need1(a, n, verb)
        return Effect(verb, a[0], line=n)
    if verb in ("add", "sub"):
        if len(a) != 2:
            raise A2Error(f"effet '{verb}' attend STAT N", n)
        return Effect(verb, a[0], value=_int(a[1], n), line=n)
    if verb == "goto":
        _need1(a, n, verb)
        return Effect("goto", a[0], line=n)
    if verb == "sound":
        _need1(a, n, verb)
        return Effect("sound", a[0], line=n)
    if verb == "score":
        if len(a) != 1:
            raise A2Error("effet 'score' attend N (points a ajouter)", n)
        return Effect("score", "", value=_int(a[0], n), line=n)
    if verb == "restore":
        _need1(a, n, verb)                     # ~ restore STAT (= au max)
        return Effect("restore", a[0], line=n)
    if verb == "setmax":
        if len(a) != 2:
            raise A2Error("effet 'setmax' attend STAT N", n)
        return Effect("setmax", a[0], value=_int(a[1], n), line=n)
    raise A2Error(f"effet inconnu: '{verb}'", n)


def _parse_on_off(args: list[str], n: int, key: str) -> bool:
    if len(args) != 1 or args[0] not in ("on", "off"):
        raise A2Error(f"{key} attend 'on' ou 'off'", n)
    return args[0] == "on"


def _need1(a: list[str], n: int, verb: str) -> None:
    if len(a) != 1:
        raise A2Error(f"effet '{verb}' attend un seul argument", n)


def _int(s: str, n: int) -> int:
    try:
        v = int(s)
    except ValueError:
        raise A2Error(f"valeur numérique attendue, reçu '{s}'", n)
    if not 0 <= v <= 255:
        raise A2Error(f"valeur hors [0,255]: {v}", n)
    return v
