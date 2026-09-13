/* assetcache.c -- backend Apple II de assetcache.h : de simples redirections
 * vers ramdisk.c (cache disque RAM ProDOS /RAM et /RAM2), qui reste sinon
 * prive a ce dossier -- aucun fichier commun n'inclut ramdisk.h. */

#include "assetcache.h"
#include "ramdisk.h"

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
