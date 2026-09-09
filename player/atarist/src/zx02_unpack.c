/* zx02_unpack.c -- bouchon ST : la vraie decompression (zx02.s, cf. apple2/)
 * est ecrite en assembleur 6502, inutilisable sur 68000. Comme scr_load_hgr
 * (scr.c) echoue toujours pour l'instant, ceci n'est jamais reellement
 * appele -- juste necessaire pour lier simage.c. Un decompresseur ST reel
 * viendra avec le mode graphique (spec-atarist.md §4/§10). */
#include "zx02_getbyte.h"

void zx02_unpack(void *dst)
{
    (void)dst;
}
