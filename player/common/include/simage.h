/* simage.h -- scenes graphiques : splash, image plein ecran, image d'intro. */
#ifndef A2ADV_SIMAGE_H
#define A2ADV_SIMAGE_H

#include "format.h"
#include "scene.h"

/* Charge une image par son nom, via le cache d'assets (cf. assetcache.h).
 * Renvoie comme scr_load_hgr : 0 si l'image est chargee. */
signed char img_load(const char *name);

/* Charge `hgr_name` (ex: "IMG00.HGR") SANS passer par le cache d'assets :
 * prefere une eventuelle variante compressee (cf.
 * assetcache.h:cache_load_compressed -- reelle sur Apple II via ZX02, sans
 * objet sur ST) si elle existe, sinon lit `hgr_name` telle quelle. Utilisee
 * par img_load (repli quand rien n'est en cache) et par cache_boot_fill
 * (cf. apple2/src/ramdisk.c, pour remplir precisement ce cache). */
signed char img_load_from_disk(const char *hgr_name);

/* Affiche l'image d'un asset puis revient en texte.
 * secs = 0 : attend une touche ; sinon ~secs secondes, une touche passe.
 * Image introuvable : rien ne s'affiche. Renvoie la touche. */
char show_image(u16 asset, u8 secs);

/* @splash de la section `idx` : image plein ecran AVANT le texte, facultative.
 * secs = 0 : attend une touche ; sinon ~secs secondes, une touche passe.
 * Sans `always`, l'image n'est pas rejouee quand on revient dans la section
 * dont on vient de la montrer (carrefour). Image absente de la disquette :
 * rien ne s'affiche, la section commence directement par son texte.
 * splash_reset : au debut d'une partie. */
void show_splash(u16 idx, const SecHeader *h);
void splash_reset(void);

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
