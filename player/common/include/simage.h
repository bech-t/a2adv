/* simage.h -- scenes graphiques : splash, image plein ecran, image d'intro. */
#ifndef A2ADV_SIMAGE_H
#define A2ADV_SIMAGE_H

#include "format.h"

/* Charge une image par son nom, via le cache d'assets (cf. assetcache.h).
 * Renvoie comme scr_load_hgr : 0 si l'image est chargee. */
signed char img_load(const char *name);

/* Charge `hgr_name` (ex: "IMG00.HGR") SANS passer par le cache d'assets :
 * sur Apple II (HAS_ZX02, cf. platform.h), prefere "<meme base>.ZX2" sur la
 * disquette (decompression en flux, cf. apple2/src/zx02_getbyte.c +
 * zx02.s) si elle existe, sinon lit `hgr_name` telle quelle ; sur une
 * plateforme sans ZX02 (Atari ST : la disquette a assez de place, cf.
 * tools/zx02/ specifique Apple II), lit toujours `hgr_name` directement.
 * Utilisee par img_load (repli quand rien n'est en cache) et par
 * cache_boot_fill (cf. apple2/src/ramdisk.c, pour remplir precisement ce
 * cache). */
signed char img_load_from_disk(const char *hgr_name);

/* Affiche l'image d'un asset puis revient en texte.
 * timed=1 : ~3 s ou touche ; timed=0 : attend une touche. Renvoie la touche. */
char show_image(u16 asset, u8 timed);

/* Scene d'intro AVEC image : mode mixte (image + fenetre texte), invite en bas.
 * Rend les paragraphes de la section (curseur deja sur le bloc texte). */
char show_intro_image(u16 asset);

/* Charge l'image d'un asset en fond (pour le mode mixte) : ferme STORY,
 * charge le HGR, rouvre STORY. L'affichage mixte est fait par l'appelant. */
void load_scene_image(u16 asset);

/* Splash BOOTxx.HGR au lancement (laisse le dernier a l'ecran). */
void run_splashes(void);

/* Attend ~secs s (compte les trames VBL) ou une touche ; renvoie la touche. */
char wait_or_key(u8 secs);

#endif /* A2ADV_SIMAGE_H */
