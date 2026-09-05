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
