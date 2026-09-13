/* zx02_getbyte.c -- lecture bufferisee du flux compresse pour zx02.s.
 *
 * zx02.s ne suppose plus que le flux compresse tient en RAM (cf. son en-tete :
 * la decompression EN PLACE a ete essayee puis abandonnee, verifiee corrompue
 * des que la sortie est nettement plus grosse que l'entree -- le cas normal
 * ici). A la place, chaque octet compresse est demande un par un via
 * zx_getbyte(), qui les sert depuis un petit tampon, lui-meme rempli par
 * blocs de ZX_CHUNK octets depuis le fichier ProDOS ouvert. Le fichier
 * compresse entier n'a donc jamais besoin de tenir en memoire -- seul ce
 * petit tampon, quelle que soit la taille de l'image.
 */

#include "zx02_getbyte.h"

#define ZX_CHUNK 256

static u8    zxbuf[ZX_CHUNK];
static u16   zx_pos, zx_len;
static FILE *zx_file;

void zx_getbyte_init(FILE *f)
{
    zx_file = f;
    zx_pos  = 0;
    zx_len  = 0;
}

u8 zx_getbyte(void)
{
    if (zx_pos >= zx_len) {
        zx_len = (u16)fread(zxbuf, 1, ZX_CHUNK, zx_file);
        zx_pos = 0;
        if (zx_len == 0)
            return 0;   /* fin de fichier inattendue : ne doit jamais arriver
                         * avec un flux ZX0 bien forme, qui se termine par sa
                         * propre marque de fin AVANT d'epuiser le fichier.
                         * Filet de securite plutot que plantage. */
    }
    return zxbuf[zx_pos++];
}

#ifndef __CC65__
/* Sur cc65 (cible reelle), la decompression est en assembleur (zx02.s, non
 * lie ici pour eviter un symbole en double). Bouchon pour les tests hote
 * (cf. player/apple2/Makefile:hosttest) : hosttest ne charge aucune image
 * (coeur narratif/combat seulement), donc cache_load_compressed() -- la
 * seule appelante -- n'est jamais REELLEMENT invoquee ; ce bouchon n'existe
 * que pour satisfaire l'edition de liens. */
void zx02_unpack(void *dst)
{
    (void)dst;
}
#endif
