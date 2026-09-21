"""Modèles de sections : `@template` / `@use`.

Un modèle est un bloc de lignes `.adv` paramétré, instancié à la demande :

    @template carnet(ret)
    :: carnet_@ret
    Le carnet est ouvert.
    * [Fermer le carnet] -> @ret
    @end

    @use carnet(carrefour_ch4)

L'expansion est purement textuelle et se fait avant l'analyse : le compilateur
ne voit que des lignes ordinaires, à la place de l'`@use`. Un `@nom` du corps,
où `nom` est un paramètre, est remplacé par l'argument ; `@{nom}` délimite le
nom quand du texte lui est collé (`sec_@{ret}_2`). Tout autre `@mot` reste tel
quel (c'est une directive).

Les erreurs et avertissements d'une ligne issue d'un modèle sont rapportés à
la ligne de l'`@use` racine.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

from .errors import A2Error

_ID = r"[A-Za-z_][A-Za-z0-9_]*"
_REF = re.compile(r"@\{(%s)\}|@(%s)" % (_ID, _ID))
_MAX_DEPTH = 8

# Directives du format : un paramètre ne peut pas porter leur nom, sinon
# `@chapter` ou `@image` du corps serait remplacé.
RESERVED = frozenset("""
    title version author description license start stat item flag intro ui lang score moves
    combat_attack combat_hp combat_basedmg combat win lose flee ask answer
    correct wrong mode image splash ending on_enter on_exit chapter
    template use end
""".split())


@dataclass
class Template:
    name: str
    params: list[str]
    body: list[tuple[int, str]]      # (ligne source, texte)
    line: int
    used: set[str]                   # paramètres réellement référencés


def _directive(raw: str) -> str | None:
    """Nom de la directive (`@xxx`) d'une ligne, sans l'arobase, sinon None."""
    s = raw.strip()
    if not s.startswith("@"):
        return None
    m = re.match(r"@(%s)" % _ID, s)
    return m.group(1) if m else None


def _split_call(s: str, n: int, what: str) -> tuple[str, list[str] | None]:
    """Découpe `nom(a, b)` (ou `nom`) en (nom, arguments). Les virgules et
    parenthèses entre guillemets ne séparent pas. Un `# commentaire` final
    est ignoré. Les guillemets d'un argument sont retirés."""
    m = re.match(r"\s*(%s)\s*" % _ID, s)
    if not m:
        raise A2Error(f"{what} : nom attendu", n)
    name = m.group(1)
    rest = s[m.end():]
    if not rest or rest.startswith("#"):
        return name, None
    if rest[0] != "(":
        raise A2Error(f"{what} : '(' attendu après le nom '{name}'", n)
    args: list[str] = []
    cur: list[tuple[str, bool]] = []     # (caractere, entre guillemets)
    quoted = False
    seen = False                         # argument commence (guillemets compris)

    def flush() -> str:
        """Argument courant, blancs de bord retires hors guillemets."""
        chars = list(cur)
        while chars and chars[0][0] in " \t" and not chars[0][1]:
            chars.pop(0)
        while chars and chars[-1][0] in " \t" and not chars[-1][1]:
            chars.pop()
        cur.clear()
        return "".join(c for c, _ in chars)

    i = 1
    while i < len(rest):
        c = rest[i]
        if c == '"':
            quoted = not quoted
            seen = True
        elif not quoted and c == ",":
            args.append(flush())
            seen = False
        elif not quoted and c == ")":
            tail = rest[i + 1:].strip()
            if tail and not tail.startswith("#"):
                raise A2Error(f"{what} : texte inattendu après ')' : '{tail}'", n)
            last = flush()
            if last or args or seen:
                args.append(last)
            return name, args
        else:
            cur.append((c, quoted))
        i += 1
    raise A2Error(f"{what} : ')' manquante" if not quoted
                  else f"{what} : guillemet non fermé", n)


def _collect(lines: list[str]) -> tuple[dict[str, Template], list[tuple[int, str]]]:
    """Passe 1 : extrait les définitions, renvoie les lignes restantes."""
    templates: dict[str, Template] = {}
    rest: list[tuple[int, str]] = []
    cur: Template | None = None
    for n, raw in enumerate(lines, start=1):
        d = _directive(raw)
        if d == "template":
            if cur is not None:
                raise A2Error(f"@template imbriqué dans '{cur.name}' "
                              f"(ligne {cur.line})", n)
            head = raw.strip()[len("@template"):]
            name, params = _split_call(head, n, "@template")
            params = params or []
            for p in params:
                if not re.fullmatch(_ID, p):
                    raise A2Error(f"@template {name} : nom de paramètre invalide "
                                  f"'{p}'", n)
                if p in RESERVED:
                    raise A2Error(f"@template {name} : le paramètre '{p}' porte "
                                  "le nom d'une directive", n)
            if len(set(params)) != len(params):
                raise A2Error(f"@template {name} : paramètre en double", n)
            if name in templates:
                raise A2Error(f"modèle '{name}' déjà défini "
                              f"(ligne {templates[name].line})", n)
            cur = Template(name, params, [], n, set())
            templates[name] = cur
        elif d == "end":
            if cur is None:
                raise A2Error("@end sans @template", n)
            unused = [p for p in cur.params if p not in cur.used]
            if unused:
                raise A2Error(f"@template {cur.name} : paramètre '{unused[0]}' "
                              "jamais utilisé dans le corps", cur.line)
            cur = None
        elif cur is not None:
            if d == "chapter":
                raise A2Error("@chapter interdit dans un modèle : le chapitre "
                              "est celui de l'@use", n)
            cur.body.append((n, raw))
            for m in _REF.finditer(raw):
                cur.used.add(m.group(1) or m.group(2))
        else:
            rest.append((n, raw))
    if cur is not None:
        raise A2Error(f"@template {cur.name} sans @end", cur.line)
    return templates, rest


def _substitute(raw: str, values: dict[str, str]) -> str:
    def rep(m: re.Match) -> str:
        name = m.group(1) or m.group(2)
        return values.get(name, m.group(0))
    return _REF.sub(rep, raw)


def _expand_use(raw: str, n: int, templates: dict[str, Template],
                stack: list[str]) -> list[tuple[str, str]]:
    """Lignes (texte, note) d'un `@use`. `n` est la ligne racine."""
    name, args = _split_call(raw.strip()[len("@use"):], n, "@use")
    t = templates.get(name)
    if t is None:
        raise A2Error(f"@use : modèle inconnu '{name}'", n)
    args = args or []
    if len(args) != len(t.params):
        raise A2Error(f"@use {name} : {len(args)} argument(s) donné(s), "
                      f"{len(t.params)} attendu(s) ({', '.join(t.params) or 'aucun'})", n)
    if name in stack or len(stack) >= _MAX_DEPTH:
        raise A2Error("@use récursif : " + " -> ".join(stack + [name]), n)
    values = dict(zip(t.params, args))
    out: list[tuple[str, str]] = []
    for tline, traw in t.body:
        text = _substitute(traw, values)
        note = f"modèle '{name}', ligne {tline}"
        if _directive(text) == "use":
            for sub, subnote in _expand_use(text, n, templates, stack + [name]):
                out.append((sub, f"{subnote}, via {note}"))
        else:
            out.append((text, note))
    return out


def expand_templates(text: str) -> tuple[list[tuple[int, str]], list[str | None]]:
    """Renvoie (lignes, notes) : chaque ligne est (numéro source, texte) ;
    `notes[i]` est None pour une ligne d'origine, sinon sa provenance dans un
    modèle. Le numéro d'une ligne issue d'un modèle est celui de l'`@use`."""
    lines = text.splitlines()
    if not any(_directive(l) in ("template", "use", "end") for l in lines):
        return list(enumerate(lines, start=1)), [None] * len(lines)

    templates, rest = _collect(lines)
    items: list[tuple[int, str]] = []
    notes: list[str | None] = []
    for n, raw in rest:
        if _directive(raw) == "use":
            for text_, note in _expand_use(raw, n, templates, []):
                items.append((n, text_))
                notes.append(note)
        else:
            items.append((n, raw))
            notes.append(None)
    return items, notes
