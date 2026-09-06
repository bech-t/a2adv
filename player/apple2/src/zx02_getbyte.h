/* zx02_getbyte.h -- lecture bufferisee du flux compresse (cf. zx02_getbyte.c). */
#ifndef A2ADV_ZX02_GETBYTE_H
#define A2ADV_ZX02_GETBYTE_H

#include <stdio.h>
#include "format.h"

/* Prepare la lecture d'un nouveau flux compresse depuis `f` (deja ouvert,
 * position de lecture au debut du flux). A appeler avant zx02_unpack. */
void zx_getbyte_init(FILE *f);

/* Rend l'octet compresse suivant. Sert de source a zx02.s (jsr _zx_getbyte,
 * valeur rendue en A) -- pas d'usage direct cote C attendu. */
u8 zx_getbyte(void);

/* Decompresse le flux prepare par zx_getbyte_init, ecrit le resultat a
 * partir de `dst` (implementee en assembleur, cf. zx02.s). */
void zx02_unpack(void *dst);

#endif /* A2ADV_ZX02_GETBYTE_H */
