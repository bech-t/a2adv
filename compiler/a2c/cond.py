"""Conditions : grammaire, puis normalisation en forme disjonctive.

Grammaire (les espaces autour des parentheses sont facultatifs) :

    expr   := terme ('or' terme)*
    terme  := facteur ('and' facteur)*
    facteur:= 'not' facteur | '(' expr ')' | atome
    atome  := flag NOM | has ITEM | stat NOM OP N

`and` et `or` ne se melangent pas dans un meme niveau sans parentheses
(`a and b or c` est refuse : ecrire `(a and b) or c`). `not` s'applique a un
atome ou a un groupe ; une condition commence aussi par `else` (voir
`parse_condition`).

Le resultat est toujours un OU de ET d'atomes (`Condition.clauses`) : c'est ce
que le player sait evaluer, sans pile. Un `not` est resolu a la compilation
(lois de De Morgan, comparaison de stat inversee)."""

from __future__ import annotations

import re
from dataclasses import replace

from .errors import A2Error
from .model import Atom, CMP_FROM_TEXT, Cmp, Condition

# Au-dela, le developpement en OU de ET devient deraisonnable : mieux vaut
# scinder la condition (ou la section) que de gonfler silencieusement les donnees.
MAX_CLAUSES = 16

_NEG_OP = {"flag": "not_flag", "not_flag": "flag", "has": "not_has", "not_has": "has"}
_NEG_CMP = {Cmp.EQ: Cmp.NE, Cmp.NE: Cmp.EQ, Cmp.LT: Cmp.GE, Cmp.GE: Cmp.LT,
            Cmp.LE: Cmp.GT, Cmp.GT: Cmp.LE}


def negate_atom(a: Atom) -> Atom:
    if a.op == "stat":
        return replace(a, cmp=_NEG_CMP[a.cmp])
    return replace(a, op=_NEG_OP[a.op])


# --- analyse : texte -> arbre ------------------------------------------------
# noeuds : ("atom", Atom) | ("not", noeud) | ("and", [noeuds]) | ("or", [noeuds])

class _Parser:
    def __init__(self, toks: list[str], n: int):
        self.toks, self.n, self.i = toks, n, 0

    def peek(self) -> str | None:
        return self.toks[self.i] if self.i < len(self.toks) else None

    def take(self) -> str:
        t = self.peek()
        if t is None:
            raise A2Error("condition incomplète", self.n)
        self.i += 1
        return t

    def whole(self):
        node, _ = self.expr()
        if self.peek() is not None:
            raise A2Error(f"condition : '{self.peek()}' inattendu "
                          "(attendu 'and', 'or' ou la fin)", self.n)
        return node

    def expr(self):
        items, raw_and = [], False
        node, ra = self.term()
        items.append(node)
        raw_and |= ra
        while self.peek() == "or":
            self.take()
            node, ra = self.term()
            items.append(node)
            raw_and |= ra
        if len(items) > 1:
            if raw_and:
                raise A2Error("mélange de 'and' et de 'or' sans parenthèses "
                              "(écrire (a and b) or c)", self.n)
            return ("or", items), False
        return items[0], raw_and

    def term(self):
        items = [self.factor()]
        while self.peek() == "and":
            self.take()
            items.append(self.factor())
        if len(items) > 1:
            return ("and", items), True
        return items[0], False

    def factor(self):
        t = self.take()
        if t == "not":
            return ("not", self.factor())
        if t == "(":
            node, _ = self.expr()
            if self.peek() != ")":
                raise A2Error("parenthèse fermante attendue", self.n)
            self.take()
            return node
        if t == "flag":
            return ("atom", Atom("flag", self.take(), line=self.n))
        if t == "has":
            return ("atom", Atom("has", self.take(), line=self.n))
        if t == "stat":
            name, op, val = self.take(), self.take(), self.take()
            if op not in CMP_FROM_TEXT:
                raise A2Error(f"opérateur de comparaison invalide: '{op}'", self.n)
            try:
                value = int(val)
            except ValueError:
                raise A2Error(f"valeur numérique attendue, reçu '{val}'", self.n)
            if not 0 <= value <= 65535:
                raise A2Error(f"valeur hors [0,65535]: {value}", self.n)
            return ("atom", Atom("stat", name, cmp=CMP_FROM_TEXT[op],
                                 value=value, line=self.n))
        raise A2Error(f"condition : '{t}' inattendu (attendu flag, has, stat, "
                      "not ou une parenthèse)", self.n)


# --- normalisation : arbre -> OU de ET ----------------------------------------

def _dnf(node, neg: bool, n: int) -> list[list[Atom]]:
    kind = node[0]
    if kind == "atom":
        return [[negate_atom(node[1]) if neg else node[1]]]
    if kind == "not":
        return _dnf(node[1], not neg, n)
    subs = [_dnf(item, neg, n) for item in node[1]]
    if (kind == "and") == neg:                    # OU (De Morgan si neg)
        out = [c for s in subs for c in s]
        if len(out) > MAX_CLAUSES:
            raise A2Error(_too_complex(), n)
        return out
    out = [[]]                                    # ET : produit des clauses
    for s in subs:
        out = [c + d for c in out for d in s]
        if len(out) > MAX_CLAUSES:
            raise A2Error(_too_complex(), n)
    return out


def _too_complex() -> str:
    return (f"condition trop complexe (plus de {MAX_CLAUSES} clauses une fois "
            "développée) : la scinder")


def _key(a: Atom):
    return (a.op, a.name, a.cmp, a.value)


def _tidy(clauses: list[list[Atom]], n: int) -> list[list[Atom]]:
    """Retire les doublons d'atomes et de clauses, les clauses qui se
    contredisent (flag X et not flag X) et celles qu'une autre clause absorbe."""
    out, seen = [], set()
    for cl in clauses:
        uniq, keys = [], set()
        for a in cl:
            if _key(a) not in keys:
                keys.add(_key(a))
                uniq.append(replace(a))
        if any((_NEG_OP[a.op], a.name, None, 0) in keys
               for a in uniq if a.op != "stat"):
            continue
        sig = frozenset(keys)
        if sig not in seen:
            seen.add(sig)
            out.append(uniq)
    if not out:
        raise A2Error("condition toujours fausse (elle se contredit)", n)
    # absorption : une clause qui contient tous les atomes d'une autre est
    # inutile (a ou (a et b) = a)
    sigs = [frozenset(_key(a) for a in cl) for cl in out]
    kept = [cl for cl, sig in zip(out, sigs)
            if not any(o < sig for o in sigs)]
    return kept


def _to_clauses(node, n: int) -> list[list[Atom]]:
    return _tidy(_dnf(node, False, n), n)


# --- point d'entree ------------------------------------------------------------

def parse_condition(src: str | None, n: int, chain: list | None = None) -> Condition:
    """Analyse `src` (le contenu d'un `{...}`).

    `else` : la condition commence par `else`, seul ou suivi de `and <expr>`.
    Il designe « aucune des conditions precedentes de la meme suite » (la suite
    est l'enchainement de lignes conditionnelles voisines : textes, choix ou
    effets d'une meme liste). `chain` est une cellule `[noeud]` que l'appelant
    garde pour chaque liste : elle porte ce qui est deja couvert, et se vide
    quand un element sans condition interrompt la suite."""
    toks = re.findall(r"\(|\)|[^\s()]+", src or "")
    if not toks:
        if chain is not None:
            chain[0] = None
        return Condition(line=n)

    p = _Parser(toks, n)
    if toks[0] == "else":
        if chain is None or chain[0] is None:
            raise A2Error("'else' sans condition précédente dans la même liste", n)
        prev = chain[0]
        p.i = 1
        if p.peek() is None:
            node, covered = ("not", prev), None
        elif p.peek() == "and":
            p.i = 2
            rest = p.whole()
            node, covered = ("and", [("not", prev), rest]), ("or", [prev, rest])
        else:
            raise A2Error("après 'else' : rien, ou 'and' suivi d'une condition", n)
    else:
        node = p.whole()
        covered = node
    if chain is not None:
        chain[0] = covered
    return Condition(clauses=_to_clauses(node, n), line=n, src=" ".join(toks)
                     .replace("( ", "(").replace(" )", ")"))
