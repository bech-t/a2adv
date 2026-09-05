# a2c — compilateur d'aventures

Compile le DSL `.adv` (cf. `../docs/GUIDE-FORMAT-ADV.md`) vers les données
binaires lues par le player Apple II : `STORY.DAT` + `ASSETS.IDX`.

## Utilisation

```bash
# depuis compiler/
python3 -m a2c ../examples/demo.adv -o ../build --summary
```

Produit `../build/STORY.DAT` et `../build/ASSETS.IDX`.

Options :
- `-o, --out DOSSIER` : dossier de sortie (défaut `build`).
- `--majuscules` : force le texte en capitales (rendu d'origine, affichable
  sur tout Apple II). Par défaut la casse du source est conservée, ce qui
  suppose un //e ou plus récent. Cf. `../docs/GUIDE-FORMAT-ADV.md` §2.
- `--max-file OCTETS` : taille max d'un `STORYnn.DAT`, pour forcer le
  sous-découpage et tester le multi-fichiers.
- `--summary` : résumé de l'aventure compilée.
- `--version`.

## Inspecter un STORY0.DAT

```bash
python3 -m a2c.decode ../build/STORY0.DAT
```

Dump lisible (sections, textes, choix, effets résolus en indices) — sert aussi de
référence pour l'implémentation du player 6502.

## Analyser une aventure (QA)

```bash
python3 -m a2c.analyze ../examples/orbe_de_sortis.adv
```

Détecte les défauts de conception : sections inatteignables, culs-de-sac, fins
non joignables, objets requis jamais octroyés, flags/objets morts. Reachabilité
optimiste (conditions ignorées).

## Tests

```bash
python3 tests/test_compile.py       # ou: pytest
```

## Architecture

| module | rôle |
|--------|------|
| `a2c/parser.py`  | DSL `.adv` -> modèle (orienté lignes) |
| `a2c/model.py`   | dataclasses + constantes du format (opcodes) |
| `a2c/symbols.py` | validation + résolution des noms en indices |
| `a2c/translit.py`| accents -> majuscules ASCII Apple II |
| `a2c/encoder.py` | modèle -> `STORY0.DAT` / `ASSETS.IDX` (little-endian) |
| `a2c/decode.py`  | relecture du binaire (tests + dump) |
| `a2c/analyze.py` | analyse de graphe / QA (reachabilité, culs-de-sac, objets morts) |
| `a2c/cli.py`     | interface `python -m a2c` |

Aucune dépendance externe (bibliothèque standard uniquement).
