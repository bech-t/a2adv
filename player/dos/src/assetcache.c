/* assetcache.c -- backend DOS de assetcache.h : no-op complet. Chaque
 * STORYnn.DAT est de toute facon charge integralement en RAM par diskio.c
 * (cf. son entete), et la RAM conventionnelle d'un PC DOS est abondante pour
 * un fichier de 64 Ko max : rien ne justifie un cache separe comme le /RAM
 * ProDOS de l'Apple II (cf. apple2/src/ramdisk.c). Meme choix que le backend
 * Atari ST (cf. atarist/src/assetcache.c), pour la meme raison. */

#include "assetcache.h"

void cache_boot_fill(u8 from, scr_progress_cb cb)
{
    (void)from;
    (void)cb;
}

void cache_prepare(u8 id)
{
    (void)id;
}

const char *cache_story_path(u8 id)
{
    (void)id;
    return 0;
}

const char *cache_asset_path(const char *name)
{
    return name;
}

/* Aucune compression d'image ici : une disquette DOS 720 Ko a assez de marge
 * pour les images de ce projet (cf. Makefile), et le seul codec existant
 * (ZX02, cf. apple2/src/assetcache.c) est ecrit en assembleur 6502,
 * inutilisable sur x86. Toujours -1 : l'appelant retombe alors sur un
 * chargement direct. */
signed char cache_load_compressed(const char *name, void *dst)
{
    (void)name;
    (void)dst;
    return -1;
}
