# a2adv — des livres-jeu qui tournent sur un vrai Apple II

**a2adv** est une chaîne d'outils complète pour écrire des livres dont vous êtes
le héros et les faire tourner sur un Apple II de 1979 — pas sur un émulateur qui
fait semblant, mais sur une **disquette ProDOS bootable** qu'une machine
d'époque avale sans broncher.

Vous écrivez l'aventure dans un format texte lisible, sur un ordinateur moderne.
Un compilateur la transforme en données binaires compactes. Un moteur écrit en C
et en assembleur 6502 les lit et joue l'histoire.

```
    aventure.adv          a2c (Python)          STORYnn.DAT        player 6502
   ┌──────────────┐      ┌────────────┐       ┌─────────────┐     ┌──────────┐
   │  texte, choix │ ───▶ │ compilateur│ ───▶  │  binaire    │ ──▶ │ Apple II │
   │  conditions   │      │  + QA      │       │  + images   │     │  ProDOS  │
   └──────────────┘      └────────────┘       └─────────────┘     └──────────┘
```

---

## Pourquoi

Le livre-jeu et l'Apple II sont contemporains, et pourtant écrire l'un pour
l'autre a toujours été pénible : on tapait du BASIC, on comptait les octets à la
main, et changer une phrase voulait dire renuméroter des paragraphes.

a2adv part de l'idée inverse : **l'auteur écrit du texte, la machine s'occupe des
octets.** Les contraintes de 1979 sont respectées à la lettre — mais elles sont
absorbées par le compilateur, pas subies par l'auteur.

## Les objectifs qui gouvernent le projet

**Compatibilité maximale.** Le moteur vise le **6502** et un budget de **64 Ko de
RAM principale** : il tourne sur un Apple II tout court, pas seulement sur un
//e. Les capacités supplémentaires — 80 colonnes, carte son Mockingboard,
disque RAM sur 128 Ko — sont **détectées à l'exécution** et exploitées si
présentes, jamais exigées.

**Pas de limite de taille d'aventure.** Le moteur ne charge jamais l'histoire
entière : chaque section est lue à la demande grâce à un index. L'aventure est
bornée par la capacité de la disquette, pas par la RAM.

**Le moteur ne contient aucun texte de langue.** Menus, invites, messages de fin,
libellés d'inventaire : tout vient de l'aventure. Traduire un jeu, c'est éditer
son fichier source — sans recompiler le moteur.

**Rien à la volée.** Toute stat, tout objet, tout drapeau doit être déclaré. Le
compilateur refuse une faute de frappe plutôt que de créer silencieusement une
variable — et l'ordre figé des déclarations rend les sauvegardes stables.

**Se tromper à la compilation, pas devant le joueur.** Un analyseur détecte les
sections inatteignables, les culs-de-sac, les fins injoignables, les objets
requis mais jamais donnés.

## Ce qui marche aujourd'hui

- **Narration** : sections, choix conditionnels, effets, drapeaux, objets,
  caractéristiques bornées, score et compteur de mouvements.
- **Combat** au tour par tour (2d6 + stat, armure, modificateurs d'équipement,
  fuite) avec branchements victoire / défaite / fuite.
- **Énigmes à saisie** : le joueur tape un mot ou un code, comparé à plusieurs
  réponses acceptées.
- **Affichage** : pilote texte maison 40/80 colonnes (le 80 colonnes passe par la
  mémoire auxiliaire du //e), images HIRES plein écran ou en mode mixte,
  paragraphes justifiés à la largeur réelle, styles centré et inversé.
- **Son** : haut-parleur 1 bit, et **Mockingboard** en option avec détection
  automatique au démarrage.
- **Performance** : cache des données d'histoire dans le **disque RAM `/RAM`**
  sur machine 128 Ko, avec fenêtre glissante sur les chapitres.
- **Sauvegarde** sur disquette (un emplacement).
- **Découpage en chapitres** : chaque chapitre devient son propre fichier, ce qui
  permet des aventures de taille arbitraire.

Non fait à ce jour : sauvegardes multi-emplacements, échange de disquettes réel,
éditeur visuel.

> **Rien de tout ceci n'a encore été validé sur matériel réel.** Le moteur
> compile, la disquette se fabrique, les tests hôtes passent — mais le passage
> sur émulateur puis sur machine physique reste à faire.

## Démarrage rapide

Il vous faut **cc65** (compilateur 6502), **Python 3** et **Java** (pour
AppleCommander, qui fabrique la disquette).

```bash
# fabriquer la disquette d'une aventure
cd player/apple2
make dsk ADV=homme_costume_blanc
# -> adventures/homme_costume_blanc/build/homme_costume_blanc.dsk
```

Bootez le `.dsk` obtenu dans n'importe quel émulateur Apple II gérant ProDOS.

```bash
# compiler seulement, avec un résumé de l'aventure
cd compiler
python3 -m a2c ../adventures/homme_costume_blanc/homme_costume_blanc.adv -o /tmp/x --summary

# contrôle qualité : sections inatteignables, culs-de-sac, objets morts
python3 -m a2c.analyze ../adventures/homme_costume_blanc/homme_costume_blanc.adv

# rejouer le cœur du moteur sur PC, sans émulateur
cd player/apple2 && make hosttest
```

## Organisation du dépôt

| Dossier | Contenu |
|---|---|
| `compiler/` | `a2c`, le compilateur Python — parseur, validation, encodeur, analyseur QA. Aucune dépendance externe. |
| `player/apple2/` | Le moteur cc65 : pilote écran, streaming, combat, saisie, son, cache `/RAM`. |
| `adventures/` | Une aventure par dossier : source `.adv`, images, disquette produite. |
| `editor/` | Éditeur visuel — prévu bien plus tard, vide aujourd'hui. |
| `spec.md` | La spécification technique complète : format binaire, carte mémoire, décisions d'architecture. |

## Écrire une aventure

Le format source est décrit dans **[FORMAT-AVENTURE.md](FORMAT-AVENTURE.md)** —
il s'apprend en une demi-heure et tient sur une page.

Un exemple complet et jouable : **[L'Homme en Costume
Blanc](adventures/homme_costume_blanc/)**, 60 sections, quatre chapitres.

## Licence

Le code (compilateur `a2c` et player `player/apple2/`) est sous licence
**[MIT](LICENSE)**.

Les aventures livrées dans `adventures/` portent leurs propres licences de
contenu — voir chacune. L'adaptation de *L'Homme en Costume Blanc* est sous
**CC BY-SA 4.0**.
