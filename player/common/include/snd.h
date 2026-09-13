/* snd.h -- contrat son commun aux players : trois choses seulement,
 * chacune implementee par chaque plateforme selon ce qu'elle a (cf.
 * apple2/src/snd.c, haut-parleur seul ; atarist/src/snd.c, YM2149). Aucune
 * notion de backend, de carte ou de materiel n'est visible d'ici -- si une
 * plateforme n'a rien a jouer pour l'une des trois, sa fonction ne fait
 * rien, et le reste du moteur (main.c/smenu.c, communs) n'a aucun test a
 * faire pour le savoir. */
#ifndef A2ADV_SND_H
#define A2ADV_SND_H

#include "format.h"

/* Jingle de demarrage. Bloquant (~1 s) : occupe l'attente sur le dernier
 * splash pendant que le reste se charge (cf. main.c). No-op si la
 * plateforme n'a rien a jouer ici. */
void snd_intro(void);

/* Musique de menu, non bloquante : avance d'un cran par VBL via
 * scr_idle_hook (cf. scr.h), appele depuis scr_getkey() pendant l'attente
 * clavier. on=1 : lance/relance le theme (rappele a CHAQUE affichage du
 * menu, pas seulement la premiere fois -- cf. smenu.c) ; on=0 : coupe
 * (avant d'entrer dans l'aventure). No-op si la plateforme n'a pas de
 * musique de fond. */
void snd_menu_music(u8 on);

/* Joue un effet predefini (SND_SELECT, SND_WIN, ... -- cf. format.h). */
void snd_play(u8 id);

#endif /* A2ADV_SND_H */
