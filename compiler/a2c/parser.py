"""Parser du DSL .adv.

Orienté lignes : chaque ligne est classée d'après son premier caractère non-blanc
(`#` commentaire, `::` section, `@` directive, `*` choix, `~` effet, `{...}` texte
conditionnel, sinon texte narratif).
"""

from __future__ import annotations

import re

from .cond import parse_condition
from .errors import A2Error
from .template import expand_templates
from .model import (STYLE_CENTER, STYLE_INVERSE, UI_KEY_SET, Choice, Combat, Condition, Effect, Ending, FlagDecl, Input, ItemDecl, Mode, Section, StatDecl, Story, TextSegment)

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


def split_comment(line: str) -> tuple[str, str | None]:
    """Sépare une ligne structurelle (@, ::, *, ~) de son commentaire de fin
    (`#` hors guillemets), s'il y en a un. Renvoie (code, commentaire)."""
    out, in_str = [], False
    for i, ch in enumerate(line):
        if ch == '"':
            in_str = not in_str
        if ch == "#" and not in_str:
            comment = line[i + 1:]
            if comment.startswith(" "):
                comment = comment[1:]
            return "".join(out).rstrip(), comment
        out.append(ch)
    return "".join(out).rstrip(), None


def _strip_comment(line: str) -> str:
    """Retire un commentaire de fin de ligne (`#`) hors guillemets, pour les
    lignes structurelles (@, ::, *, ~). Les lignes de texte gardent leur `#`."""
    return split_comment(line)[0]


def comment_lead(text: str) -> str:
    """Contenu d'une ligne de commentaire pleine ligne (`# texte` -> `texte`)."""
    body = text[1:]
    return body[1:] if body.startswith(" ") else body


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


class _Cursor:
    """Lignes (numero source, texte) a analyser ; garde l'indice de la ligne
    en cours pour situer une erreur dans son modele."""

    def __init__(self, items: list[tuple[int, str]]):
        self.items = items
        self.i = 0

    def __iter__(self):
        for self.i, item in enumerate(self.items):
            yield item


def parse(text: str) -> Story:
    items, notes = expand_templates(text)
    cursor = _Cursor(items)
    try:
        return _parse_lines(cursor)
    except A2Error as e:
        note = notes[cursor.i] if cursor.i < len(notes) else None
        if note and e.line is not None and e.line == items[cursor.i][0]:
            raise A2Error(f"{e.message} ({note})", e.line) from None
        raise


def _parse_lines(lines: _Cursor) -> Story:
    story = Story()

    cur: Section | None = None       # section courante
    attach: list[Effect] | None = None   # cible des lignes '~' (on_enter ou choix)
    seen_section = False
    pending: dict | None = None      # paragraphe de texte en cours d'accumulation
    chapter = 0                      # chapitre courant (frontiere de decoupage fichier)
    lead: list[str] = []             # commentaires pleine ligne en attente d'attache
    # suites de conditions `{else ...}` : une cellule par liste (textes, choix,
    # et chaque liste d'effets), remise a zero a chaque section.
    chains: dict = {"text": [None], "choice": [None], "fx": {}}

    def flush():
        """Termine le paragraphe courant : les lignes consecutives ont ete
        rejointes (re-justifiees), une ligne vide separe les paragraphes."""
        nonlocal pending
        if pending is not None and cur is not None:
            cur.texts.append(TextSegment(text=pending["text"], cond=pending["cond"],
                                         style=pending["style"], line=pending["line"]))
        pending = None

    for n, raw in lines:
        stripped = raw.strip()
        if not stripped:
            flush()                  # ligne vide = fin de paragraphe
            continue
        first = stripped[0]

        # commentaire pleine ligne : mis en attente, attache au prochain
        # element rencontre (section/choix/declaration/directive). Survit aux
        # lignes vides (les blocs de commentaires sont souvent separes de leur
        # cible par une ligne vide), mais pas a une ligne de texte narratif
        # (pas de point d'attache pour un commentaire avant un paragraphe).
        if first == "#":
            flush()
            lead.append(comment_lead(stripped))
            continue

        # --- section ---------------------------------------------------
        if stripped.startswith("::"):
            flush()
            body, trail = split_comment(stripped)
            name = body[2:].strip()
            if not re.fullmatch(_ID, name):
                raise A2Error(f"nom de section invalide: '{name}'", n)
            cur = Section(name=name, line=n, chapter=chapter,
                         lead=lead, trail=trail or "")
            chains = {"text": [None], "choice": [None], "fx": {}}
            lead = []
            story.sections.append(cur)
            # par defaut, les '~' avant le 1er choix sont des effets d'entree
            # (on_enter implicite) ; @on_enter reste la forme explicite.
            attach = cur.on_enter
            seen_section = True
            continue

        # --- directive -------------------------------------------------
        if first == "@":
            flush()
            body, trail = split_comment(stripped)
            directive_lead, lead = lead, []
            # @chapter : frontiere de decoupage (nouveau fichier au compilateur).
            # Purement compile-time ; les sections suivantes changent de chapitre.
            if body.split(None, 1)[0] == "@chapter":
                chapter += 1
                m = re.search(r'"([^"]*)"', body)
                story.chapters.append(m.group(1) if m else "")
                continue
            _parse_directive(body, n, story, cur, seen_section,
                             set_attach=lambda a: None, chapter=chapter)
            key0 = body.split(None, 1)[0]
            _attach_directive_comment(story, key0[1:], body, directive_lead, trail)
            # @on_enter / @on_exit redirigent les '~' suivants
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
            code, trail = split_comment(stripped)
            m = _CHOICE_RE.match(code)
            if not m:
                raise A2Error("syntaxe de choix invalide "
                              "(attendu: * {cond} [libellé] -> cible)", n)
            cond = parse_condition(m.group("cond"), n, chains["choice"])
            choice = Choice(label=m.group("label").strip(),
                            target=m.group("target"), cond=cond, line=n,
                            lead=lead, trail=trail or "")
            lead = []
            cur.choices.append(choice)
            attach = choice.effects
            continue

        # --- effet -----------------------------------------------------
        if first == "~":
            flush()
            if cur is None or attach is None:
                raise A2Error("effet '~' hors d'une section", n)
            code, trail = split_comment(stripped)
            eff = _parse_effect(code[1:].strip(), n,
                                chains["fx"].setdefault(id(attach), [None]))
            eff.trail = trail or ""
            lead = []   # pas de point d'attache pour un lead sur un effet
            attach.append(eff)
            continue

        # --- texte narratif (éventuellement conditionnel) --------------
        lead = []   # idem : pas de point d'attache pour un lead sur du texte
        if cur is None:
            raise A2Error(f"texte hors d'une section: '{stripped}'", n)
        cond = Condition(line=n)
        content = stripped
        is_cond = False
        if first == "{":
            end = content.find("}")
            if end < 0:
                raise A2Error("condition de texte non fermée (manque '}')", n)
            cond = parse_condition(content[1:end], n, chains["text"])
            content = content[end + 1:].strip()
            is_cond = True
        else:
            chains["text"][0] = None      # un texte sans condition interrompt la suite
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
                     seen_section: bool, set_attach, chapter: int = 0) -> None:
    parts = body.split()
    key = parts[0]
    args = parts[1:]

    # directives de préambule (avant toute section)
    # (@flag local fait exception : il se declare dans son chapitre, cf. _parse_flag)
    if key in ("@title", "@version", "@author", "@description", "@license", "@start",
               "@stat", "@item",
               "@flag", "@intro", "@ui", "@lang", "@score", "@moves",
               "@combat_attack", "@combat_hp", "@combat_basedmg"):
        if seen_section and not (key == "@flag" and args and args[-1] == "local"):
            raise A2Error(f"{key} doit figurer dans le préambule "
                          "(avant la première section)", n)

    if key == "@title":
        story.title = body[len(key):].strip().strip('"')
    elif key == "@version":
        story.version = body[len(key):].strip().strip('"')
    elif key == "@author":
        story.author = body[len(key):].strip().strip('"')
    elif key == "@description":
        story.description = body[len(key):].strip().strip('"')
    elif key == "@license":
        story.license = body[len(key):].strip().strip('"')
    elif key == "@start":
        if len(args) != 1:
            raise A2Error("@start attend un nom de section", n)
        story.start = args[0]
    elif key == "@stat":
        _parse_stat(args, n, story)
    elif key == "@item":
        _parse_item(body, args, n, story)
    elif key == "@flag":
        _parse_flag(args, n, story, chapter)
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
        # Choisit lang/<code>.lng comme socle d'interface.
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
    elif key == "@splash":
        _require_section(cur, n)
        _parse_splash(args, n, cur)
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


# Directives scalaires du preambule : pas de dataclass a elles (juste un champ
# sur Story), donc leurs commentaires vont dans Story.directive_comments.
_SCALAR_DIRECTIVES = {"title", "author", "description", "license", "version", "start", "lang", "score",
                      "moves", "combat_attack", "combat_hp", "combat_basedmg",
                      "intro"}


def _attach_directive_comment(story: Story, bare: str, body: str,
                              lead: list[str], trail: str | None) -> None:
    """Rattache un commentaire capture avant/apres une directive `@...` a
    l'endroit du modele qui lui correspond. Les directives de section
    (@mode/@image/@ending/@combat/@win/.../@on_enter/@on_exit) n'ont pas de
    point d'attache dedie : leur commentaire est perdu, comme avant."""
    if not lead and not trail:
        return
    if bare == "stat":
        story.stats[-1].lead, story.stats[-1].trail = lead, trail or ""
    elif bare == "item":
        story.items[-1].lead, story.items[-1].trail = lead, trail or ""
    elif bare == "flag":
        story.flags[-1].lead, story.flags[-1].trail = lead, trail or ""
    elif bare == "ui":
        parts = body.split(None, 2)
        if len(parts) >= 2:
            story.directive_comments[f"ui:{parts[1]}"] = {"lead": lead, "trail": trail or ""}
    elif bare in _SCALAR_DIRECTIVES:
        story.directive_comments[bare] = {"lead": lead, "trail": trail or ""}


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
        if not 0 <= v <= 65535:
            raise A2Error("@stat: valeurs hors [0,65535]", n)
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
    tail = " ".join(args[1:])          # sans libelle : ce qui suit l'id
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


def parse_lang(text: str) -> tuple[str, dict[str, str], dict]:
    """Lit un fichier de langue `.lng` -> (code, {clé: texte}, commentaires).

    Même famille que le `.adv` mais volontairement minimal : `@lang <code>` et
    une ligne `@ui <clé> "texte"` par chaine. Rien d'autre n'est accepté — un
    fichier de langue n'a pas de sections.

    Le 3e element suit la meme convention que `Story.directive_comments` :
    {"lang": {"lead":[...], "trail":"..."}, "ui:<cle>": {...}, ...}.
    """
    lang = "fr"
    strings: dict[str, str] = {}
    comments: dict[str, dict] = {}
    lead: list[str] = []
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line:
            continue
        if line.startswith("#"):
            lead.append(comment_lead(line))
            continue
        code, trail = split_comment(line)
        parts = code.split()
        if not parts:
            lead = []
            continue
        if parts[0] == "@lang":
            if len(parts) != 2 or not re.fullmatch(r"[a-z]{2}", parts[1]):
                raise A2Error("@lang attend un code de 2 lettres minuscules", n)
            lang = parts[1]
            if lead or trail:
                comments["lang"] = {"lead": lead, "trail": trail or ""}
        elif parts[0] == "@ui":
            if len(parts) < 2 or parts[1] not in UI_KEY_SET:
                raise A2Error(f"@ui: clé inconnue '{parts[1] if len(parts) > 1 else ''}'", n)
            m = re.search(r'"([^"]*)"', code)
            if not m:
                raise A2Error("@ui: texte attendu entre guillemets", n)
            strings[parts[1]] = m.group(1)
            if lead or trail:
                comments[f"ui:{parts[1]}"] = {"lead": lead, "trail": trail or ""}
        else:
            raise A2Error(f"ligne inattendue dans un fichier de langue : "
                          f"'{parts[0]}' (attendu @lang ou @ui)", n)
        lead = []
    return lang, strings, comments


SPLASH_MAX_SECS = 31


def _parse_splash(args: list[str], n: int, sec: Section) -> None:
    # @splash ID [secondes] [always]
    if sec.splash is not None:
        raise A2Error("@splash déclaré deux fois dans la section", n)
    if not args or not re.fullmatch(_ID, args[0]):
        raise A2Error("@splash attend un id d'image : "
                      "@splash ID [secondes] [always]", n)
    secs, always = 0, False
    for tok in args[1:]:
        if tok == "always":
            always = True
        elif tok.isdigit():
            secs = int(tok)
            if not 0 <= secs <= SPLASH_MAX_SECS:
                raise A2Error(f"@splash : durée hors 0..{SPLASH_MAX_SECS} "
                              "secondes (0 = attendre une touche)", n)
        else:
            raise A2Error(f"@splash : '{tok}' inattendu "
                          "(attendus : un nombre de secondes, always)", n)
    sec.splash, sec.splash_secs, sec.splash_always = args[0], secs, always


def _parse_flag(args: list[str], n: int, story: Story, chapter: int = 0) -> None:
    # @flag NOM [on|off]         : global, declare dans le preambule.
    # @flag NOM local            : local, declare DANS un chapitre (apres son
    #                              @chapter) ; sa portee est ce chapitre et il
    #                              y repart a off a chaque entree.
    is_local = False
    if args and args[-1] == "local":
        is_local = True
        args = args[:-1]
        if chapter == 0:
            raise A2Error("@flag local : à déclarer dans un chapitre (après un "
                          "@chapter) ; sa portée est ce chapitre", n)
    if len(args) not in (1, 2):
        raise A2Error("@flag attend: NOM [on|off] [local]", n)
    default = args[1] if len(args) == 2 else None
    default_on = _parse_bool_state(default, n)
    if is_local and default_on:
        raise A2Error(f"@flag {args[0]}: un flag 'local' demarre toujours a off "
                      "(il est remis a 0 a chaque changement de chapitre)", n)
    story.flags.append(FlagDecl(args[0], default_on, line=n, is_local=is_local,
                                chapter=chapter if is_local else 0, base=args[0]))


def _parse_effect(src: str, n: int, chain: list | None = None) -> Effect:
    # garde optionnelle : ~ {condition} effet
    cond = Condition(line=n)
    src = src.strip()
    if src.startswith("{"):
        end = src.find("}")
        if end < 0:
            raise A2Error("condition d'effet non fermée (manque '}')", n)
        cond = parse_condition(src[1:end], n, chain)
        src = src[end + 1:].strip()
    elif chain is not None:
        chain[0] = None
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
            return Effect("setstat", a[0], value=_int(a[1], n, hi=65535), line=n)
        raise A2Error("effet 'set' invalide (set FLAG | set STAT N)", n)
    if verb in ("give", "take"):
        _need1(a, n, verb)
        return Effect(verb, a[0], line=n)
    if verb in ("add", "sub"):
        if len(a) != 2:
            raise A2Error(f"effet '{verb}' attend STAT N", n)
        return Effect(verb, a[0], value=_int(a[1], n, hi=65535), line=n)
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
        return Effect("setmax", a[0], value=_int(a[1], n, hi=65535), line=n)
    raise A2Error(f"effet inconnu: '{verb}'", n)


def _parse_on_off(args: list[str], n: int, key: str) -> bool:
    if len(args) != 1 or args[0] not in ("on", "off"):
        raise A2Error(f"{key} attend 'on' ou 'off'", n)
    return args[0] == "on"


def _need1(a: list[str], n: int, verb: str) -> None:
    if len(a) != 1:
        raise A2Error(f"effet '{verb}' attend un seul argument", n)


def _int(s: str, n: int, hi: int = 255) -> int:
    try:
        v = int(s)
    except ValueError:
        raise A2Error(f"valeur numérique attendue, reçu '{s}'", n)
    if not 0 <= v <= hi:
        raise A2Error(f"valeur hors [0,{hi}]: {v}", n)
    return v
