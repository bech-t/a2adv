# a2adv

## Écrire un livre-jeu comme on écrit un texte. Le jouer sur une machine de 1979.

a2adv est une chaîne d'outils qui transforme un simple fichier texte en disquette
bootable pour Apple II.

---

## L'idée

Le livre-jeu et l'Apple II sont nés la même année, dans le même monde. Et
pourtant, écrire l'un pour l'autre a toujours été une punition : on tapait du
BASIC, on comptait les octets à la main, et déplacer un paragraphe voulait dire
renuméroter tous les autres.

a2adv prend le problème à l'envers. **Vous écrivez du texte ; la machine s'occupe
des octets.** Votre aventure tient dans un seul fichier, lisible par n'importe
qui — des paragraphes, des choix, des conditions écrites en français. Le reste
est du travail d'outil.

Les contraintes de 1979 sont respectées à la lettre. Simplement, ce n'est plus
vous qui les portez.

---

## Ce que vous écrivez, ce que la machine affiche

Vous tapez ceci :

```
:: chantier_camion
Vous inspectez les caisses : poteries, statuettes, tablettes de glyphes.

{stat PERCEPTION >= 8} Un homme en *costume blanc*, immobile, tourné vers
le chantier.

* [Descendre dans le temple] -> temple
* [Rebrousser chemin] -> chantier
```

Et l'Apple II affiche cela :

```
VOUS INSPECTEZ LES CAISSES : POTERIES,
STATUETTES, TABLETTES DE GLYPHES.

UN HOMME EN COSTUME BLANC, IMMOBILE,
TOURNE VERS LE CHANTIER.

1) DESCENDRE DANS LE TEMPLE
2) REBROUSSER CHEMIN
```

Les accents sont tombés, le texte s'est rejustifié à la largeur de l'écran, le
passage en surbrillance a trouvé sa place, et la phrase conditionnelle
n'apparaît qu'au personnage assez observateur pour la remarquer. Vous n'avez rien
eu à calculer.

---

## Comment ça marche

**1. Vous écrivez.** Un fichier texte. Des sections, des choix, des objets, des
caractéristiques. Aucun code.

**2. L'outil vérifie et compacte.** Il signale les impasses, les passages qu'on
ne peut pas atteindre, les objets exigés mais jamais donnés, les fautes de frappe
— puis réduit tout à des données minuscules.

**3. La disquette sort.** Une image bootable, à glisser dans un émulateur ou dans
un vrai lecteur.

---

## Le luxe dans 64 kilo-octets

Le moteur ne charge jamais l'histoire entière : il va chercher chaque paragraphe
au moment où vous l'atteignez. La taille de l'aventure n'est donc pas limitée par
la mémoire de la machine, mais par la place sur la disquette.

Il s'adapte aussi à ce qu'il trouve sous lui. Écran 80 colonnes, carte son,
mémoire supplémentaire : s'ils sont là, il s'en sert ; sinon il fait sans, sans
rien réclamer. Un Apple II de base suffit.

Et le moteur ne contient **aucun mot d'anglais imposé** : menus, invites,
messages de fin, tout vient du jeu lui-même. Traduire une aventure, c'est éditer
son fichier — rien d'autre.

---

## Ce qu'on peut raconter avec

Des choix conditionnels, des objets à trouver, des caractéristiques qui montent
et qui s'usent, un score. Des combats au tour par tour où l'équipement compte.
Des énigmes où le joueur tape un mot au clavier. Des images plein écran. Du son.
Une sauvegarde sur disquette.

Assez pour une vraie aventure, pas seulement une démonstration.

---

## Une aventure pour l'essayer

> On brûla ses temples, on pendit ses prêtres, on martela son nom sur chaque
> pierre et on le raya des calendriers. On le crut mort avec son monde.
>
> On avait tort.

**L'Homme en Costume Blanc** — Golfe du Mexique, années trente. Vous dirigez une
fouille pour le Musée d'Histoire naturelle de New York, et la colline que vous
éventrez depuis un mois n'était pas une colline.

Quatre chapitres, de la jungle au dernier étage d'un immeuble de Central Park
Ouest. Trois personnages au choix, aux angles morts différents : certaines portes
ne s'ouvrent qu'à qui sait regarder, d'autres qu'à qui sait cogner. Soixante
sections, une fin heureuse et trois façons d'y rester.

Adaptation d'un scénario de Cosmicsoap publié par Rolis, sous licence CC BY-SA 4.0.

---

## Où ça en est

Le moteur joue des aventures complètes, et le compilateur les vérifie avant de
les livrer. Une aventure de soixante sections tourne de bout en bout.

Prochaine étape : le passage sur émulateur, puis sur une machine physique. Tout
est écrit — rien n'a encore chauffé un vrai tube cathodique.
