/* assetcache.c -- backend Apple II de assetcache.h : de simples redirections
 * vers ramdisk.c (cache disque RAM ProDOS /RAM), qui reste sinon prive a ce
 * dossier -- aucun fichier commun n'inclut ramdisk.h. Implemente
 * aussi cache_load_compressed() via ZX02 (zx02_getbyte.c + zx02.s), tout
 * aussi prive : aucun fichier commun ne connait ".ZX2" ni zx02_getbyte.h. */

#include <stdio.h>
#include "assetcache.h"
#include "ramdisk.h"
#include "zx02_getbyte.h"

void cache_boot_fill(u8 from, scr_progress_cb cb)
{
    ram_boot_fill(from, cb);
}

void cache_prepare(u8 id)
{
    ram_ensure(id);
}

const char *cache_story_path(u8 id)
{
    return ram_has(id) ? ram_path(id) : 0;
}

const char *cache_asset_path(const char *name)
{
    return ram_file_path(name);
}

/* "<meme base>.ZX2" (produit par le Makefile, cf. tools/zx02/pack_images.py)
 * si elle existe, decompresse EN FLUX dans dst : zx_getbyte lit le fichier
 * par blocs de 256 o (cf. zx02_getbyte.c), le flux compresse entier n'a donc
 * jamais besoin de tenir en RAM, quelle que soit la taille de l'image. */
signed char cache_load_compressed(const char *name, void *dst)
{
    char zx_name[11];        /* "BOOT00.ZX2" (le plus long) + NUL = 11 o */
    FILE *f;
    u8 i;

    for (i = 0; name[i] != '\0' && name[i] != '.' && i < 6; ++i)
        zx_name[i] = name[i];
    zx_name[i++] = '.'; zx_name[i++] = 'Z'; zx_name[i++] = 'X'; zx_name[i++] = '2';
    zx_name[i] = '\0';

    f = fopen(zx_name, "rb");
    if (f == NULL)
        return -1;
    zx_getbyte_init(f);
    zx02_unpack(dst);
    fclose(f);
    return 0;
}
