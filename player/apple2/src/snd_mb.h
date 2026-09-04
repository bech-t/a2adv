/* snd_mb.h -- son Mockingboard (2x AY-3-8910 via 6522), backend optionnel.
 *
 * ATTENTION : code materiel NON teste ici (necessite un emulateur qui emule la
 * Mockingboard : AppleWin, MAME, Virtual II). L'API reste neutre sur l'hote.
 *
 * Meme jeu d'effets que snd.c (SND_SELECT, SND_WIN, ...). Le backend se choisit
 * a la MAIN dans le menu Options, via snd_use_mockingboard(slot) (cf. snd.h) :
 * il n'y a pas de detection automatique. */
#ifndef A2ADV_SND_MB_H
#define A2ADV_SND_MB_H

#include "format.h"

/* Initialise la carte au slot donne (1..7) : ports en sortie, AY reset, silence. */
void mb_init(u8 slot);

/* Joue un effet predefini (memes ids que snd_play : SND_*). */
void mb_play(u8 id);

#endif /* A2ADV_SND_MB_H */
