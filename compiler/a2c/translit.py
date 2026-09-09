"""Normalisation du texte source avant encodage.

Le format binaire stocke le texte en Latin-1 (ISO-8859-1) — accents et casse
conservés tels quels dans le source. C'est chaque player (scr.c) qui adapte
à l'affichage selon les capacités de son écran matériel : aucune machine
cible n'a de glyphe accentué, et l'Apple II/II+ n'a même aucune minuscule
(cf. player/apple2/src/scr.c).

``normalize_display`` ne touche donc qu'à ce qu'AUCUNE police cible ne sait
rendre, quelle que soit la machine : ligatures (Œ/œ), ponctuation
"typographique" (guillemets courbes, tirets demi/cadratin, points de
suspension), espace insécable. Les lettres accentuées, elles, passent telles
quelles — elles seront transformées à l'affichage si besoin.

``to_match_key`` reste la version ASCII majuscule sans accent d'origine :
utilisée uniquement pour comparer une réponse tapée au clavier (@ask) à la
réponse attendue, indépendamment de l'affichage — un clavier Apple II ne
produit jamais de caractère accentué, la comparaison doit donc rester en
ASCII pur quelle que soit la machine.
"""

from __future__ import annotations

import re
import unicodedata

# Ligatures capitales : leur expansion dépend de ce qui suit. `ŒUVRE` donne
# `OEUVRE`, mais `Œuf` doit donner `Oeuf` et non `OEuf`. On regarde donc la
# lettre suivante.
_LIGATURES = {"Œ": ("OE", "Oe"), "Æ": ("AE", "Ae")}
_LIGATURE_RE = re.compile("([ŒÆ])(?=(.?))")

# Cas particuliers qu'aucune police cible ne sait rendre, quelle que soit la
# machine (contrairement aux lettres accentuées simples, cf. docstring).
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


def normalize_display(text: str) -> str:
    """Aplati ligatures et ponctuation typographique ; conserve accents et casse."""
    def _ligature(m: re.Match) -> str:
        caps, mixed = _LIGATURES[m.group(1)]
        return caps if m.group(2).isupper() else mixed

    text = _LIGATURE_RE.sub(_ligature, text)
    for src, dst in _SPECIALS.items():
        text = text.replace(src, dst)
    return text


def to_match_key(text: str) -> str:
    """Version ASCII majuscule sans accent, pour comparer une saisie clavier."""
    text = normalize_display(text)
    text = unicodedata.normalize("NFD", text)
    text = "".join(ch for ch in text if unicodedata.category(ch) != "Mn")
    return text.upper().encode("ascii", "replace").decode("ascii")
