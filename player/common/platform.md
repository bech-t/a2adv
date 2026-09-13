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
| `HAS_MENU_MUSIC` | Musique de fond au menu titre activée (`smenu.c`) | 1 | 0 (YM2149 pas encore vérifié à l'oreille) |
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

### 4. `scr.h` / `snd.h` — contrats écran et son

Déclarés dans `common/include/`, mais **entièrement implémentés** par
`apple2/src/scr.c`+`snd.c`(+`snd_mb.c`) et `atarist/src/scr.c`+`snd.c` — il
n'existe aucune version commune de ces `.c`, le matériel est trop différent
d'une machine à l'autre pour qu'un seul fichier ait un sens.

Point d'attention pour un nouveau portage : `scr_gfx_mixed()` (image en
haut, texte en bas) doit laisser EXACTEMENT le même nombre de lignes que ce
que `MENU_ROW_*`/`UI_INTRO_HINT_ROW` (platform.h, point 1) supposent — c'est
la source du décalage de +1 entre Apple II et Atari ST (une ligne d'air en
plus sur ST). Écrire `scr_gfx_mixed()` et compter ses lignes AVANT de fixer
ces constantes, pas l'inverse.

`snd.h` déclare aussi `MUS_NONE`/`MUS_TITLE` (identifiants de morceaux) et
`SND_SELECT`/`SND_WIN`/... (`format.h`, effets — ceux-là sont figés par le
compilateur `a2c`, ne pas y toucher). Une machine sans musique de fond fait
de `snd_music()` un no-op et met `HAS_MENU_MUSIC=0`.

### 5. `z2_intro.h` — jingle de démarrage

Le plus petit contrat (une seule fonction, `z2_intro(void)`, bloquant,
~1 s). `apple2/src/z2_intro.s` pilote le haut-parleur en assembleur 6502 ;
`atarist/src/z2_intro.c` est un bouchon vide en attendant un vrai jingle
YM2149. Un nouveau portage peut commencer par un bouchon vide ici sans que
rien d'autre n'en souffre.

## Fichiers volontairement PAS derrière un contrat commun

- **Le Makefile de chaque player** (`apple2/Makefile`, `atarist/Makefile`) :
  chaque toolchain (cc65, m68k-atari-mint-gcc, ...) a ses propres flags,
  adresses de chargement, format de disquette. Le Makefile racine
  (`../../Makefile`) reste lui totalement neutre : il ne compile QUE les
  aventures (`.adv` → `STORYnn.DAT`) via `a2c`, jamais un player.
- **`snd_mb.c/h`** (Apple II) : Mockingboard, une carte son optionnelle sans
  équivalent sur les autres machines visées à ce jour.
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
5. `z2_intro.c` : bouchon vide, à faire vibrer plus tard si souhaité.
6. Le Makefile : copier `atarist/Makefile` (le plus récent, donc le plus à
   jour sur la séparation compilateur/player/disquette) et l'adapter à la
   toolchain cible.
7. `make hosttest` (cf. `apple2/Makefile`) reste le test le plus rapide
   pendant l'écriture : il rejoue une vraie aventure via gcc sur PC, sans
   dépendre d'aucun matériel ni émulateur, et valide tout `common/` d'un
   coup avant même que `scr.c`/`snd.c` existent pour la nouvelle machine.
