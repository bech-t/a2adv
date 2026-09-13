/* snd.h -- contrat son commun aux players, un backend par plateforme.
 *
 * Apple II (cf. player/apple2/src/snd.c) : haut-parleur 1 bit ($C030) par
 * defaut, monophonique et bloquant (suffisant pour un livre-jeu au tour par
 * tour) ; Mockingboard optionnel (cf. player/apple2/src/snd_mb.c) pour la
 * musique de fond, au choix MANUEL du joueur (menu Options) -- rare sur //e,
 * jamais detecte automatiquement.
 *
 * Atari ST (cf. player/atarist/src/snd.c) : YM2149 (PSG interne, soude sur
 * la carte mere -- TOUJOURS present, rien a detecter ni choisir), compatible
 * registre avec l'AY-3-8910 du Mockingboard. L'interface reste identique
 * (main.c/smenu.c, communs, n'ont rien a savoir de la machine), mais
 * `snd_backend`/`snd_mb_slot` y perdent leur sens de "carte optionnelle" :
 * cf. le commentaire de tete de player/atarist/src/snd.c pour le detail. */
#ifndef A2ADV_SND_H
#define A2ADV_SND_H

#include "format.h"

/* Un ton carre : pitch = demi-periode (grand = grave), dur = nombre de
 * bascules. Sans objet sur un backend sans haut-parleur (ex. ST) -- gardee
 * pour le contrat avec le reste du moteur, ne fait alors rien. */
void snd_tone(u8 pitch, u16 dur);

/* Joue un effet predefini (SND_SELECT, SND_WIN, ...). */
void snd_play(u8 id);

/* Backend son actif : 0 = repli "silencieux/minimal" de la plateforme
 * (haut-parleur sur Apple II, silence sur ST), 1 = puce dediee active
 * (Mockingboard sur Apple II, YM2149 -- toujours present -- sur ST). */
extern u8 snd_backend;

/* Slot Mockingboard actif (1..7), ou 0 si backend 0. Sans objet sur ST
 * (aucune notion de slot), y reste toujours a 0. Informatif (menu Options). */
extern u8 snd_mb_slot;

/* Active le backend 1 (slot 1..7 cote Apple II -- n'importe lequel sur ST,
 * ou la notion de slot n'existe pas) ; 0 = repli. Pilote par le menu
 * Options : le choix est MANUEL, jamais de detection automatique au boot. */
void snd_use_mockingboard(u8 slot);

/* Musique de fond, reservee au backend 1. Reservee a la Mockingboard/YM2149 :
 * au haut-parleur 1 bit, une musique de fond monopoliserait le processeur et
 * figerait le jeu -- ces appels sont donc des no-op tant que le backend est
 * le repli, les appelants n'ont aucun test a faire. Non bloquante : avance
 * d'un cran par VBL via scr_idle_hook (cf. scr.h), appele depuis
 * scr_getkey() pendant l'attente clavier. */
#define MUS_NONE   0
#define MUS_TITLE  1

void snd_music(u8 id);   /* lance un morceau en boucle, ou l'arrete avec MUS_NONE */

#endif /* A2ADV_SND_H */
