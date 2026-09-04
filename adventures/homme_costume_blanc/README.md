# L'Homme en Costume Blanc

> Avant les conquistadors, avant la Croix, le Mexique redoutait un dieu entre
> tous : **Xipe Totec**, l'Écorché.
>
> On brûla ses temples, on pendit ses prêtres, on martela son nom sur chaque
> pierre et on le raya des calendriers. On le crut mort avec son monde.
>
> On avait tort.

**Un livre-jeu pour Apple II.** Golfe du Mexique, années trente. Vous dirigez
une fouille pour le Musée d'Histoire naturelle de New York, et depuis un mois
vous éventrez une colline qui n'était pas une colline : c'est une ziggourat, un
temple entier enfoui sous la jungle.

Ce matin, l'entrée est ouverte.

Ce que vous en remonterez fera votre nom. Ce que vous en remonterez vous suivra
jusqu'à New York.

---

## Ce qui vous attend

Quatre chapitres, de la jungle au dernier étage d'un immeuble de Central Park
Ouest, en passant par une soirée de gala qui tourne mal et une enquête dans une
ville où l'on retrouve des corps écorchés.

- **Trois personnages** au choix — l'érudit, le baroudeur, l'aventurier — avec
  des forces et des angles morts différents. Certaines portes ne s'ouvrent qu'à
  qui sait regarder ; d'autres qu'à qui sait cogner.
- **Des combats** au tour par tour, où votre équipement compte.
- **Une énigme à composer** au clavier.
- **Quatre caractéristiques** : Adresse, Endurance, Sang-Froid, Perception. Le
  Sang-Froid s'use — et ce que vous voyez de trop l'entame plus vite.
- **Une fin de victoire, trois façons d'y rester.**
- Un score, pour ceux qui veulent y retourner et faire mieux.

Comptez une heure la première fois. Deux ou trois parties pour voir ce que les
autres personnages, eux, avaient remarqué.

## Jouer

L'aventure se distribue sous forme de **disquette ProDOS bootable**, à démarrer
dans n'importe quel émulateur Apple II — ou sur une vraie machine.

Elle tourne sur un Apple II 64 Ko. Si votre machine ou votre émulateur en offre
plus, elle s'en sert toute seule : 80 colonnes sur //e, carte son Mockingboard,
et chargement accéléré par le disque RAM sur 128 Ko.

Pour la fabriquer depuis les sources — il vous faut `cc65`, Python 3 et Java :

```bash
cd player/apple2
make dsk ADV=homme_costume_blanc
```

La disquette apparaît dans `adventures/homme_costume_blanc/build/`.

**En jeu** : les chiffres choisissent, `I` ouvre l'inventaire, `S` sauvegarde,
`L` recharge, `Q` revient au menu.

## Crédits et licence

Adaptation du scénario **« L'Homme en Costume Blanc »**, premier épisode de la
campagne *Notre Seigneur l'Écorché*, écrit par **Cosmicsoap** et publié par
**Rolis** — adaptation Simulacres de **Nicolas Nguyen**.

Scénario original : [rolis.net](https://rolis.net/scenarios/scenarioWeb/43/l-homme-en-costume-blanc)

La prose a été réécrite à la deuxième personne pour le format livre-jeu, mais
reste fidèle au fil, aux personnages et aux lieux de l'original.

**Licence [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/deed.fr)**
— réutilisation et modification libres, à condition de créditer les auteurs et de
partager sous la même licence.

---

*Écrit avec [a2adv](../../README.md). Le fichier source, `homme_costume_blanc.adv`,
se lit comme un texte — allez y jeter un œil si vous voulez écrire le vôtre.*
