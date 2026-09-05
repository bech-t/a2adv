"""Translittération : texte source (avec accents) -> ASCII sûr pour l'Apple II.

Cf. spec §6.1 : la source `.adv` s'écrit avec les vrais accents ; le compilateur
la ramène à l'ASCII, car le générateur de caractères des machines cibles n'a
**aucun glyphe accentué** — ni en 40, ni en 80 colonnes. Les diacritiques sont
donc toujours retirés (`é` -> `e`).

Reste le choix de la **casse**, et lui dépend de l'écran :

- jeu de caractères **standard** (Apple II, II+, et //e tant que `ALTCHAR` est
  éteint) : pas de minuscules du tout — tout s'affiche en capitales ;
- jeu **alternatif** d'un //e à carte 80 colonnes : les minuscules existent, et
  un texte en casse mixte est nettement plus confortable à lire.

D'où ``upper`` : ``False`` (défaut) conserve la casse du source — c'est le
rendu visé, un //e en 80 colonnes ; ``True`` force les capitales, seul rendu
affichable sur un Apple II à jeu de caractères standard (option ``--majuscules``
de `a2c`).
"""

from __future__ import annotations

import re
import unicodedata

# Ligatures capitales : leur expansion dépend de ce qui suit. `ŒUVRE` donne
# `OEUVRE`, mais `Œuf` doit donner `Oeuf` et non `OEuf`. On regarde donc la
# lettre suivante ; en mode tout-capitales la question ne se pose pas.
_LIGATURES = {"Œ": ("OE", "Oe"), "Æ": ("AE", "Ae")}
_LIGATURE_RE = re.compile("([ŒÆ])(?=(.?))")

# Cas particuliers non résolus par la décomposition Unicode
_SPECIALS = {
    "œ": "oe",
    "æ": "ae",
    "ß": "ss",
    "«": '"', "»": '"',
    "“": '"', "”": '"', "„": '"',
    "‘": "'", "’": "'", "‚": "'",
    "–": "-", "—": "-", "…": "...",
    " ": " ",   # espace insécable
}


def transliterate(text: str, upper: bool = False) -> str:
    """Renvoie une version ASCII de ``text``, sûre pour l'écran Apple II.

    ``upper`` force les capitales ; par défaut la casse du source est
    conservée. Dans les deux cas les accents sont retirés et tout caractère
    non-ASCII restant devient ``?``.
    """
    def _ligature(m: re.Match) -> str:
        caps, mixed = _LIGATURES[m.group(1)]
        return caps if upper or m.group(2).isupper() else mixed

    text = _LIGATURE_RE.sub(_ligature, text)
    for src, dst in _SPECIALS.items():
        text = text.replace(src, dst)
    # décompose puis retire les marques diacritiques (é -> e)
    text = unicodedata.normalize("NFD", text)
    text = "".join(ch for ch in text if unicodedata.category(ch) != "Mn")
    if upper:
        text = text.upper()
    # tout caractère non-ASCII restant devient '?'
    return text.encode("ascii", "replace").decode("ascii")
