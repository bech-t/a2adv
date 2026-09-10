/* snd.h -- son Atari ST via le YM2149 (PSG interne, compatible registre avec
 * l'AY-3-8910 du Mockingboard Apple II -- cf. player/apple2/src/snd_mb.c,
 * la meilleure reference de conception disponible pour ce fichier : meme
 * modele de programmation par registres, seul l'acces materiel change).
 *
 * Contrairement au Mockingboard (carte optionnelle, deux AY-3-8910, sondee
 * par slot), le YM2149 est TOUJOURS present -- soude sur la carte mere du
 * ST, rien a detecter ni choisir. L'interface publique reste identique a
 * celle du player Apple II (main.c/smenu.c, partages, n'ont rien a savoir
 * de la machine), mais son sens est reinterprete :
 *   - snd_backend            : 0 = silence, 1 = YM2149 actif (plus un choix
 *                              de carte)
 *   - snd_mb_slot             : sans objet sur ST, reste a 0
 *   - snd_use_mockingboard(0) coupe le son ; un slot 1..7 (n'importe lequel)
 *     l'active -- smenu.c propose encore un choix de "slot" 1-7 a l'ecran
 *     Options, ce qui n'a pas de sens sur ST (aucun numero de slot ici) ;
 *     fonctionne tel quel mais l'intitule affiche resterait a revoir cote
 *     UI/traduction -- hors perimetre de ce fichier.
 *
 * Un seul chip = un seul generateur de bruit et un seul generateur
 * d'enveloppe pour les trois voies (meme limite que CHAQUE AY du
 * Mockingboard pris individuellement, qui n'en a pas plus). Repartition
 * retenue (cf. snd.c) : voies A/B = musique (deux voix, amplitude FIXE,
 * sans enveloppe) ; voie C = effets (seule a utiliser l'enveloppe
 * materielle) -- separation qui garantit qu'un effet ne peut jamais
 * perturber l'enveloppe ou le mixeur de la musique en cours. */
#ifndef A2ADV_SND_H
#define A2ADV_SND_H

#include "format.h"

/* Sans objet sur ST (pas de haut-parleur 1 bit a piloter) -- gardee pour le
 * contrat avec le reste du moteur, ne fait rien. */
void snd_tone(u8 pitch, u16 dur);

/* Joue un effet predefini (SND_SELECT, SND_WIN, ...) sur la voie C. */
void snd_play(u8 id);

extern u8 snd_backend;    /* 0 = silence, 1 = YM2149 actif */
extern u8 snd_mb_slot;    /* sans objet sur ST, toujours 0 */

/* slot == 0 : coupe le son. slot != 0 (1..7, n'importe lequel) : l'active.
 * Cf. note d'en-tete sur l'intitule "slot" herite du Mockingboard. */
void snd_use_mockingboard(u8 slot);

/* --- Musique de fond ----------------------------------------------------- */
#define MUS_NONE   0
#define MUS_TITLE  1

/* Lance un morceau en boucle sur les voies A/B, ou l'arrete avec MUS_NONE.
 * Non bloquant : avance d'un cran par VBL via scr_idle_hook (cf. scr.h),
 * appele depuis scr_getkey() pendant l'attente clavier -- meme mecanisme
 * que mb_music_tick cote Apple II. */
void snd_music(u8 id);

#endif /* A2ADV_SND_H */
