/* game.h -- constantes partagees entre les modules du player. */
#ifndef A2ADV_GAME_H
#define A2ADV_GAME_H

#include "format.h"

#define STORY_FILE "STORY00.DAT"    /* fichier 0 de l'aventure (multi-fichiers) */
#define GAME_OVER  0xFFFF           /* fin d'aventure OU retour au menu */
#define SPLASH_SECS 3               /* duree d'un splash (secondes) */

/* Touches speciales (hors 1..9 et I/S/L/Q gerees par ui_read_choice). */
#define KEY_ESC   0x1B
#define KEY_ENTER 0x0D
#define KEY_SPACE 0x20

/* Actions du menu principal. */
#define ACT_NEW   0
#define ACT_LOAD  1
#define ACT_QUIT  2

#endif /* A2ADV_GAME_H */
