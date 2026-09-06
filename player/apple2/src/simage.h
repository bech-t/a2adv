/* simage.h -- scenes graphiques : splash, image plein ecran, image d'intro. */
#ifndef A2ADV_SIMAGE_H
#define A2ADV_SIMAGE_H

#include "format.h"

/* Charge une image par son nom, via le cache en disque RAM (cf. ramdisk.h).
 * Renvoie comme scr_load_hgr : 0 si l'image est en page HIRES. */
signed char img_load(const char *name);

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
