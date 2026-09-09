/* snd.h -- son via le haut-parleur 1 bit ($C030). Monophonique et bloquant
 * (convient a un livre-jeu au tour par tour : sons courts entre deux ecrans). */
#ifndef A2ADV_SND_H
#define A2ADV_SND_H

#include "format.h"

/* Un ton carre : pitch = demi-periode (grand = grave), dur = nombre de bascules. */
void snd_tone(u8 pitch, u16 dur);

/* Joue un effet predefini (SND_SELECT, SND_WIN, ...). */
void snd_play(u8 id);

/* Backend son : 0 = haut-parleur (defaut), 1 = Mockingboard. */
extern u8 snd_backend;

/* Slot Mockingboard actif (1..7), ou 0 si haut-parleur. Informatif (menu Options). */
extern u8 snd_mb_slot;

/* Active la Mockingboard au slot 1..7 (et l'initialise) ; slot 0 = repli
 * haut-parleur. Pilote par le menu Options : le choix est MANUEL.
 *
 * Pas de detection automatique au boot : un balayage ecrirait dans sept slots
 * dont on ignore le contenu, et le joueur sait mieux que nous ce qu'il a dans
 * ses machines.
 *
 * Le slot choisi est en revanche VERIFIE avant usage (mb_probe) : deux 6522
 * doivent repondre, en $Cn00 et $Cn80. Si non, on retombe silencieusement sur
 * le haut-parleur -- le menu Options continue d'afficher "HAUT-PARLEUR", ce
 * qui suffit a dire au joueur que son slot n'a pas ete accepte. */
void snd_use_mockingboard(u8 slot);

/* --- Musique de fond ----------------------------------------------------- */
/* Reservee a la Mockingboard : au haut-parleur 1 bit, une musique de fond
 * monopoliserait le processeur et figerait le jeu. Ces appels sont donc des
 * no-op tant que le backend est le haut-parleur -- les appelants n'ont aucun
 * test a faire. */
#define MUS_NONE   0
#define MUS_TITLE  1

/* Lance un morceau en boucle, ou l'arrete avec MUS_NONE. */
void snd_music(u8 id);

#endif /* A2ADV_SND_H */
