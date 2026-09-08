# lang/ — socles de langue de l'interface

Ce dossier contient les fichiers `.lng` : le socle des chaînes fixes de
l'interface du player (menus, écran de combat, sauvegarde, messages
système...), un fichier par langue. Une aventure choisit son socle avec
`@lang <code>` dans son `.adv` (voir [docs/GUIDE-FORMAT-ADV.md, section
`@lang`](../docs/GUIDE-FORMAT-ADV.md)).

## Format

```
@lang fr

@ui menu_new  "COMMENCER"
@ui menu_quit "QUITTER"
...
```

- Une ligne `@lang <code>` en tête : code de 2 lettres minuscules, qui doit
  correspondre au nom du fichier (`fr.lng` → `fr`).
- Une ligne `@ui <clé> "texte"` par chaîne.
- Les 46 clés sont **toutes obligatoires** : un socle incomplet fait échouer
  la compilation (`a2c` liste les clés manquantes) plutôt que de laisser le
  player muet sur l'une d'elles. La liste complète des clés, avec leur valeur
  française par défaut, est documentée dans
  [docs/GUIDE-FORMAT-ADV.md, section `@ui`](../docs/GUIDE-FORMAT-ADV.md).
- Les accents sont acceptés : ils sont transposés en ASCII à la compilation
  (`é` → `e`) ; tout caractère non convertible devient `?`. Restez concis, ces
  chaînes s'affichent sur un écran 40 colonnes.

## Ajouter une langue

1. Copiez `fr.lng` vers `<code>.lng` (ex. `en.lng`), `<code>` = 2 lettres
   minuscules.
2. Remplacez `@lang fr` par `@lang <code>`.
3. Traduisez chaque texte entre guillemets, sans toucher aux clés.
4. Référencez ce code dans une aventure avec `@lang <code>`.

Le compilateur cherche `lang/<code>.lng` à la racine du dépôt (cf.
`compiler/a2c/cli.py`) et le compile en `APP.LNG` sur la disquette.

Pour ne changer qu'une ou deux chaînes sans écrire un socle complet, une
aventure peut aussi surcharger ponctuellement n'importe quelle clé avec `@ui`
directement dans son `.adv`.
