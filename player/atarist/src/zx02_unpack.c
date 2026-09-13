/* zx02_unpack.c -- bouchon ST : la vraie decompression (zx02.s, cf. apple2/)
 * est ecrite en assembleur 6502, inutilisable sur 68000. scr_load_hgr
 * (scr.c) sait desormais charger une image non compressee ; ce bouchon
 * n'est donc appele que si un ".ZX2" compresse existe sur la disquette
 * (img_load_from_disk, cf. simage.c) -- rien ne produit encore de ".ZX2"
 * cote ST (le pipeline de compression, tools/zx02/, est specifique a
 * l'Apple II), donc ce chemin n'est jamais emprunte en pratique aujourd'hui.
 * Juste necessaire pour lier simage.c ; un vrai decompresseur ST reste a
 * ecrire si la compression devient utile sur cette plateforme. */
#include "zx02_getbyte.h"

void zx02_unpack(void *dst)
{
    (void)dst;
}
