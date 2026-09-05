# Le format `.adv` — référence complète

Une aventure a2adv (livre-jeu interactif pour Apple II) s'écrit dans **un seul
fichier texte**, encodé en UTF-8, avec l'extension `.adv`. Ce document décrit
entièrement ce format : chaque élément qu'on peut y écrire, sa syntaxe exacte,
ses limites, et la façon dont on s'en sert en pratique.

---

## 1. Principe général

Le fichier est **orienté lignes**. Le **premier caractère non blanc** d'une
ligne décide entièrement de ce qu'elle est :

| Premier caractère | Rôle de la ligne |
|---|---|
| `#` | commentaire (ligne entière ignorée) |
| `::` | début d'une **section** |
| `@` | une **directive** |
| `*` suivi d'un espace | un **choix** |
| `~` | un **effet** |
| `{…}` | du **texte conditionnel** |
| n'importe quoi d'autre | du **texte narratif** |

Il n'y a rien de plus à retenir sur la structure de base. L'indentation est
purement décorative — elle aide à lire le fichier, elle n'a aucun effet sur le
jeu produit.

Un point à connaître : le `#` de commentaire n'est retiré **qu'en fin des
lignes structurelles** (`@`, `::`, `*`, `~`). Sur une ligne de texte narratif,
un `#` est un caractère comme un autre et s'affiche tel quel — utile si un jour
un texte doit citer un mot-dièse, mais à savoir si vous essayez d'ajouter un
commentaire en bout de phrase narrative : ça ne marchera pas, il faut le
mettre sur sa propre ligne.

---

## 2. Texte et paragraphes

Les lignes de texte narratif **consécutives** sont réunies en un seul
paragraphe, que le moteur redécoupe et justifie à la largeur réelle de l'écran
(40 ou 80 colonnes selon la machine). La façon dont vous coupez vos lignes dans
le fichier source n'a donc **aucune incidence** sur le rendu : écrivez
confortablement, sans vous soucier de la largeur de ligne.

Une **ligne vide sépare deux paragraphes** :

```
Le camp fume sous une pluie tiede. Les ouvriers chargent deja le camion
des premieres caisses ; la jeep attend, capot brulant.

Personne n'ose descendre avant vous.
```

Une ligne **stylée** (voir plus bas) ou **conditionnelle** (`{…}`) forme
toujours son propre paragraphe, même si elle suit immédiatement une ligne de
texte ordinaire — elle ne se fond jamais dans le paragraphe précédent.

### Styles de paragraphe

Un paragraphe peut commencer par un marqueur de style : une suite de `=` et/ou
`!`, **suivie d'un espace**. Sans cet espace, le caractère est pris pour du
texte littéral (ça permet d'écrire une phrase qui commence vraiment par un
signe égal, par exemple).

| Écriture | Rendu |
|---|---|
| `= Titre` | centré |
| `! Attention` | vidéo inversée |
| `=! GROS TITRE` | centré **et** inversé |

### Mise en relief dans le texte : `*mot*`

N'importe où dans un paragraphe, une paire d'astérisques bascule l'affichage en
vidéo inversée pour le texte qu'elle encadre :

```
Vous les reconnaissez : *les freres Jean*. Chasseurs de tresors, voleurs de
gloire.
```

Chaque `*` rencontré dans le texte **bascule** l'état (allumé ↔ éteint) : ce
n'est pas une paire qui s'interprète globalement, c'est un simple compteur.
Utilisez-les donc toujours **par paires** dans un même paragraphe. Un `*`
esseulé n'est pas une erreur bloquante — le moteur remet l'affichage en mode
normal par sécurité en fin de ligne — mais le résultat visuel devient
imprévisible, donc autant les compter.

> Un choix (`*` suivi d'un espace en tout début de ligne) n'est jamais
> confondu avec cette mise en relief : c'est la présence de l'espace juste
> après l'astérisque qui tranche. `* [Ouvrir]` est un choix ; `*ouvrir*` en
> milieu de texte est une mise en relief.

### Accents et jeu de caractères

Écrivez votre français normalement dans le fichier source, avec tous ses
accents. Le compilateur le ramène à l'**ASCII** : le générateur de caractères
de l'Apple II n'a aucun glyphe accentué, ni en 40 ni en 80 colonnes. La
conversion est automatique et couvre :

- les voyelles accentuées (`é`, `è`, `ê`, `à`, `ù`, `ô`, `î`, `ï`, `â`, `û`…) →
  la lettre nue correspondante ;
- `ç` → `c` ;
- les ligatures : `œ` → `oe`, `æ` → `ae`, `ß` → `ss` (en capitales si le mot
  l'est : `ŒUVRE` → `OEUVRE`, mais `Œuf` → `Oeuf`) ;
- les guillemets typographiques (`«`, `»`, guillemets courbes) → `"` droit ;
- les tirets longs (`–`, `—`) → `-`, les points de suspension (`…`) → `...` ;
- tout caractère qui resterait non-ASCII après ce traitement devient `?`.

Vous écrivez donc du français propre et lisible dans votre éditeur ; la
machine affiche ce qu'elle peut afficher.

**La casse, elle, se choisit à la compilation.** Par défaut `a2c` conserve la
casse du source : sur un //e, les minuscules s'affichent — en 40 comme en 80
colonnes — et rendent un long paragraphe bien plus confortable à lire.
L'option `--majuscules` force tout en capitales :

```bash
python3 -m a2c mon_aventure.adv -o build --majuscules
```

C'est le rendu d'origine, et le seul affichable sur un **Apple II ou II+** :
leur générateur de caractères ne contient que 64 glyphes, couvrant l'ASCII
`$20-$5F`. Il n'y a tout simplement pas de glyphe minuscule dedans. Le //e, lui,
couvre l'ASCII `$20-$7F` — c'est une question de générateur de caractères, pas
de largeur d'écran.

Les réponses d'`@ask` échappent à ce choix : elles sont toujours normalisées en
capitales, des deux côtés de la comparaison. La casse d'affichage ne décide
jamais si une réponse est acceptée.

---

## 3. Commentaires

Une ligne dont le premier caractère non blanc est `#` est entièrement ignorée,
n'importe où dans le fichier — préambule, intérieur d'une section, entre deux
choix. C'est le seul moyen de laisser une note à vous-même : il n'existe pas
d'autre syntaxe de commentaire, et un `#` en fin d'une ligne de texte narratif
**n'est pas** un commentaire (voir §1).

---

## 4. Les sections

Une section commence par `::` suivi d'un nom, et se poursuit jusqu'à la
prochaine section (ou la fin du fichier). C'est l'unité de base : le joueur va
toujours d'une section à l'autre.

```
:: caverne_entree
```

Le nom doit être un **identifiant** : lettres, chiffres, underscore, sans
commencer par un chiffre, sans accent ni espace (`caverne_entree`,
`piste_moretti_2`, `_secret` — tous valides ; `caverne entrée` ne l'est pas).
La même règle s'applique en pratique aux noms de caractéristiques, d'objets et
de drapeaux (voir plus bas) : ce sont de simples mots séparés par des espaces
dans le fichier, donc un nom avec un espace ou une accolade casserait la
syntaxe.

Une section peut contenir, dans un ordre globalement libre :

- des **directives** propres à la section (`@mode`, `@image`, `@ending`,
  `@combat`, `@ask`…) ;
- des **effets d'entrée** (`~ ...` avant le premier choix) ;
- du **texte**, conditionnel ou non ;
- des **choix** (`*`), chacun pouvant porter ses propres effets.

### Fin de partie implicite

Une section **sans aucun choix écrit** est automatiquement une fin de partie.
Sans `@ending`, le joueur voit l'écran générique `*** FIN ***` ; avec
`@ending victoire` ou `@ending defaite`, il voit l'écran dédié (et le son de
victoire/défaite joue).

Il existe une **troisième situation**, plus subtile : une section qui a bien
des choix écrits, mais dont **toutes les conditions sont fausses** pour l'état
courant du joueur. Dans ce cas, le moteur affiche `(AUCUNE ISSUE POSSIBLE)` et
termine la partie — ce n'est ni une vraie fin, ni une erreur détectée à la
compilation, puisque ça dépend de l'état du joueur au moment où il y arrive.
C'est un cas à connaître : la façon de s'en prémunir est du ressort de la
conception, pas du format — voir le guide des bonnes pratiques.

### `@on_exit`

Des effets qu'on veut appliquer **quel que soit le choix pris** se regroupent
sous `@on_exit`, plutôt que d'être dupliqués sous chaque choix :

```
@on_exit
  ~ set q_gala
```

---

## 5. Le préambule

Avant la première section, on déclare tout ce que le jeu va manipuler : titre,
caractéristiques, objets, drapeaux. **Tout doit être déclaré à l'avance** — le
compilateur refuse qu'une caractéristique, un objet ou un drapeau soit utilisé
sans avoir été déclaré ici. C'est un filet contre les fautes de frappe, et
c'est ce qui garantit que le format des sauvegardes reste stable.

### `@title`, `@author`, `@start`, `@intro`

```
@title  L'Homme en Costume Blanc
@author Votre nom
@start  premiere_section
@intro  intro_1 intro_2 intro_3
```

- `@title` (33 caractères max) et `@author` sont de simples chaînes, affichées
  au menu.
- `@start` est **obligatoire** : c'est la section par laquelle une nouvelle
  partie commence.
- `@intro` liste des sections jouées automatiquement au lancement, avant
  `@start` (8 au maximum) — pratique pour une préface, un générique, un choix
  de personnage.

### `@lang`

```
@lang fr
```

Choisit le socle de chaînes d'interface (menus, invites, messages de combat)
dans lequel l'aventure va puiser par défaut. Un code de deux lettres
minuscules. On peut ensuite surcharger n'importe laquelle de ces chaînes avec
`@ui` (voir plus bas) sans avoir à en écrire une traduction complète.

### `@stat` — les caractéristiques

```
@stat NOM  init  min  max  [hidden]
```

```
@stat ADRESSE    8 0 12
@stat ENDURANCE 20 0 24
@stat NUITS      0 0 9 hidden
```

- `init`, `min`, `max` sont des entiers entre 0 et 255 (`min` peut être omis
  avec `max`, la paire est alors implicitement `0 255`).
- `hidden` : la caractéristique reste utilisable dans toutes les conditions et
  tous les effets, mais **n'apparaît pas** dans le bandeau d'état du joueur.
  C'est fait pour les compteurs internes que le joueur doit *ressentir* par le
  récit plutôt que lire sur un cadran (un compteur de nuits, de suspicion,
  de corruption…).
- Maximum **8 caractéristiques** ; le nom tient sur 12 caractères affichés.

### `@item` — les objets

```
@item id ["Libellé"] [on|off] [atk=N] [dmg=N] [armor=N]
```

```
@item machette "Machette"          on  atk=1 dmg=3
@item urne     "Urne rituelle"
@item dague    "Dague d'obsidienne"    dmg=3
```

- Les objets **n'ont pas de quantité** : on les possède, ou pas.
- Le libellé entre guillemets est ce qui s'affiche dans l'inventaire ; s'il est
  omis, c'est l'id lui-même qui s'affiche.
- `on` (par défaut `off`) : possédé dès le début de la partie.
- `atk`, `dmg`, `armor` (entiers signés, -128 à 127) : des modificateurs de
  combat, actifs automatiquement tant que l'objet est en possession du joueur
  — pas besoin d'un effet séparé pour les activer.
- Maximum **24 objets** ; le libellé tient sur 20 caractères.

### `@flag` — les drapeaux

```
@flag nom [on|off] [local]
```

```
@flag porte_ouverte
@flag urne_geole      # commence a off, jamais force
@flag q_gala   local  # remis a zero a chaque chapitre
```

- Un booléen d'histoire ; `off` par défaut.
- `local` : voir la section dédiée plus bas. Un drapeau local ne peut **pas**
  démarrer à `on` (`@flag x on local` est refusé) — il vaut toujours `off` au
  début, par construction.
- Maximum **128 drapeaux au total**, dont **32 locaux**.

### `@score`, `@moves`

```
@score on
@moves on
```

Active (ou non) l'affichage d'un compteur de points et d'un compteur de
déplacements dans le bandeau d'état. Indépendant de l'effet `~ score N` (voir
§7), qui fonctionne de toute façon — `@score off` masque juste l'affichage.

### `@combat_attack`, `@combat_hp`, `@combat_basedmg`

```
@combat_attack  ADRESSE
@combat_hp      ENDURANCE
@combat_basedmg 2
```

Configuration globale du combat, une fois pour toute l'aventure : quelle
caractéristique sert de score d'attaque au héros, laquelle sert de points de
vie, et quels dégâts de base il inflige à mains nues (avant modificateurs
d'objets). Le déroulé d'un combat est détaillé au §8.

### `@ui` — personnaliser une chaîne d'interface

```
@ui menu_new  "COMMENCER"
@ui cb_flee   "DECAMPER"
```

Remplace, pour cette seule aventure, n'importe laquelle des chaînes fixes de
l'interface (menus, écran de combat, messages système). Toutes les clés
disponibles, avec leur valeur par défaut en français :

| Clé | Défaut |
|---|---|
| `menu_new` | COMMENCER |
| `menu_load` | CHARGER |
| `menu_quit` | QUITTER |
| `menu_options` | OPTIONS |
| `inv_hud` | INV: |
| `hints` | I=INV S=SAUVE L=CHARGE Q=MENU |
| `anykey` | APPUYEZ SUR UNE TOUCHE... |
| `intro_hint` | ESPACE OU ENTREE POUR CONTINUER |
| `end_win` | \*\*\* VICTOIRE \*\*\* |
| `end_lose` | \*\*\* DEFAITE \*\*\* |
| `end_generic` | \*\*\* FIN \*\*\* |
| `saved` | PARTIE SAUVEGARDEE. |
| `save_fail` | ECHEC DE LA SAUVEGARDE. |
| `no_save` | AUCUNE SAUVEGARDE TROUVEE. |
| `inventory` | INVENTAIRE |
| `inv_empty` | (INVENTAIRE VIDE) |
| `no_exit` | (AUCUNE ISSUE POSSIBLE) |
| `section_err` | ERREUR DE CHARGEMENT DE SECTION |
| `quit_confirm` | REVENIR AU MENU ? |
| `quit_save` | S) SAUVEGARDER ET REVENIR |
| `quit_nosave` | Q) REVENIR SANS SAUVEGARDER |
| `quit_cancel` | ESC) ANNULER |
| `loading` | CHARGEMENT |
| `saving` | SAUVEGARDE |
| `score` | SCORE |
| `moves` | MVT |
| `cbt_atk` / `cbt_dmg` / `cbt_arm` | ATT / DEG / ARM |
| `cb_hp` | PV |
| `cb_dice` | DES |
| `cb_you` | VOUS |
| `cb_parry` | PARADE |
| `cb_attack` | ATTAQUER |
| `cb_flee` | FUIR |
| `opt_title` | OPTIONS SON |
| `opt_output` | SORTIE |
| `opt_speaker` | HAUT-PARLEUR |
| `opt_mb` | MOCKINGBOARD |
| `opt_slot` | SLOT |
| `opt_mb_slots` | MOCKINGBOARD (NO DE SLOT) |
| `opt_no_mb` | MOCKINGBOARD NON COMPILEE |
| `opt_test` | TESTER LES SONS |
| `opt_back` | RETOUR |
| `snd_title` | TEST DES SONS |
| `snd_all` | TOUT JOUER |

---

## 6. Conditions

Une condition s'écrit entre accolades, en préfixe d'une ligne de texte, d'un
choix, ou d'un effet. Les atomes disponibles :

```
flag NOM              vrai si le drapeau NOM est actif
not flag NOM           vrai s'il ne l'est pas
has OBJET              vrai si l'objet est possédé
not has OBJET           vrai s'il ne l'est pas
stat NOM OP VALEUR      comparaison numérique
```

Opérateurs de comparaison acceptés pour `stat` : `==`, `!=`, `<`, `<=`, `>`,
`>=`.

Plusieurs atomes se combinent avec `and` **ou** `or` — un seul des deux à la
fois dans une même condition, sans parenthèses :

```
{has torche}
{not flag porte_ouverte}
{stat ADRESSE >= 9 and has machette}
{flag lead_moretti or flag lead_conservateur}
```

> Il n'existe pas de `not stat …` : pour l'inverse d'une comparaison,
> utilisez l'opérateur complémentaire (`stat NUITS < 5` plutôt que
> `not stat NUITS >= 5`).

Une ligne de texte conditionnelle ne s'affiche que si sa condition est vraie ;
un choix dont la condition est fausse **n'apparaît tout simplement pas** dans
la liste proposée au joueur.

---

## 7. Effets

Un effet s'écrit avec `~`, soit avant le premier choix d'une section (effet
d'**entrée**), soit juste après un choix (appliqué **si ce choix est pris**,
avant le saut vers sa section cible).

```
~ set FLAG              active un drapeau
~ clear FLAG             le désactive
~ toggle FLAG            inverse son état
~ give OBJET             donne un objet
~ take OBJET             le retire
~ add STAT N             ajoute N à une caractéristique
~ sub STAT N             lui retranche N
~ set STAT N             la fixe à N
~ setmax STAT N          change son PLAFOND (pas sa valeur courante)
~ restore STAT           la remet à son plafond courant (soin complet)
~ score N                ajoute N au score (peut être négatif)
~ sound NOM              joue un effet sonore (liste au §9)
~ goto SECTION           saute directement vers une autre section
```

Toute caractéristique modifiée par `add`, `sub` ou `set` est **automatiquement
bornée** entre son minimum déclaré et son plafond courant — impossible de la
faire sortir de sa plage, pas besoin de le vérifier vous-même.

`setmax` et `restore` sont un couple à part : ils changent le **plafond**
(distinct de la valeur courante), utile pour un personnage dont la jauge
maximale dépend d'un choix initial :

```
:: perso_erudit
~ setmax SANG_FROID 12    # ce personnage peut monter jusqu'a 12...
~ set    SANG_FROID 12    # ...et il commence au maximum
```

### Effets conditionnels

**Tout effet peut être gardé par une condition**, y compris un effet d'entrée
ou de choix :

```
~ {not flag deja_vu} score 10        # les points, une seule fois
~ {stat PERCEPTION >= 8} set a_remarque_le_detail
```

C'est le mécanisme qui permet de récompenser un personnage perspicace, ou de
n'accorder un bonus qu'une fois, sans dupliquer toute une section pour ça.

### `@on_enter` : ajouter des effets d'entrée après coup

Par défaut, tous les `~` écrits **avant le premier choix** d'une section sont
des effets d'entrée. Mais une fois qu'un choix a été écrit, les `~` suivants
lui appartiennent, pas à l'entrée de la section. Si vous devez ajouter un
effet d'entrée plus loin dans le fichier (après avoir déjà écrit des choix),
la directive `@on_enter` redirige explicitement les `~` qui suivent :

```
:: exemple
~ score 5                # effet d'entree (implicite)

* [Une option] -> ailleurs
  ~ set vu_ailleurs       # effet de CE choix

@on_enter
~ set toujours_vrai       # de nouveau un effet d'entree, explicite cette fois
```

Ce mécanisme d'« attache » (le prochain `~` va au dernier `@on_enter`,
`@on_exit`, choix, `@victoire`/`@defaite`/`@fuite`, ou `@correct`/`@wrong`
rencontré) est purement séquentiel — l'indentation visuelle n'y joue aucun
rôle, elle ne fait qu'aider à la lecture.

---

## 8. Combat

```
:: combat_gobelin
@combat "Gobelin" att=6 hp=9 dmg=3 armor=0 image=gobelin
@victoire sortie "Le gobelin s'effondre dans un dernier rale."
  ~ score 100
  ~ give tresor
@defaite mort
@fuite   entree
Le gobelin bondit, dague au poing !

* [Affronter] -> combat_gobelin
```

- `@combat "Nom" att=N hp=N dmg=N armor=N [image=id]` décrit l'adversaire.
  `hp` doit être **au moins 1** ; les autres valeurs vont de 0 à 255. `image`
  est optionnelle (portrait affiché ~3 secondes avant le premier round).
- `@victoire` et `@defaite` sont **obligatoires** : chacune pointe vers une
  section, avec un texte d'issue optionnel entre guillemets (255 caractères
  max), affiché sur un écran dédié avant le saut. Les `~` indentés juste
  après s'appliquent avant de sauter.
- `@fuite` est **optionnelle**. Si elle est présente, une option « FUIR »
  apparaît à chaque round de combat : la fuite réussit **toujours**, mais
  l'adversaire porte un dernier coup gratuit au moment de fuir — un coup qui
  peut être fatal (le joueur bascule alors sur `@defaite`, pas sur `@fuite`).

**Résolution d'un round** : le héros et l'adversaire lancent chacun 2d6 et y
ajoutent leur score d'attaque (`@combat_attack` + bonus d'objets portés pour
le héros). Le plus haut score touche ; en cas d'égalité, personne n'est
touché. Les dégâts infligés valent `max(1, dégâts − armure adverse)` — il y a
toujours au moins 1 point de dégâts sur un coup qui porte, ce qui garantit
qu'un combat se termine.

---

## 9. Sons

```
~ sound magic
```

Neuf effets sonores prédéfinis, communs à toute aventure : `select`, `error`,
`win`, `lose`, `pickup`, `hit`, `magic`, `door`, `page`. `~ sound` accepte
n'importe lequel de ces identifiants ; il n'y a pas d'import de sons
personnalisés.

---

## 10. Énigmes à saisie clavier

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

- `@ask "invite" [maxlen=N]` (N entre 1 et 40, 20 par défaut) affiche l'invite
  et attend une saisie clavier.
- `@answer` accepte une ou plusieurs réponses valables, avec ou sans
  guillemets (`@answer XIPE TOTEC` et `@answer "XIPE TOTEC"` sont
  équivalentes). Au moins une est obligatoire.
- `@correct` et `@wrong` sont **tous deux obligatoires**, chacun vers une
  section, avec ses propres `~` indentés appliqués avant le saut.
- La comparaison **ignore la casse, les accents et les espaces en bord de
  chaîne** — le joueur peut taper « xipe totec », « Xipe Tôtec » ou
  « XIPE TOTEC » indifféremment. Le moteur n'affiche lui-même aucun message
  de réussite ou d'échec : c'est la section `@correct` ou `@wrong` qui porte
  ce texte, donc vous gardez la main sur le ton.

---

## 11. Images

```
:: caverne_entree
@mode  image_text
@image caverne
```

Trois modes d'affichage par section :

| Mode | Rendu |
|---|---|
| `full_text` | texte seul, plein écran (par défaut) |
| `image_text` | image en haut, quelques lignes de texte et les choix en bas |
| `full_image` | image plein écran, attente d'une touche |

Un mode graphique (`image_text` ou `full_image`) exige `@image id` — sans
image déclarée, la compilation échoue. À l'inverse, une `@image` posée sur une
section `full_text` est ignorée (avec un avertissement) : l'image ne
s'affichera jamais.

En `image_text`, la place est comptée : environ 4 lignes de 40 colonnes sont
disponibles pour le texte **et** les choix réunis (chaque tranche de 40
caractères de texte compte pour une ligne, chaque choix aussi). Au-delà, un
avertissement de compilation invite à passer en `full_text`, faute de place à
l'écran.

---

## 12. Chapitres

```
@chapter "Le Musee"
```

`@chapter` marque une frontière dans le fichier : toutes les sections qui
suivent (jusqu'au prochain `@chapter`) sont regroupées et livrées séparément
sur la disquette. Pour l'auteur, ça ne change rien à l'écriture — les sections
d'avant et d'après se référencent normalement entre elles par leur nom. C'est
un découpage purement technique, qui permet à la machine de ne garder en
mémoire que le chapitre en cours et d'en précharger le contenu pendant que le
joueur lit.

Découper généreusement en chapitres coûte donc rien narrativement et aide la
machine — un bon repère est une frontière de chapitre à chaque licenciement de
lieu ou de temps fort (un nouveau site, une nouvelle nuit, un nouveau
personnage aux commandes).

---

## 13. Drapeaux locaux

Un drapeau déclaré `local` fonctionne exactement comme un drapeau normal —
seule sa **durée de vie** change : il est remis à `off` **à chaque
changement de chapitre**, automatiquement.

```
@flag q_gala local
```

C'est fait pour les carrefours et les dialogues à choix multiples, où l'on
veut retenir « ce sujet a déjà été abordé » **pour la durée du chapitre**,
sans polluer l'état global de la partie ni avoir à le remettre à zéro à la
main quand le joueur change de lieu :

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

Chaque option se retire de la liste une fois choisie, et le carrefour reste
ouvert (`* [...] -> georgio`, la section se relance elle-même) jusqu'à ce que
le joueur choisisse de partir. Au chapitre suivant, ces deux drapeaux
repartent à zéro : revisiter Georgio y redevient possible sans rien nettoyer
à la main.

---

## 14. Limites à connaître

| Élément | Limite |
|---|---|
| Corps d'une section (compilé) | **1024 octets** |
| Choix simultanément affichables | 9 (touches 1 à 9) |
| Scènes d'intro (`@intro`) | 8 |
| Caractéristiques | 8 |
| Nom de caractéristique | 12 caractères |
| Objets | 24 |
| Libellé d'objet | 20 caractères |
| Drapeaux (total) | 128, dont 32 `local` |
| Titre de l'aventure | 33 caractères |
| Nom d'ennemi (combat) | 20 caractères |
| Saisie clavier (`@ask`) | 40 caractères, `maxlen` réglable |
| Texte d'issue de combat/énigme | 255 caractères |
| Images | une par section, format HIRES 8 Ko |

La limite qui surprend le plus souvent en pratique est celle de la
**section** : une description avec plusieurs variantes conditionnelles
l'atteint plus vite qu'on ne croit, surtout en cumulant du texte et beaucoup de
choix. Le compilateur signale la section fautive nommément ; il suffit
généralement de la scinder en deux, reliées par un choix intermédiaire, ou de
resserrer la prose.

---

Ce document couvre le format en lui-même — chaque élément, sa syntaxe, son
comportement exact. Pour les patrons d'écriture qui en découlent (carrefours,
récompenses différées, pièges à éviter), voir le compagnon de ce guide :
*Écrire une aventure `.adv` — techniques et bonnes pratiques*.
