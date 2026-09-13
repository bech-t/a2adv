# Portage : ce qui est commun, ce qui reste à écrire

Ce dossier (`player/common/`) contient le moteur du player, partagé mot pour
mot entre `player/apple2/` et `player/atarist/`. Ce document explique où
passe la frontière, pour qui voudrait ajouter une troisième machine (C64,
Atari 800XL, PC en EGA/VGA via DOS, ...) sans avoir à relire tout `story.c`
pour le comprendre.

Le principe : **`common/` ne connaît AUCUN nom de plateforme, de fichier
disque, de registre materiel ou de constante liée à une machine precise.**
Chaque fois qu'il a besoin d'une valeur ou d'un comportement qui varie d'une
machine à l'autre, il passe par un des points de portage ci-dessous — jamais
par un `#ifdef PLATFORM_XXX` local.

## Ce qui est déjà commun (rien à écrire pour un nouveau portage)

`common/include/` + `common/src/` : format des données (`format.h`,
`game.h`), analyseur DSL de l'`APP.LNG` (`lang.c/h`), état de la partie
(`state.c/h`), inventaire/flags/stats (`scene.c/h`), combat (`combat.c/h`,
`scombat.c/h`), sauvegarde (`save.c/h`), entrée clavier haut niveau
(`sinput.c/h`), menu + écran Options (`smenu.c/h`), scènes graphiques
(`simage.c/h`), UI générique (`ui.c/h`), le point d'entrée (`main.c`), et
surtout `story.c/h` — le chargement de `STORY.DAT` et le streaming des
sections, le plus gros morceau.

Tout ça compile tel quel pour une nouvelle machine, à condition que le
compilateur cible fournisse un C89/C99 raisonnable et que les points de
portage ci-dessous existent.

## Les points de portage

### 1. `platform.h` (un par machine, même nom, mêmes macros, valeurs différentes)

Pas dans `common/include/` : chaque machine a le SIEN, dans son propre
`src/`, retrouvé via `-I src` dans son Makefile. C'est le seul fichier que
`common/` inclut sans qu'il existe dans `common/`.

| Macro | Sens | Apple II | Atari ST |
|---|---|---|---|
| `IMG_EXT` | Extension des fichiers image (concaténée à la compilation : `"IMG00." IMG_EXT`) | `"HGR"` | `"PI1"` |
| `MENU_ROW_TITLE`, `MENU_ROW_CHOICES`, `MENU_ROW_QUIT` | Lignes du menu semi-graphique (`smenu.c`) | 20/22/23 | 21/23/24 |
| `UI_INTRO_HINT_ROW` | Ligne de l'invite en scène d'intro mixte (`simage.c`) | 23 | 24 |
| `HAS_SCR_FRCLOCK` | `scr_frclock()` existe (compteur VBL brut, cf. `scr.h`) | 0 | 1 |

Pour une nouvelle machine : copier un des deux fichiers, changer les
valeurs. Les deux lignes `MENU_ROW_*`/`UI_INTRO_HINT_ROW` ne sont PAS un
choix esthétique à deviner : elles dépendent de la hauteur exacte que
`scr_gfx_mixed()` réserve entre l'image et le texte sur cette machine (cf.
`scr.c`, point 4) — à mesurer sur cette implémentation-là.

### 2. `diskio.h` — accès bas niveau à un fichier déjà identifié

Aucune notion de cache ici, juste lire des octets à une position donnée.
Contrat complet dans `common/include/diskio.h`.

| Fonction | Rôle |
|---|---|
| `dio_open(path)` | Ouvre `path` (referme silencieusement un fichier déjà ouvert) |
| `dio_close()` | Ferme |
| `dio_seek(pos)` | Position absolue |
| `dio_fill(n)` | Annonce une lecture octet-par-octet à venir : les `n` prochains `dio_u8()` doivent réussir et être rapides |
| `dio_u8()` | Un octet, avance |
| `dio_read(dst, n)` | `n` octets courants → `dst`, avance |

Deux implémentations très différentes existent déjà et servent de modèle :
- `apple2/src/diskio.c` : `fopen`/`fseek`/`fread` (stdio cc65 + ProDOS), avec
  un micro-tampon dans `dio_fill`/`dio_u8` pour économiser le surcoût
  d'appel MLI (cf. le commentaire en tête du fichier — la logique du gain
  n'est pas évidente, à lire avant d'y toucher).
- `atarist/src/diskio.c` : contourne un vrai bug de `fseek()` sous mintlib
  en chargeant le fichier COURANT entièrement en RAM (le ST en a largement
  assez) ; "seek" devient un simple index dans un tableau.

Une machine à lecteur lent et RAM abondante (Atari 800XL avec RAM disque,
un PC DOS avec sa RAM conventionnelle largement suffisante pour les 64 Ko
max d'un `STORYnn.DAT`) recopierait plutôt le modèle Atari ST : `fopen`
standard, pas de bug à contourner, chargement direct sans complication.
Une machine à mémoire très contrainte recopierait plutôt le modèle Apple II.

### 3. `assetcache.h` — cache optionnel, entièrement facultatif

Contrat complet dans `common/include/assetcache.h`. Tout est appelé
INCONDITIONNELLEMENT par `story.c`/`simage.c`/`main.c` : un backend qui n'a
rien à offrir répond juste "pas de cache" partout (cf.
`atarist/src/assetcache.c`, un stub complet).

| Fonction | Rôle | Réel sur | No-op sur |
|---|---|---|---|
| `cache_boot_fill(from, cb)` | Remplit le cache dispo au démarrage, avant `story_open()` | Apple II (`ramdisk.c`, disque RAM ProDOS `/RAM` + `/RAM2`) | Atari ST |
| `cache_prepare(id)` | Avant d'ouvrir le fichier STORY `id` : fenêtre glissante | Apple II | Atari ST |
| `cache_story_path(id)` | Chemin en cache pour le fichier STORY `id`, NULL sinon | Apple II | Atari ST |
| `cache_asset_path(name)` | Chemin en cache pour un asset nommé (image...), `name` inchangé sinon | Apple II | Atari ST |
| `cache_load_compressed(name, dst)` | Charge une variante compressée si elle existe, `-1` sinon | Apple II (ZX02, cf. `tools/zx02/`) | Atari ST |

**Pourquoi cet existe séparément de `diskio.h`** : ce ne sont pas les mêmes
questions. `diskio.h` répond à "comment lire ces octets", `assetcache.h`
répond à "y a-t-il un raccourci pour ne pas avoir à les relire depuis un
support lent". Une machine avec un lecteur lent ET peu de RAM (le motif qui
justifie `ramdisk.c`) écrira ici son propre équivalent ; une machine avec
beaucoup de RAM ou un support déjà rapide répondra juste "non" partout,
comme le ST aujourd'hui.

### 4. `scr.h` — contrat écran

Déclaré dans `common/include/`, mais **entièrement implémenté** par
`apple2/src/scr.c` et `atarist/src/scr.c` — il n'existe aucune version
commune de ce `.c`, le matériel est trop différent d'une machine à l'autre
pour qu'un seul fichier ait un sens.

Point d'attention pour un nouveau portage : `scr_gfx_mixed()` (image en
haut, texte en bas) doit laisser EXACTEMENT le même nombre de lignes que ce
que `MENU_ROW_*`/`UI_INTRO_HINT_ROW` (platform.h, point 1) supposent — c'est
la source du décalage de +1 entre Apple II et Atari ST (une ligne d'air en
plus sur ST). Écrire `scr_gfx_mixed()` et compter ses lignes AVANT de fixer
ces constantes, pas l'inverse.

### 5. `snd.h` — contrat son : trois choses, rien d'autre

Déclaré dans `common/include/`, entièrement implémenté par
`apple2/src/snd.c` et `atarist/src/snd.c`. Volontairement réduit à ce que le
moteur a vraiment besoin de demander — **aucune notion de backend, de carte
ou de slot n'y apparaît** :

| Fonction | Rôle | Apple II | Atari ST |
|---|---|---|---|
| `snd_intro()` | Jingle de démarrage, bloquant (~1 s) | Réel (assembleur, `snd_intro.s`) | Bouchon vide (pas encore écrit) |
| `snd_menu_music(on)` | Musique du menu titre, on/off, non bloquante | No-op (haut-parleur 1 bit incapable de fond sonore) | Réel, mais gardé désactivé en interne (`MENU_MUSIC_VERIFIED` dans `snd.c` — jamais vérifié à l'oreille) |
| `snd_play(id)` | Effet prédéfini (`SND_SELECT`, `SND_WIN`, ... — `format.h`, figés par `a2c`, ne pas y toucher) | Réel (haut-parleur) | Réel (YM2149) |

`main.c`/`smenu.c` (communs) appellent les trois INCONDITIONNELLEMENT,
exactement comme pour `assetcache.h` (point 3) : une plateforme qui n'a rien
à offrir répond par un no-op, jamais par un test côté appelant. Le choix
"cette plateforme joue-t-elle vraiment cette musique aujourd'hui" (par
opposition à "en est-elle matériellement capable") reste une décision
interne au `snd.c` de cette plateforme, cf. l'exemple `MENU_MUSIC_VERIFIED`
côté Atari ST — ça n'a pas sa place dans `platform.h`, qui ne décrit que des
faits durables de la machine, pas un état d'avancement du portage.

### 6. `sysinfo.h` — info système (écran Options, diagnostic)

Déclaré dans `common/include/`, entièrement implémenté par
`apple2/src/sysinfo.c` et `atarist/src/sysinfo.c` : une seule fonction,
`sys_info()`, qui écrit quelques lignes (modèle, mode écran, mémoire...) via
`scr_puts()`/`ui_newline()` — le format exact et le nombre de faits affichés
sont laissés à chaque plateforme, `smenu.c` (commun) ne fait qu'ajouter le
titre et le retour autour (cf. `run_sys_info`).

Choisir QUOI afficher est le vrai travail ici : ne montrer que des faits
qu'une API standard et documentée donne de façon fiable (modèle machine,
colonnes/résolution actives, mémoire libre annoncée par l'OS) — pas une
détection matérielle maison non vérifiée (ex. taille RAM totale sur Apple
II, hors de portée sans sonder la mémoire directement). Un fait dont
l'affichage n'a pas encore été rejoué sur émulateur/matériel réel doit le
dire dans un commentaire (cf. `atarist/src/sysinfo.c`), jamais prétendre
être vérifié sans l'avoir été.

## Fichiers volontairement PAS derrière un contrat commun

- **Le Makefile de chaque player** (`apple2/Makefile`, `atarist/Makefile`) :
  chaque toolchain (cc65, m68k-atari-mint-gcc, ...) a ses propres flags,
  adresses de chargement, format de disquette. Le Makefile racine
  (`../../Makefile`) reste lui totalement neutre : il ne compile QUE les
  aventures (`.adv` → `STORYnn.DAT`) via `a2c`, jamais un player.
- **ZX02** (`apple2/src/zx02.s` + `zx02_getbyte.c/h`) : compression d'image,
  utile uniquement parce que la disquette ProDOS fait 140 Ko. Rangée
  derrière `cache_load_compressed()` (point 3), donc invisible de
  `common/` — un nouveau portage n'a besoin de s'en soucier QUE si son
  support de stockage est aussi serré que celui de l'Apple II.

## Pour ajouter une machine : ordre suggéré

1. `platform.h` avec des valeurs plausibles (les lignes de menu se
   corrigeront après coup, cf. point 4).
2. `scr.c`/`snd.c` (les vrais pilotes matériel) — le plus gros travail,
   sans lien avec ce document.
3. `diskio.c` : le plus simple qui marche (streaming direct si le CPU/OS le
   permet) avant d'optimiser.
4. `assetcache.c` : un stub complet (tout renvoie "pas de cache") suffit
   pour démarrer — cf. `atarist/src/assetcache.c` comme modèle minimal.
5. `snd_intro()`/`snd_menu_music()` : des bouchons vides suffisent pour
   démarrer (cf. `atarist/src/snd.c` avant que son jingle soit écrit) ;
   `snd_play()` seul doit faire quelque chose dès le début, les effets
   sonores étant beaucoup plus présents dans le jeu qu'intro/musique.
6. Le Makefile : copier `atarist/Makefile` (le plus récent, donc le plus à
   jour sur la séparation compilateur/player/disquette) et l'adapter à la
   toolchain cible.
7. `make hosttest` (cf. `apple2/Makefile`) reste le test le plus rapide
   pendant l'écriture : il rejoue une vraie aventure via gcc sur PC, sans
   dépendre d'aucun matériel ni émulateur, et valide tout `common/` d'un
   coup avant même que `scr.c`/`snd.c` existent pour la nouvelle machine.
