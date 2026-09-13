/* diskio.c -- backend Atari ST de diskio.h.
 *
 * `fseek()` de la libc (mintlib, cross-mint-essential) echoue silencieusement
 * sur un fichier ouvert par fopen() sous l'emulation GEMDOS-HDD de Hatari --
 * confirme le 2026-09-09 et isole via smoketest/fseek_test.c (l'appel GEMDOS
 * BRUT Fseek(), lui, fonctionne). Plutot que contourner fseek() a chaque
 * site d'appel (avec le risque de desynchroniser un tampon stdio partage),
 * dio_open() charge le fichier COURANT entierement en memoire : chaque
 * STORYnn.DAT fait au plus DEFAULT_MAX_FILE = 0xFC00 o (~63 Ko, verrouille
 * cote compilateur, cf. a2c/encoder.py), large comme un boisseau dans le Mo
 * de RAM d'un ST -- ce que l'Apple II (64 Ko en tout) ne peut pas se
 * permettre, d'ou le streaming plus prudent du backend apple2/. Une fois le
 * fichier en RAM, "seek" devient un simple index dans un tableau : plus
 * aucun appel a fseek()/ftell() ici. */

#include <string.h>
#include <osbind.h>
#include "diskio.h"

#define FILEBUF_MAX 0xFC00   /* = DEFAULT_MAX_FILE cote compilateur (a2c/encoder.py) */
static u8  filebuf[FILEBUF_MAX];
static u32 filesize;   /* octets reellement charges pour le fichier COURANT */
static u32 fpos;       /* curseur de lecture "virtuel" -- remplace fseek/ftell */

/* Charge integralement `path` dans filebuf[] via GEMDOS brut (Fopen/Fread :
 * fseek()/fopen() de la libc sont evites ici, cf. entete de ce fichier).
 * 0 = ok, -1 = fichier absent (ou trop gros pour FILEBUF_MAX -- ne devrait
 * jamais arriver, cf. DEFAULT_MAX_FILE cote compilateur). */
signed char dio_open(const char *path)
{
    long h, n;
    h = Fopen(path, 0);          /* 0 = lecture seule */
    if (h < 0)
        return -1;
    n = Fread(h, (long)FILEBUF_MAX, filebuf);
    Fclose(h);
    if (n < 0)
        return -1;
    filesize = (u32)n;
    fpos = 0;
    return 0;
}

void dio_close(void)
{
    filesize = 0;
    fpos = 0;
}

signed char dio_seek(u32 pos)
{
    if (pos > filesize)
        return -1;
    fpos = pos;
    return 0;
}

/* Le fichier est deja entierement charge dans filebuf[] : "remplir" ne fait
 * plus qu'un controle de bornes -- garde la meme signature/le meme usage que
 * le backend apple2/ (fichier tronque = erreur), mais il n'y a plus de
 * tampon separe a alimenter. */
signed char dio_fill(u16 n)
{
    if (n == 0 || fpos + (u32)n > filesize)
        return -1;
    return 0;
}

u8 dio_u8(void)
{
    return filebuf[fpos++];
}

signed char dio_read(void *dst, u16 n)
{
    if (fpos + (u32)n > filesize)
        return -1;
    memcpy(dst, filebuf + fpos, n);
    fpos += n;
    return 0;
}
