# Éditeur a2adv (v0)

Angular 22 (signaux, composants autonomes) + Angular Material, en style
tableau de bord : barre supérieure, panneau latéral permanent, cards,
boîtes de dialogue Material pour toute confirmation/saisie (plus de
`confirm()`/`prompt()` natifs). Importe un fichier `.adv`, édite le texte
de chaque section avec coloration syntaxique, permet d'ajouter/retirer
chapitres, sections, caractéristiques, drapeaux et objets, et exporte à
nouveau un `.adv`.

## Organisation du code

```
src/app/
  core/            logique pure, sans UI (adv-parser.ts, adv-language.ts)
  components/      un sous-dossier par composant (ts + html + scss)
  app.ts/html/scss composant racine
```

Deux vues, dans le panneau principal (le panneau latéral — la hiérarchie —
reste visible dans les deux) :

- **Éditeur** — hiérarchie Aventure → Chapitre → Section (suit les
  `@chapter` du source), avec une icône par genre de section (départ, fin
  victoire/défaite, combat, énigme, image, scène d'intro). Un vrai arbre,
  sans cycle, repliable chapitre par chapitre.
- **Graphe** — vue d'ensemble en lecture seule, cette fois-ci sur les
  **choix** (`->`) plutôt que les chapitres : utile pour voir comment les
  sections s'enchaînent réellement. Cliquer une section y bascule dans
  l'onglet Éditeur.

## Ce que c'est — et ce que ce n'est pas

Volontairement **simpliste** : ce n'est pas un compilateur, ni un
remplaçant de `a2c`. Le fichier est découpé en sections et en choix
(pour tracer les deux vues et permettre l'export), tout le reste — texte,
effets, conditions, directives — reste du **texte brut**, réinjecté tel
quel. Sans édition, un aller-retour import → export redonne le fichier
**identique à l'octet près** (vérifié sur les cinq aventures du dépôt).

Compiler et valider une aventure reste le travail de `a2c` /
`a2c.analyze` (`compiler/`) — cet éditeur ne fait aucune vérification de
fond (sections mortes, bornes de stats, objets jamais donnés...).

## Pourquoi une hiérarchie plutôt qu'un simple fichier texte

Sur une grosse aventure (`homme_costume_blanc.adv` : 88 sections, 114
choix, 4 chapitres, 33 drapeaux), suivre le fichier en le faisant défiler
devient vite pénible. La hiérarchie Aventure/Chapitre/Section donne une
vue d'ensemble immédiate ; le graphe des choix (onglet séparé) reste
disponible quand on a besoin de suivre un enchaînement précis plutôt
qu'une vue d'ensemble.

## Construire, pas seulement lire

Ajout/suppression de chapitres, sections, caractéristiques, drapeaux et
objets — pas seulement l'édition du texte d'une section existante. Un
principe simple pour rester fiable : ces mutations ne décalent jamais un
index à la main, elles modifient les lignes brutes puis **ré-analysent le
document en entier** (`AdvParser.parse`). Volontairement plus coûteux
qu'un décalage incrémental, mais imperméable à la classe de bug qu'on a
trouvée en le construisant : une section qui « avalait » la frontière de
chapitre suivante dans son propre intervalle de lignes tant qu'aucune
nouvelle section n'apparaissait — un ajout aurait pu atterrir dans le
mauvais chapitre, une suppression aurait pu emporter une frontière avec
elle, en silence. Corrigé à la source ; testé (ajout puis suppression
immédiats redonnent le texte identique à l'octet près, sur les cinq
aventures du dépôt).

## Éditeur de texte : CodeMirror, pas un `<textarea>`

Coloration syntaxique dédiée au `.adv` (`adv-language.ts`) : sections,
directives, choix, effets, conditions `{...}` et mise en relief `*mot*`
ressortent visuellement — sur le même principe de classification que le
format lui-même (premier caractère non blanc de la ligne). Approximatif
par nature, pas une validation.

## Lancer en local

```bash
npm install
npx @angular/cli serve
```

Nécessite Node ≥ 22.22 (Angular 22) — `nvm install 22` si besoin.

## Prochaines étapes envisagées (pas encore faites)

- Compiler réellement via `a2c` **dans le navigateur** (Pyodide), pour
  valider et pas seulement visualiser.
- Rejouer l'aventure compilée dans l'éditeur (le cœur portable du player
  — `story.c`/`state.c`/`combat.c` — se prête à une compilation
  Emscripten vers WebAssembly, sans rien réécrire).
- Auto-sauvegarde locale (IndexedDB) du document en cours d'édition.
- Éditer un choix (cible, condition) directement depuis la hiérarchie ou
  le graphe, sans repasser par le texte brut.
- Tables du préambule (stats/drapeaux/objets) en `mat-form-field` plutôt
  qu'en `<input>` stylés à la main — pour l'instant volontairement compact
  (une vraie grille Material aurait pris trop de hauteur par ligne).

Aucune dépendance serveur n'est nécessaire ni prévue : tout tourne dans
le navigateur, déployable comme n'importe quelle page statique.
