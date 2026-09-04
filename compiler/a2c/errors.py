"""Erreurs de compilation, avec numéro de ligne source."""

from __future__ import annotations


class A2Error(Exception):
    """Erreur de compilation rapportée à l'auteur (avec ligne si connue)."""

    def __init__(self, message: str, line: int | None = None):
        self.message = message
        self.line = line
        super().__init__(self.__str__())

    def __str__(self) -> str:
        if self.line is not None:
            return f"ligne {self.line}: {self.message}"
        return self.message
