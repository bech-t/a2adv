/* diskio.c -- backend DOS de diskio.h.
 *
 * Meme modele que le backend Atari ST (cf. atarist/src/diskio.c) : dio_open()
 * charge le fichier COURANT integralement en RAM plutot que de le relire par
 * blocs. Sur ST c'etait pour contourner un fseek() casse sous Hatari ; ici la
 * raison est differente mais la conclusion identique (cf. player/common/
 * platform.md, point 2) -- un PC DOS a largement assez de RAM conventionnelle
 * pour un fichier de 64 Ko max (DEFAULT_MAX_FILE, verrouille cote
 * compilateur, cf. a2c/encoder.py), et un lecteur de disquette reel est lent
 * a chaque acces (temps de positionnement de tete) : autant lire le fichier
 * en une seule fois plutot qu'a coups de petites lectures eparpillees par
 * story.c. fopen()/fread() d'Open Watcom n'ont ici aucun bug connu a
 * contourner (a la difference de mintlib/ST) ; le choix est une question de
 * performance, pas de correction. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diskio.h"

/* FILEBUF_MAX (62,5 Ko) depasse a lui seul ce qu'un segment de donnees "near"
 * (modele SMALL, 64 Ko DGROUP PARTAGES avec tout le reste des variables
 * globales du moteur) peut contenir -- ce joueur est donc compile en modele
 * COMPACT (-mc : code near, donnees FAR, cf. Makefile) plutot que SMALL
 * comme un player DOS minimal l'aurait fait naturellement. malloc() y rend
 * directement un pointeur FAR, capable d'adresser ce bloc a lui seul : rien
 * de plus a faire cote appelant (fread/memcpy suivent le meme pointeur). */
#define FILEBUF_MAX 0xFC00UL   /* = DEFAULT_MAX_FILE cote compilateur (a2c/encoder.py) */
static unsigned char *filebuf;   /* alloue une seule fois, cf. dio_open */
static u32 filesize;   /* octets reellement charges pour le fichier COURANT */
static u32 fpos;       /* curseur de lecture "virtuel" -- pas de fseek/ftell ici */

signed char dio_open(const char *path)
{
    FILE *f;
    size_t n;

    if (filebuf == NULL) {
        filebuf = malloc(FILEBUF_MAX);
        if (filebuf == NULL)
            return -1;
    }
    f = fopen(path, "rb");
    if (f == NULL)
        return -1;
    n = fread(filebuf, 1, FILEBUF_MAX, f);
    fclose(f);
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
 * plus qu'un controle de bornes, cf. atarist/src/diskio.c:dio_fill. */
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
