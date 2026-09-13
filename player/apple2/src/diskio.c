/* diskio.c -- backend Apple II de diskio.h (stdio cc65 + ProDOS).
 *
 * ATTENTION a ne pas se tromper de gain avec le micro-tampon dio_fill/dio_u8 :
 * ProDOS met DEJA en cache le bloc courant de 512 o dans le tampon de 1 Ko de
 * chaque fichier ouvert. Le manuel technique est explicite : "Neither a READ
 * nor a WRITE call necessarily causes a disk access. It is only when a read
 * or write crosses a 512-byte (block) boundary that a disk access occurs."
 * Lire le preambule octet par octet ne coute donc PAS des centaines d'acces
 * disque : il y en a autant dans les deux cas (un par bloc traverse).
 *
 * Ce qu'on economise, c'est le SURCOUT D'APPEL : la stdio de cc65 n'etant pas
 * tamponnee, chaque fgetc() descend en read() puis en appel MLI (bloc de
 * parametres, JSR $BF00, dispatch, commutation de banque) meme quand la
 * donnee sort du cache ProDOS. ~900 appels au demarrage, a quelques centaines
 * de cycles piece, valent de l'ordre de 0,2 a 0,4 s a 1 MHz.
 *
 * On charge donc d'un bloc dans secbuf — libre a ces moments-la — et dio_u8()
 * y puise tant qu'il reste quelque chose. Repli automatique sur la lecture
 * directe si le bloc ne tient pas dans secbuf. */

#include <stdio.h>
#include "diskio.h"
#include "story.h"   /* secbuf, SECTION_MAX */

static FILE *fp;
static u16   pbuf_pos;
static u16   pbuf_left;

signed char dio_open(const char *path)
{
    dio_close();
    fp = fopen(path, "rb");
    return fp ? 0 : -1;
}

void dio_close(void)
{
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
    pbuf_left = 0;
}

signed char dio_seek(u32 pos)
{
    pbuf_left = 0;
    return (fseek(fp, (long)pos, SEEK_SET) == 0) ? 0 : -1;
}

/* Charge n octets a la position courante du fichier dans secbuf. 0 = ok (les
 * lectures suivantes viennent du tampon), -1 = trop gros ou lecture courte. */
signed char dio_fill(u16 n)
{
    pbuf_pos = 0;
    pbuf_left = 0;
    if (n == 0 || n > SECTION_MAX)
        return -1;
    if (fread(secbuf, 1, n, fp) != n)
        return -1;
    pbuf_left = n;
    return 0;
}

u8 dio_u8(void)
{
    if (pbuf_left) {
        --pbuf_left;
        return secbuf[pbuf_pos++];
    }
    return (u8)fgetc(fp);
}

signed char dio_read(void *dst, u16 n)
{
    pbuf_left = 0;
    return (fread(dst, 1, n, fp) == n) ? 0 : -1;
}
