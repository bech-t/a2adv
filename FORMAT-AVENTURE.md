# Le format `.adv` — écrire une aventure a2adv

Une aventure a2adv est **un seul fichier texte**, en UTF-8, avec l'extension
`.adv`. Vous l'écrivez dans n'importe quel éditeur ; le compilateur `a2c` le
transforme en données binaires pour l'Apple II.

Ce document décrit le format source. Le format binaire produit (`STORYnn.DAT`,
`ASSETS.IDX`) est documenté dans `spec.md` §7ter — vous n'avez pas besoin de le
connaître pour écrire une aventure.

---

## Le principe : une ligne, un rôle

Le fichier est **orienté lignes**. C'est le **premier caractère non blanc** d'une
ligne qui décide de ce qu'elle est :

| Il commence par | La ligne est |
|---|---|
| `#` | un commentaire |
| `::` | le début d'une **section** |
| `@` | une **directive** |
| `*` | un **choix** |
| `~` | un **effet** |
| `{…}` | du **texte conditionnel** |
| autre chose | du **texte narratif** |

Il n'y a rien d'autre à retenir. L'indentation est décorative.

## Texte et paragraphes

Les lignes de texte consécutives sont **réunies en un seul paragraphe**, que le
moteur re-justifie à la largeur réelle de l'écran (40 ou 80 colonnes). La façon
dont vous coupez vos lignes dans le `.adv` n'a donc **aucun effet** à l'écran :
écrivez confortablement.

Une **ligne vide sépare deux paragraphes**.

```
Le camp fume sous une pluie tiede. Les ouvriers chargent deja le camion
des premieres caisses ; la jeep attend, capot brulant.

Personne n'ose descendre avant vous.
```

**Styles** — un marqueur suivi d'un espace, en tête de paragraphe :

| Écriture | Rendu |
|---|---|
| `= titre` | centré |
| `! attention` | inversé (surbrillance) |
| `=! TITRE` | centré et inversé |
| `*mot*` | le mot en surbrillance, au fil du texte |

**Accents** : écrivez-les normalement dans la source. Le compilateur les
translittère en majuscules ASCII, seul jeu de caractères garanti sur un
Apple II (`é` → `E`, `ç` → `C`). Vous écrivez du français propre, la machine
affiche ce qu'elle sait afficher.

---

## Le préambule

Avant la première section, on déclare tout l'état du jeu.

```
@title  L'Homme en Costume Blanc
@author Votre nom
@start  premiere_section          # obligatoire : par où commence la partie
@intro  intro_1 intro_2           # scènes jouées au lancement (facultatif)

@stat ADRESSE    8 0 12           # nom  initial  min  max
@stat ENDURANCE 20 0 24

@item machette "Machette" on atk=1 dmg=3    # `on` = possédé au départ
@item urne     "Urne rituelle"              # absent par défaut

@flag porte_ouverte               # drapeau d'histoire, `off` par défaut
@flag q_sujet_demande local       # drapeau LOCAL (voir plus bas)

@score on                         # compteur de points
@moves on                         # compteur de déplacements

@ui menu_new "COMMENCER"          # traduire n'importe quel texte d'interface
```

**Tout doit être déclaré.** Le compilateur refuse une stat, un objet ou un
drapeau employé sans déclaration : c'est votre filet contre les fautes de frappe,
et c'est ce qui rend les sauvegardes stables d'une version à l'autre.

Les objets n'ont **pas de quantité** : on les a ou on ne les a pas. Ils peuvent
porter des modificateurs de combat (`atk`, `dmg`, `armor`) actifs tant qu'ils
sont possédés.

---

## Une section

```
:: caverne_entree
@mode  image_text                 # full_text | image_text | full_image
@image caverne                    # requis si le mode affiche une image

~ add MORAL 1                     # effets joués à l'entrée dans la section
~ set a_visite

La salle s'ouvre sur un gouffre. L'air qui en monte est froid.
{flag torche_allumee} La flamme dessine des bas-reliefs sur les parois.
{not flag torche_allumee} Il fait trop noir pour distinguer quoi que ce soit.

* [Descendre] -> gouffre
* {has corde} [Descendre en rappel] -> gouffre_sur
  ~ score 10
* [Rebrousser chemin] -> foret
```

- Un choix dont la condition est fausse **n'apparaît pas**.
- Les effets `~` placés **avant le premier choix** s'appliquent à l'entrée ; après
  un `*`, ils appartiennent à ce choix.
- Une section **sans aucun choix est une fin**. Précisez `@ending victoire` ou
  `@ending defaite` pour l'écran de conclusion.
- `@on_exit` regroupe des effets appliqués en sortant, quel que soit le choix.

---

## Conditions

Une condition s'écrit entre accolades. Les atomes disponibles :

```
flag NOM          not flag NOM
has OBJET         not has OBJET
stat NOM >= 5     ( >=  <=  >  <  ==  != )
```

On les relie par `and` **ou** `or` — un seul des deux à la fois, sans
parenthèses.

```
{has torche}
{not flag porte_ouverte}
{stat ADRESSE >= 9 and has machette}
```

## Effets

```
~ set FLAG            ~ clear FLAG         ~ toggle FLAG
~ give OBJET          ~ take OBJET
~ add STAT N          ~ sub STAT N         ~ set STAT N
~ setmax STAT N       ~ restore STAT
~ score N             ~ sound win          ~ goto SECTION
```

Les stats sont automatiquement bornées à leur intervalle déclaré.

**Tout effet peut être gardé par une condition** :

```
~ {not flag vu} score 10          # les points une seule fois
~ {stat PERCEPTION >= 8} set a_remarque_la_silhouette
```

C'est le mécanisme qui permet de récompenser un personnage perspicace sans
dupliquer la section.

---

## Combat

```
:: combat_gobelin
@combat "Gobelin" att=6 hp=9 dmg=3 armor=0 image=gobelin
@victoire sortie "Le gobelin s'effondre dans un dernier rale."
  ~ score 100
  ~ give tresor
@defaite mort
@fuite   entree
Le gobelin bondit, dague au poing !
```

Le héros attaque à **2d6 + sa stat d'attaque** (choisie au préambule par
`@combat_attack`), ses points de vie sont une stat (`@combat_hp`). Les
modificateurs des objets portés s'ajoutent automatiquement.

Chaque issue mène à une section et peut porter ses propres effets. Le texte entre
guillemets, s'il est présent, s'affiche sur un écran d'issue avec attente d'une
touche — de quoi conclure un combat sans coupure sèche.

## Énigmes à saisie

```
:: coffre
@ask "Le mot a composer ?" maxlen=20
@answer XIPE TOTEC
@answer ECORCHE
@correct coffre_ouvert
  ~ score 30
@wrong   coffre_alarme
Un coffre a cadran de lettres.
```

La comparaison ignore la casse, les accents et les espaces de bord. Plusieurs
`@answer` sont acceptées. Le moteur n'affiche aucun message de succès ou d'échec :
c'est la section `@correct` ou `@wrong` qui le fait — vous gardez la main sur le
ton.

---

## Chapitres

```
@chapter "Le Musee"
```

`@chapter` marque une frontière : les sections qui suivent partent dans un
**fichier séparé** sur la disquette. Le moteur ne garde en mémoire que l'index du
chapitre courant, ce qui rend le nombre total de sections illimité (256 par
chapitre au maximum).

Découpez généreusement : c'est aussi ce qui permet au moteur de mettre les
chapitres en cache dans le disque RAM.

## Drapeaux locaux

Un drapeau déclaré `local` se manipule exactement comme les autres — seule sa
**durée de vie** change : il est **remis à zéro à chaque changement de chapitre**.

```
@flag q_gala local
```

C'est fait pour les carrefours et les dialogues, où l'on veut se souvenir « ce
sujet a déjà été abordé » sans polluer l'état global — et pour qu'un lieu
revisité au chapitre suivant reparte propre, sans nettoyage à la main.

Un dialogue court tient alors dans une seule section :

```
:: georgio
* {not flag q_gala} [Le braquage du gala] -> georgio
  ~ set q_gala
* {not flag q_peru} [Le Peruvien] -> georgio
  ~ set q_peru
* [Le laisser a son cafe] -> enquete

{flag q_gala} « Le gala ? On n'etait pas seuls sur ce coup-la. »
{flag q_peru} « Le type en blanc ? Il paie cash. »
```

Maximum 32 drapeaux locaux ; ils démarrent toujours à `off`.

---

## Limites à connaître

| Élément | Limite |
|---|---|
| Corps d'une section | **1024 octets** compilés |
| Sections par chapitre | 256 |
| Chapitres | 100 |
| Caractéristiques | 8 |
| Objets | 24 |
| Drapeaux | 128, dont 32 locaux |
| Images | HIRES 8 Ko, une par section |

La limite qui surprend en pratique est celle de la **section** : une longue
description avec plusieurs variantes conditionnelles l'atteint plus vite qu'on ne
croit. Le compilateur vous le dit avec le nom de la section fautive — il suffit
alors de la scinder.

## Compiler et vérifier

```bash
cd compiler
python3 -m a2c ../adventures/mon_aventure/mon_aventure.adv -o /tmp/sortie --summary
python3 -m a2c.analyze ../adventures/mon_aventure/mon_aventure.adv
```

`a2c.analyze` est votre relecteur : il signale les sections qu'on ne peut pas
atteindre, celles dont on ne peut pas sortir, les fins injoignables, les objets
exigés par une condition mais jamais donnés, les drapeaux morts.

Puis, pour obtenir la disquette :

```bash
cd player/apple2
make dsk ADV=mon_aventure
```
