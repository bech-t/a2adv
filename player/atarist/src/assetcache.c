/* assetcache.c -- backend Atari ST de assetcache.h : no-op complet. Chaque
 * STORYnn.DAT est de toute facon charge integralement en RAM par diskio.c,
 * et la RAM est abondante sur ST -- rien ne justifie un cache separe comme
 * le /RAM ProDOS de l'Apple II (cf. apple2/src/ramdisk.c). */

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
