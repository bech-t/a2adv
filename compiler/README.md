# a2c — compilateur d'aventures

Compile le DSL `.adv` (cf. `../docs/GUIDE-FORMAT-ADV.md`) vers les données
binaires lues par le player : `STORY.DAT` + `ASSETS.IDX`.

## Utilisation

```bash
# depuis compiler/
python3 -m a2c ../examples/demo.adv -o ../build --summary
```

Produit `../build/STORY.DAT` et `../build/ASSETS.IDX`.

Options :
- `-o, --out DOSSIER` : dossier de sortie (défaut `build`).
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

## Convertir en JSON

```bash
python3 -m a2c.jsonconv ../adventures/demo_simple/demo_simple.adv   # -> demo_simple.json
python3 -m a2c.jsonconv demo_simple.json                            # -> demo_simple.adv
```

Le sens de conversion suit l'extension du fichier source (`-o` pour choisir
la sortie). Utile pour manipuler une aventure depuis un outil externe (un
éditeur, un script) sans réimplémenter le parser ligne-à-ligne. Le JSON
reflète le modèle du parser (mêmes déclarations, textes, choix, effets) ; le
retour vers `.adv` produit un source valide et recompilable, mais pas
forcément identique octet pour octet à l'original (la mise en forme —
indentation, largeur de ligne — n'est pas conservée).

Les commentaires `#` sont conservés là où ils apparaissent réellement dans
les aventures du dépôt : en tête de fichier/déclaration (`@stat`/`@item`/
`@flag`/`@ui`/directives scalaires du préambule), en tête de section et de
choix, et en fin de ligne sur ces mêmes constructions ainsi que sur les
effets (`~`). Un commentaire ailleurs (avant un paragraphe de texte, à
l'intérieur d'un bloc `@combat`/`@ask`) reste perdu.

### Paquet "global" (`--bundle`)

Une aventure `.adv` seule ne dit pas tout : les chaînes d'interface (menu,
combat, etc.) viennent d'un socle `.lng` séparé (`lang/<code>.lng`, partagé
entre plusieurs aventures). `--bundle` produit un JSON unique qui embarque
les deux :

```bash
python3 -m a2c.jsonconv --bundle ../adventures/combat_demo/combat_demo.adv \
    -o combat_demo.bundle.json
# -> {"adventure": {...}, "lang": {"code": "fr", "ui": {...}, "comments": {...}}}

python3 -m a2c.jsonconv combat_demo.bundle.json -o combat_demo.adv
# -> combat_demo.adv + combat_demo.lng (sens .json -> .adv détecté
#    automatiquement à la présence de la clé "adventure")
```

Le retour écrit le `.lng` reconstruit à côté de l'`.adv` (`<sortie>.lng`),
jamais directement dans `lang/` : ce socle est partagé par plusieurs
aventures, l'écraser sans le demander explicitement serait une action à
l'aveugle. À toi de le recopier dans `lang/` si tu veux qu'il devienne le
nouveau socle partagé.

## Compiler pour le player web (`a2c.webjson`)

```bash
python3 -m a2c.webjson ../adventures/combat_demo/combat_demo.adv -o story.json
```

Produit le JSON attendu par `player/web` et `player/webng` (modèle RÉSOLU :
noms → indices, flags locaux triés, réponses `@ask` normalisées — même
contenu que `STORY.DAT`/`APP.LNG`, juste sérialisé en JSON). C'est la
commande lancée par `npm run sync` des deux players web — pas besoin de
l'invoquer à la main sauf pour déboguer.

**Images** : en plus du JSON, copie les images de l'aventure à côté du
fichier de sortie (`<dossier de -o>/img/IMGnn.png`), une par id `@image`,
numérotées dans le même ordre que `IMAGES.MAP`. Source attendue :
`<adv>/img/web/<ID EN MAJUSCULES>.png` — un PNG déjà prêt, à fournir à la
main (même principe que `img/named/<ID>.HGR` pour l'Apple II ou
`img/atarist/<ID>.PI1` pour l'Atari ST : pas de conversion palette/
résolution à faire pour le web, donc pas d'outil `img2web.py`, juste
l'image déposée directement). Une image manquante n'est qu'un avertissement
sur stderr — l'aventure reste jouable en texte sans elle.

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
| `a2c/translit.py`| normalisation du texte : ligatures/typographie aplaties, accents/casse conservés (Latin-1) |
| `a2c/encoder.py` | modèle -> `STORY0.DAT` / `ASSETS.IDX` (little-endian) |
| `a2c/decode.py`  | relecture du binaire (tests + dump) |
| `a2c/analyze.py` | analyse de graphe / QA (reachabilité, culs-de-sac, objets morts) |
| `a2c/jsonconv.py`| conversion bidirectionnelle `.adv` <-> JSON (modèle SOURCE, pour éditer/round-tripper) |
| `a2c/webjson.py` | modèle RÉSOLU -> JSON pour le player web (`player/web`, `player/webng`) + copie des images web (cf. ci-dessous) |
| `a2c/cli.py`     | interface `python -m a2c` |

Aucune dépendance externe (bibliothèque standard uniquement).
