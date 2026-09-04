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
 * Pas de detection automatique au boot. La sonde (lecture du timer d'un 6522)
 * confond une Mockingboard avec n'importe quelle autre carte a 6522, et le
 * joueur sait mieux que nous ce qu'il a dans ses slots. */
void snd_use_mockingboard(u8 slot);

#endif /* A2ADV_SND_H */
