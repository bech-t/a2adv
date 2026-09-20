# Évolutions envisagées

Idées d'amélioration du format `.adv`, du compilateur `a2c` et de la chaîne
d'outils, tirées de l'écriture d'une grande aventure à énigmes (13 chapitres,
plus de 450 sections). Rien ici n'est engagé : c'est une liste classée par
intérêt, avec le problème rencontré, l'idée, et une piste de mise en œuvre.

## Priorité haute

### 1. Compression du texte

**Problème.** L'histoire compilée n'est pas compressée : ~ 163 Ko pour cette
aventure, alors qu'une disquette Apple II n'en laisse qu'environ 90 pour
l'histoire et les images. Seules les images sont compressées (ZX02).

**Idée.** Compresser le texte des sections. Le français se compresse bien
(un facteur 2 à 3).

**Piste.** Substitution par dictionnaire de mots ou de bigrammes, partagé
par aventure et stocké dans `APP.LNG` ou un fichier voisin : le décodage est
simple à écrire en 6502 et garde l'accès direct par section. Plus ambitieux :
ZX02 par section, avec le décompresseur que le player embarque déjà pour les
images.

### 2. Rapport de taille et budget de plateforme

**Problème.** La taille n'apparaît qu'à la fin, au moment de fabriquer la
disquette. On l'a découverte trop tard.

**Idée.** `a2c --budget apple2|atarist|dos` : taille par chapitre, par
section les plus lourdes, total contre la capacité utile de la plateforme,
avec un avertissement dès qu'on dépasse.

### 3. Détecteur de verrous logiques

**Problème.** Un objet ou un drapeau que seule une scène facultative donne
verrouille la suite sans que le compilateur ni le simulateur ne le disent
clairement : le joueur au hasard « boucle » dans un carrefour.

**Idée.** Une analyse de dominance : pour chaque condition qui protège un
choix vers l'avant (`has X`, `flag Y`), vérifier que le `give` ou le `set`
correspondant se trouve sur **tous** les chemins depuis `@start`, ou signaler
« objet possiblement manquant ». Même chose pour les seuils de
caractéristiques, où l'on regarde si une valeur suffisante est atteignable.

### 4. Avertissements de mise en page à la compilation

- Plus de 9 choix pouvant être visibles en même temps dans une section
  (aujourd'hui vérifié à la main).
- Paragraphe centré plus large que l'écran (il n'est jamais coupé).
- `**` accolés (deux bascules d'inversion qui s'annulent).
- Caractères refusés : la compilation échoue déjà, mais avec un message par
  ligne ; un rapport de **tous** les caractères fautifs d'un coup serait plus
  pratique.

## Priorité moyenne

### 5. Source en plusieurs fichiers

**Problème.** Une aventure de 6000 lignes tient dans un seul fichier, ce qui
la rend pénible à relire et à faire travailler à plusieurs.

**Idée.** `@include chapitre_03.adv` pour couper la source : une expansion
textuelle, dans le même préprocesseur que `@template` (`a2c/template.py`),
avec les numéros de ligne rapportés au fichier d'origine.

### 6. Retour à la section précédente

**Problème.** Une section utilitaire (carnet, inventaire détaillé, aide) ne
sait pas d'où elle vient : il en faut une copie par carrefour.

**Idée.** `-> @back`, la section qui a précédé celle-ci (un seul niveau
suffit).

**Coût mesuré sur l'Apple II** (marge actuelle : 53 octets). La version
minimale, avec `prev` mémorisé dans la boucle de `main` et une cible spéciale
`0xFFFE`, coûte 57 octets : elle ne rentre pas. Sauvegarder `prev` dans
`SAVE0.DAT`, nécessaire pour qu'un chargement dans le carnet ait un retour,
porte le total à environ 102 octets. Le compilateur, l'Atari ST, le DOS et le
web ne posent pas de problème (le code est partagé).

**Ce qui bloque aussi.** Pour remplacer les copies de carnet, il faut des
sections partagées, rattachées à aucun chapitre, et le player remet les locaux
à zéro à chaque changement de fichier : retourner d'un fichier partagé effacerait
les locaux du chapitre. À traiter avec la compression du texte (point 1) ou
tout autre gain de place sur le player.

### 7. `@ask` plus souple

- Réponses comparées sans ponctuation et sans tenir compte de l'ordre
  (`@answer_set 1 2`), au lieu de lister `1 2`, `2 1`, `12`, `21`, `1,2`.
- Indice automatique après N échecs :

```
@hint 2 "Élise : « Pas l'ordre des offrandes. »"
```

- Un nombre d'essais avant une issue différente.

### 8. Séquences et jeux d'états

**Problème.** Un enchaînement où il faut choisir dans l'ordre (trois dalles à
fouler, trois vannes à tourner) prend une section par étape plus une section
d'échec, et des drapeaux pour les états.

**Idée.** Une directive `@sequence a b c` qui compare les choix successifs du
joueur à la bonne suite, avec un `@wrong` unique, sans section intermédiaire.

## Priorité basse

### 9. Plus de caractéristiques, ou des compteurs légers

Huit caractéristiques, c'est peu pour une aventure de relations : la confiance
de deux personnages, un compte à rebours et un état de santé y suffisent à en
occuper la moitié. Une classe de compteurs masqués plus économes (un octet, pas
d'affichage, pas de plafond) soulagerait le format.

### 10. Simulateur

- `--dump-state` : quand une partie boucle ou se bloque, afficher les
  drapeaux, les objets et les caractéristiques pour trouver la porte fermée.
- Un mode « dirigé » qui préfère les sections jamais vues et évite les
  sections utilitaires (carnet, aide) : sinon elles gonflent le nombre de
  « boucles ».
- Rejouer un parcours scripté (suite de choix et de réponses) : un test de non
  régression pour le chemin gagnant de chaque aventure.

### 11. Export d'aide au contrôle qualité

- La liste de toutes les réponses `@ask` (`--answers`), avec la section.
- Le chemin gagnant minimal (objets et drapeaux nécessaires, dans l'ordre).
- Un graphe des sections (`--graph`) pour visualiser les carrefours et les
  branches, à exporter vers Graphviz ou vers l'éditeur visuel.

### 12. Plusieurs disquettes

Pour les aventures qui ne tiennent pas sur une disquette, une directive de
changement de disque (`@disk 2`) et l'invite associée : c'est la seule
manière de garder une grande aventure sur Apple II sans compression.
