/* assetcache.h -- cache optionnel des assets du jeu (STORYnn.DAT, images) sur
 * un support plus rapide que celui d'origine, un backend par plateforme (cf.
 * player/apple2/src/assetcache.c, player/atarist/src/assetcache.c).
 *
 * Sur Apple II, implemente via le disque RAM ProDOS /RAM (cf.
 * apple2/src/ramdisk.c, PRIVE a ce backend -- ramdisk.h n'est inclus par
 * aucun fichier commun). Sur Atari ST, no-op complet : chaque STORYnn.DAT est
 * de toute facon charge integralement en RAM par diskio.c, la RAM y est
 * abondante et rien ne justifie un cache separe.
 *
 * story.c/simage.c/main.c (communs) appellent ces fonctions
 * INCONDITIONNELLEMENT, sans jamais savoir si un cache existe reellement
 * derriere -- le meme motif (stockage lent + RAM disponible pour la copier)
 * se retrouvera vraisemblablement sur un futur portage 8 bits a lecteur lent
 * (C64, Atari 800XL) : ce fichier est le point d'entree ou brancher son
 * equivalent de ramdisk.c le jour venu. */
#ifndef A2ADV_ASSETCACHE_H
#define A2ADV_ASSETCACHE_H

#include "format.h"
#include "scr.h"

/* A appeler une fois au demarrage, AVANT story_open() : remplit le cache
 * disponible (STORYnn.DAT a partir de 'from', puis les images), au mieux.
 * No-op si le backend n'a pas de cache. */
void cache_boot_fill(u8 from, scr_progress_cb cb);

/* A appeler avant d'ouvrir le fichier STORY 'id' (cf. story.c:open_file) :
 * au backend de preparer son cache (fenetre glissante...). No-op sinon. */
void cache_prepare(u8 id);

/* Chemin en cache pour le fichier STORY 'id', NULL si absent du cache (ou
 * si le backend n'en a pas) -- l'appelant retombe alors sur son chemin
 * disque habituel. */
const char *cache_story_path(u8 id);

/* Chemin a ouvrir pour l'asset nomme 'name' (une image...) : sa copie en
 * cache si elle existe, sinon `name` inchange (comparaison de pointeur :
 * l'appelant sait ainsi s'il a eu le cache ou non, cf. simage.c). */
const char *cache_asset_path(const char *name);

/* Charge dans `dst` une eventuelle variante COMPRESSEE de l'image `name`
 * (meme base, extension et codec au choix du backend), SANS passer par le
 * cache ci-dessus. 0 = chargee, -1 = pas de variante compressee (fichier
 * absent, ou plateforme sans compression -- cf. apple2/src/assetcache.c
 * pour la seule implementation reelle a ce jour, via ZX02 : cf. son
 * tools/zx02/, specifique a la disquette 140 Ko de l'Apple II) ;
 * l'appelant retombe alors sur un chargement direct non compresse. */
signed char cache_load_compressed(const char *name, void *dst);

#endif /* A2ADV_ASSETCACHE_H */
