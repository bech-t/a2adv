"""Translittération : texte source (avec accents) -> majuscules ASCII Apple II.

Cf. spec §6.1 : la source garde les accents, le rendu Apple II est en majuscules
ASCII. On mappe les ligatures/cas particuliers, on retire les diacritiques, puis
on met en majuscules et on force l'ASCII.
"""

from __future__ import annotations

import unicodedata

# Cas particuliers non résolus par la décomposition Unicode
_SPECIALS = {
    "œ": "oe", "Œ": "OE",
    "æ": "ae", "Æ": "AE",
    "ß": "ss",
    "«": '"', "»": '"',
    "“": '"', "”": '"', "„": '"',
    "‘": "'", "’": "'", "‚": "'",
    "–": "-", "—": "-", "…": "...",
    " ": " ",   # espace insécable
}


def transliterate(text: str) -> str:
    """Renvoie une version majuscule ASCII de ``text`` (sûre pour l'écran Apple II)."""
    for src, dst in _SPECIALS.items():
        text = text.replace(src, dst)
    # décompose puis retire les marques diacritiques (é -> e)
    text = unicodedata.normalize("NFD", text)
    text = "".join(ch for ch in text if unicodedata.category(ch) != "Mn")
    text = text.upper()
    # tout caractère non-ASCII restant devient '?'
    return text.encode("ascii", "replace").decode("ascii")
