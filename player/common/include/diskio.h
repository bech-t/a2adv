/* diskio.h -- acces bas niveau a un fichier ouvert, un backend par
 * plateforme (cf. player/apple2/src/diskio.c, player/atarist/src/diskio.c).
 * story.c (commun) ignore tout du support reel : stdio+ProDOS cote Apple II,
 * chargement pleine RAM cote Atari ST (mintlib y a un fseek() casse sous
 * l'emulation GEMDOS-HDD de Hatari -- confirme le 2026-09-09).
 *
 * Aucune notion de cache ici (cf. assetcache.h pour ca) : ce header ne fait
 * que lire des octets a une position donnee d'un fichier DEJA identifie par
 * son chemin.
 *
 * Contrat d'usage (respecte par story.c) :
 *   - dio_open() referme silencieusement un fichier deja ouvert ;
 *   - dio_fill(n) annonce une lecture octet-par-octet a venir : les n
 *     prochains dio_u8() doivent reussir et etre rapides (implementation
 *     libre : tampon reel cote Apple II, simple verification de bornes cote
 *     ST ou tout est deja en RAM) ;
 *   - dio_seek()/dio_read() invalident tout tampon prepare par dio_fill(). */
#ifndef A2ADV_DISKIO_H
#define A2ADV_DISKIO_H

#include "format.h"

signed char dio_open(const char *path);   /* 0 = ok, -1 = absent/erreur */
void        dio_close(void);
signed char dio_seek(u32 pos);            /* position absolue, 0 = ok */
signed char dio_fill(u16 n);              /* prepare n octets pour dio_u8() */
u8          dio_u8(void);                 /* 1 octet, avance le curseur */
signed char dio_read(void *dst, u16 n);   /* n octets courants -> dst, avance */

#endif /* A2ADV_DISKIO_H */
