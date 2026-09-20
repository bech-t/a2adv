# Écrire une aventure `.adv` — techniques et bonnes pratiques

Ce document suppose que vous connaissez déjà la syntaxe du format `.adv`
(sections, directives, conditions, effets — voir [*Le format `.adv` :
référence complète*](GUIDE-FORMAT-ADV.md)). Il ne redéfinit rien : il montre
comment combiner ces éléments pour écrire une aventure qui tient debout, et
les pièges qui reviennent le plus souvent.

---

## Un carrefour qui se vide au fil des visites

Le patron le plus courant : une section à laquelle on revient plusieurs fois,
et dont chaque option ne doit être prise qu'une fois.

```
:: enquete_hub
* {not flag q_temoin} [Interroger le temoin] -> piste_temoin
  ~ set q_temoin
* {not flag q_alibi} [Verifier l'alibi] -> piste_alibi
  ~ set q_alibi
* {flag q_temoin and flag q_alibi} [Conclure] -> conclusion
```

Chaque section de piste se termine par un choix qui revient à `enquete_hub`.
Utilisez un drapeau `local` si ce carrefour n'a de sens que pour la durée d'un
chapitre (une soirée, une enquête) — sinon un drapeau global si l'état doit
survivre au changement de chapitre.

## Récompenser la perspicacité sans dupliquer de texte

```
Un cafe de la 9e Avenue, une table au fond. L'aine pousse une enveloppe.

{flag jean_revanche} « Vous nous avez parle en gens du metier, au musee. On rend la politesse. »
{not flag jean_revanche} L'aine ne sourit qu'avec la bouche. « Ce n'est pas de l'amitie. »
```

Un même passage, deux tons selon ce que le joueur a fait des chapitres plus
tôt — sans section séparée, juste deux lignes conditionnelles qui s'excluent.

## Une récompense qui traverse plusieurs chapitres

Rien n'empêche une condition de combiner des éléments acquis à des moments très
différents de la partie :

```
* {has dague and flag mot_de_passe} [Frapper trois coups, la lame a plat] -> voie_secrete
```

Ici, `dague` peut avoir été ramassée au chapitre 1 et `mot_de_passe` appris au
chapitre 2 : le choix n'apparaît au chapitre 4 que si les deux se sont
combinés. C'est ce qui fait qu'explorer tôt paie tard, sans qu'aucun texte
n'ait besoin de le rappeler explicitement.

## Un faux choix (l'illusion du contrôle)

Toutes les options d'un menu n'ont pas besoin de mener à des résultats
différents. Deux choix qui se lisent différemment peuvent parfaitement pointer
vers la **même** section :

```
* [Sonner et s'annoncer comme un invite en retard] -> alerte
* [L'enfoncer d'un coup d'epaule] -> alerte
```

Utile pour signaler, sans le dire frontalement, qu'une approche « habile » en
apparence ne l'était pas vraiment — surtout si le texte de la section cible
reste neutre sur la méthode employée (« quelque chose, dans cette maison,
savait déjà que vous montiez »).

## Du contenu qui ne sert à rien — exprès

Un carrefour où **chaque** option fait progresser l'histoire ou rapporte des
points se joue comme un jeu de fléchage : le joueur presse la première touche
en boucle sans lire. Mêler quelques options purement décoratives — une
conversation mondaine, un aparté comique, un geste sans conséquence — casse ce
réflexe et rend les vraies options plus lisibles, par contraste :

```
* {not flag q_photographe} [Poser pour les photographes] -> gala_photographe
  ~ set q_photographe
* {not flag q_invites} [Detailler les invites] -> gala_invites
  ~ set q_invites
```

`gala_photographe` ne pose ni score ni drapeau de progression ; elle existe
pour l'ambiance et pour ralentir la lecture. Combinée au faux choix ci-dessus,
elle brise l'habitude qui s'installe le plus vite chez un joueur pressé :
appuyer sur « 1 » sans lire, parce que jusque-là ça a toujours marché.

## Un plafond de caractéristique qui dépend d'un choix initial

```
:: perso_baroudeur
~ setmax SANG_FROID 11
~ set    SANG_FROID  9
```

`setmax` fixe le plafond, `set` la valeur de départ — utile pour plusieurs
personnages jouables dont les jauges maximales diffèrent, sans dupliquer la
déclaration `@stat` (qui ne fixe qu'un seul plafond par défaut pour toute la
partie).

## Éviter le piège de la « aucune issue possible »

Une section dont tous les choix sont conditionnels peut, à l'exécution, se
retrouver sans aucune option valable pour un joueur donné — le moteur affiche
alors `(AUCUNE ISSUE POSSIBLE)` et termine la partie sèchement, sans qu'aucune
compilation n'ait pu le prévoir (voir la référence, §4).

Si un carrefour ne propose que des options qui se consomment, assurez-vous
qu'au moins une reste accessible en toutes circonstances, ou que l'état finit
forcément par permettre l'une d'elles :

```
* {flag adresse_connue} [Monter chez le suspect] -> confrontation
* {not flag adresse_connue and flag q_indice} [Une derniere piste] -> indice_final
  ~ set adresse_connue
```

Sans un filet de ce genre, un joueur malchanceux (ou qui a raté une piste plus
tôt) peut se retrouver face à un carrefour vide de toute option valable — une
fin de partie sèche, sans le mot « fin ».

## Un chemin de secours dans un donjon

Une section verrouillée par une énigme ou un piège peut toujours prévoir une
solution de force, plus coûteuse :

```
* {not flag porte_ouverte} [Chercher le mecanisme] -> enigme_porte
* [Défoncer la porte à la machette] -> porte_forcee
  ~ sub ENDURANCE 4
```

Ça garantit qu'aucun joueur ne reste bloqué devant une énigme qu'il ne résout
pas, tout en réservant la voie « propre » à qui prend le temps de chercher.

## Découper en chapitres au bon endroit

`@chapter` ne coûte rien à l'écriture, mais un découpage généreux aide la
machine (chaque chapitre se précharge séparément). Un bon repère : une
frontière à chaque rupture nette de lieu ou de temps — un nouveau site, une
nouvelle nuit, un changement de personnage aux commandes — plutôt qu'au
milieu d'une séquence qui se lit d'un trait.

## Relire son propre carrefour comme un joueur pressé

Une fois un carrefour écrit, la question à se poser n'est pas « est-ce que
chaque option a un sens ? » mais « qu'est-ce qui se passe si quelqu'un presse
1, encore 1, encore 1, sans lire ? ». Ça révèle à la fois les carrefours trop
généreux (tout gagne, rien ne coûte, voir *contenu qui ne sert à rien*
ci-dessus) et les carrefours trop punitifs (une option en apparence anodine
qui coûte cher sans prévenir). Les deux se corrigent au texte, pas au code :
c'est souvent la formulation du choix qui doit prévenir le joueur de ce qu'il
risque, plutôt que la mécanique qui doit changer.

---

## Aucun verrou : chaque porte a une clé garantie

Toute condition qui débloque la suite (`has X`, `flag Y`, `stat Z >= n`) doit
avoir, sur **tous** les chemins qui y mènent, une source garantie ou un plan B.
Les oublis se cachent presque toujours au même endroit : un objet ou un
drapeau qu'une scène **facultative** est la seule à donner.

```
# Faux : la corde n'existe que si le joueur pense à la demander.
* {not has corde} [Demander de la corde] -> entrepot_corde
* [Embarquer] -> fleuve

# Bon : on n'embarque pas sans elle, et le choix renvoie à l'endroit où l'obtenir.
* {has corde} [Embarquer] -> fleuve
* {not has corde} [Embarquer (il manque de la corde)] -> entrepot
```

Deux vérifications à faire pour chaque porte :

- **Source.** Qui donne l'objet ? Est-ce sur le chemin obligatoire, ou faut-il
  au moins une deuxième source (une trouvaille, un achat, un personnage) ?
- **Jauge de confiance.** Une caractéristique qui ouvre une porte quand elle
  atteint un seuil doit pouvoir remonter *quoi que le joueur ait fait avant*.
  Sinon, prévoir une corvée qui la remonte à coup sûr :

```
* {flag entretien_fait and not flag invitation and stat CONFIANCE < 2} [Demander comment regagner sa confiance] -> corvee
```

Le simulateur (`a2c.simulate`) aide : un joueur au hasard qui « boucle » dans un
même carrefour signale presque toujours une porte sans clé.

## Plusieurs solutions par obstacle

Un bon obstacle se contourne de plusieurs façons, chacune réservée à un joueur
qui a fait quelque chose d'avant : un objet, une connaissance, ou une stat.
Une voie « de force » reste toujours possible, à un coût.

```
* {has amulette} [Montrer l'amulette au garde] -> evasion
* {has herbe} [Lui offrir une tisane] -> evasion
* [Demander l'intervention d'un notable (il faudra lui devoir une faveur)] -> evasion
  ~ sub CONFIANCE 1
```

Ça rend chaque partie différente sans écrire trois scénarios : les trois
branches se rejoignent au même endroit.

## Des énigmes équitables

Une énigme est équitable si le joueur a pu **voir** tout ce qui sert à la
résoudre. Quatre règles :

- Chaque information nécessaire a été affichée au moins une fois, avant.
- Un carnet consultable rappelle les indices, et n'affiche que ceux que le
  joueur a réellement découverts (une ligne conditionnelle par indice).
- Un échec coûte quelque chose (temps, vivres, un point de santé) sans jamais
  bloquer : la même énigme se propose à nouveau.
- Un indice de secours apparaît après un premier échec, par exemple une ligne
  conditionnelle sur un drapeau local posé par la branche `@wrong`.

```
{flag echec_symboles} Élise, doucement : « Ce n'est pas l'ordre des offrandes. C'est celui qu'on nous a lu. »
@ask "Ordre de l'éveil (3 mots) ?" maxlen=30
@answer SOLEIL EAU PIERRE
@correct porte_ouverte
@wrong   porte_fermee    # pose `~ set echec_symboles`
```

Un **leurre** est légitime s'il est annoncé : deux ordres proches (une liste
d'offrandes et une liste d'éveil) sont un bon piège tant qu'un texte les
distingue explicitement quelque part.

Côté `@ask`, la comparaison ignore casse, accents et espaces de bord, mais pas
la ponctuation ni l'ordre : pour une réponse en plusieurs mots ou plusieurs
nombres, lister les variantes plausibles (`1 2`, `2 1`, `12`, `21`, `1,2`).

## Un compte à rebours sans « aucune issue possible »

Une course contre la montre se modélise avec une caractéristique **masquée**
(`hidden`) qui descend à chaque action. Chaque étape doit garder une branche
pour le cas où elle atteint zéro, sinon le joueur épuisé retombe sur
`(AUCUNE ISSUE POSSIBLE)` :

```
* {stat TEMPS >= 1} [Forcer la porte] -> porte_forcee
* {stat TEMPS < 1}  [Monter encore] -> noyade
```

Mieux vaut que le temps ne se perde que par des erreurs : les bonnes solutions
le rendent (`~ set TEMPS 10`), les mauvaises le consomment (`~ sub TEMPS 2`).
La défaite doit rester exceptionnelle et compréhensible.

## Où déclarer ses drapeaux locaux

Un drapeau local se déclare dans son chapitre et n'existe que là : le
compilateur refuse de le lire ou de le poser depuis un autre chapitre, ce qui
supprime les fautes de frappe entre chapitres et les « drapeaux toujours
faux ». Trois habitudes :

- Déclarer les locaux **juste sous le `@chapter`**, avec le sens en
  commentaire :

```
@chapter "Qollpa"
@flag ancien_ecoute local     # Tayta Manuel a parlé
@flag berger_ecoute local
```

- Réutiliser librement les mêmes noms d'un chapitre à l'autre : ce sont des
  drapeaux distincts, et ils partagent les mêmes emplacements (le budget est de
  32 locaux par chapitre).
- Ne jamais y stocker un état qui doit survivre au chapitre (un pont réparé,
  une porte ouverte) : ce sont des drapeaux **globaux**. Et attention aux
  sections partagées : sauter dans une section qui appartient à un autre
  chapitre remet les locaux à zéro au retour.

## Un carnet de bord

Dans une aventure à énigmes, une section « carnet » accessible depuis les
carrefours rend l'enquête jouable. Chaque ligne est conditionnée à un
drapeau posé au moment où l'indice est découvert :

```
:: carnet
{flag mots_base} Signes : triangle = TAN (pierre), disque = RU (soleil).
{flag ordre_symboles} Ordre de l'éveil : soleil, eau, pierre.

* [Fermer le carnet] -> carrefour
```

Comme une section revient toujours à une cible fixe, il faut un carnet par
carrefour (ou par chapitre) : les 1024 octets de la section, et le poids
de chaque copie dans `STORYnn.DAT`, pèsent vite. Ne garder dans chaque copie que
les indices déjà possibles à ce stade.

Un `@template` évite de recopier le texte à la main quand les copies sont
identiques (une aide, un inventaire commenté). Il ne diminue pas la taille du
binaire : chaque instance reste une section à part entière. Pour un carnet qui
se remplit chapitre après chapitre, un modèle unique avec toutes les entrées
alourdirait chaque copie ; mieux vaut un modèle pour le cadre seulement, ou
écrire les copies partielles.

## Les pièges d'affichage

- Un paragraphe **centré** (`=`) n'est jamais coupé : s'il dépasse la largeur
  de l'écran, il enroule sans respecter les mots. Au-delà de 38 caractères,
  utiliser `!` (inversé, justifié) ou un paragraphe normal.
- `**mot**` ne met rien en relief : chaque `*` bascule, deux `*` d'affilée
  s'annulent. Utiliser `*mot*`.
- Seul Latin-1 passe. Les flèches (`→`), les émojis, les guillemets courbes
  sont refusés ou aplatis : n'écrire que du texte, avec `->` si besoin.
- Un carrefour affiche 9 choix au plus : quand il en écrit davantage, vérifier
  que les conditions s'excluent deux à deux.

## Anticiper la taille

Tout le texte est stocké sans compression. Une aventure de plus de 400
sections dépasse vite la place d'une disquette Apple II (environ 80 Ko
disponibles pour l'histoire et les images, une fois ProDOS et le player
installés). Repères :

- ~ 350 octets par section de narration courte, 10 à 15 Ko par chapitre dense.
- Les répétitions coûtent : un carnet recopié à chaque chapitre, des
  variantes conditionnelles proches, des descriptions redites.
- Mesurer souvent : `du -cb build/STORY*.DAT` après chaque chapitre, plutôt qu'à
  la fin.

## Un écran de règles à la fin de l'intro

Le bandeau d'état affiche des chiffres dont le joueur ne connaît pas le sens.
Un dernier écran d'`@intro`, juste avant le début du jeu, lui dit ce que
mesure chaque caractéristique **visible** et ce qui est en jeu. Un seul écran,
dans la voix de l'histoire plutôt qu'en notice :

```
:: intro_regles
@mode full_text
=! COMMENT JOUER

Le bandeau du haut suit Gabriel :

*SANTE* : ses forces. Chutes et fatigue l'entament, le repos la rend.
*SAVOIR* : sa culture. Elle ouvre des observations et des indices.

D'autres mesures ne se lisent sur aucun cadran. Vous les sentirez.

Vous partez chercher un trésor. Vous trouverez peut-être autre chose.
```

Repères :

- **Une ligne par caractéristique visible** : ce qu'elle mesure, et ce qui
  l'entame ou la rend. Ne rien promettre qu'on n'a pas vérifié dans
  l'aventure : « à zéro, la quête s'achève » doit être vrai.
- **Les mesures masquées** (`hidden`) ne se nomment pas : une phrase suffit à
  dire qu'elles existent (le temps, la confiance, la corruption). Le but d'une
  jauge masquée est qu'on la ressente.
- **L'enjeu en dernière ligne** : ce que l'on cherche, ce que l'on risque. C'est
  la phrase qui reste avant la première scène.
- **Court** : une douzaine de lignes de 40 colonnes au plus. Ne pas répéter
  ce que la ligne d'aide du bas (`hints`) dit déjà des touches.
- **Avant un choix de personnage**, s'il y en a un : le joueur doit comprendre
  les jauges avant de décider laquelle privilégier.
- Pas de valeur de départ écrite en dur : les jauges peuvent changer avec le
  personnage. Au besoin, `%NOM%` affiche la valeur courante.

## Des images sans contrainte : `@splash`

`image_text` oblige à écrire la section pour quatre lignes et impose l'image
sur toutes les machines. Pour un beau moment (une arrivée, une révélation),
préférer `@splash` : l'image s'affiche en plein écran **avant** le texte, et la
section reste écrite normalement. Sur une machine qui n'a pas l'image, on lit
le texte, sans rien perdre.

```
:: cite_entree
@splash cite
Une caverne colossale, aussi vaste qu'une cathédrale...
```

Repères :

- Le **texte doit se suffire à lui-même** : l'image illustre, elle ne porte pas
  l'information (un indice d'énigme ne se cache pas dans une image que l'Apple II
  n'aura jamais).
- **Réserver l'image aux grands moments** : un splash à chaque section fatigue,
  et occupe de la place sur disque.
- Sur un carrefour, le splash n'est pas rejoué quand on y revient de scènes
  sans splash ; `always` force le rejeu (à éviter sur un carrefour).
- Fournir l'image des plateformes qui en ont la place (web, ST, DOS), et
  laisser l'Apple II sans image quand la disquette est pleine.

## Des conditions lisibles

- Préférer `{else}` à une paire de conditions inverses : le contraire ne peut
  plus se désynchroniser quand on modifie la première.
- Préférer une parenthèse à deux lignes presque identiques :
  `{(flag a or flag b) and has c}` plutôt que deux lignes de texte.
- Chaque condition non vide coûte quelques octets, et un `or` s'écrit en
  clauses séparées : sur Apple II, mieux vaut peu de conditions simples que
  quelques longues conditions très ramifiées.
- Une condition qui dépasse seize clauses une fois développée est refusée :
  c'est en général le signe qu'il faut la scinder en deux sections ou introduire
  un drapeau intermédiaire (`~ set peut_entrer`).

